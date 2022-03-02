// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"
#include "base/memory/raw_ptr.h"

#include "base/barrier_callback.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

// Threshold for the next migration attempt. This is needed in order to prevent
// clients from spamming GMS Core API.
constexpr base::TimeDelta kMigrationThreshold = base::Days(1);

bool IsInitialMigrationNeeded(PrefService* prefs) {
  return features::kMigrationVersion.Get() >
         prefs->GetInteger(
             prefs::kCurrentMigrationVersionToGoogleMobileServices);
}

}  // namespace

struct BuiltInBackendToAndroidBackendMigrator::IsPasswordLess {
  bool operator()(const PasswordForm* lhs, const PasswordForm* rhs) const {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  }
};

struct BuiltInBackendToAndroidBackendMigrator::BackendAndLoginsResults {
  raw_ptr<PasswordStoreBackend> backend;
  LoginsResultOrError logins_result;

  bool HasError() {
    return absl::holds_alternative<PasswordStoreBackendError>(logins_result);
  }

  // Converts std::vector<std::unique_ptr<PasswordForms>> into
  // base::flat_set<const PasswordForm*> for quick look up comparing only
  // primary keys.
  base::flat_set<const PasswordForm*, IsPasswordLess> GetLogins() {
    DCHECK(!HasError());

    return base::MakeFlatSet<const PasswordForm*, IsPasswordLess>(
        absl::get<LoginsResult>(logins_result), {},
        &std::unique_ptr<PasswordForm>::get);
  }

  BackendAndLoginsResults(PasswordStoreBackend* backend,
                          LoginsResultOrError logins)
      : backend(backend), logins_result(std::move(logins)) {}
  BackendAndLoginsResults(BackendAndLoginsResults&&) = default;
  BackendAndLoginsResults& operator=(BackendAndLoginsResults&&) = default;
  BackendAndLoginsResults(const BackendAndLoginsResults&) = delete;
  BackendAndLoginsResults& operator=(const BackendAndLoginsResults&) = delete;
  ~BackendAndLoginsResults() = default;
};

class BuiltInBackendToAndroidBackendMigrator::MigrationMetricsReporter {
 public:
  explicit MigrationMetricsReporter(base::StringPiece metric_infix)
      : metric_infix_(metric_infix) {}
  ~MigrationMetricsReporter() = default;

  void ReportMetrics(bool migration_succeeded) {
    auto BuildMetricName = [this](base::StringPiece suffix) {
      return base::StrCat({"PasswordManager.UnifiedPasswordManager.",
                           metric_infix_, ".", suffix});
    };
    base::TimeDelta duration = base::Time::Now() - start_;
    base::UmaHistogramMediumTimes(BuildMetricName("Latency"), duration);
    base::UmaHistogramBoolean(BuildMetricName("Success"), migration_succeeded);
  }

 private:
  base::Time start_ = base::Time::Now();
  base::StringPiece metric_infix_;
};

BuiltInBackendToAndroidBackendMigrator::BuiltInBackendToAndroidBackendMigrator(
    PasswordStoreBackend* built_in_backend,
    PasswordStoreBackend* android_backend,
    PrefService* prefs,
    PasswordStoreBackend::SyncDelegate* sync_delegate)
    : built_in_backend_(built_in_backend),
      android_backend_(android_backend),
      prefs_(prefs),
      sync_delegate_(sync_delegate) {
  DCHECK(built_in_backend_);
  DCHECK(android_backend_);
  base::UmaHistogramBoolean(
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone",
      !IsInitialMigrationNeeded(prefs_));
}

BuiltInBackendToAndroidBackendMigrator::
    ~BuiltInBackendToAndroidBackendMigrator() = default;

void BuiltInBackendToAndroidBackendMigrator::StartMigrationIfNecessary() {
  // Don't try to migrate passwords if there was an attempt earlier today.
  base::TimeDelta time_passed_since_last_migration_attempt =
      base::Time::Now() -
      base::Time::FromTimeT(prefs_->GetDouble(
          password_manager::prefs::kTimeOfLastMigrationAttempt));
  if (time_passed_since_last_migration_attempt < kMigrationThreshold)
    return;

  bool is_initial_migration_needed = IsInitialMigrationNeeded(prefs_);
  // Manually migrate passwords between backends if initial or rolling migration
  // is needed. Even for syncing users we still should do the initial migration
  // to move local-only data that aren't synced to GMSCore and do the rolling
  // migration to ensure deletions aren’t resurrected.
  if (is_initial_migration_needed ||
      base::FeatureList::IsEnabled(features::kUnifiedPasswordManagerAndroid)) {
    metrics_reporter_ = std::make_unique<MigrationMetricsReporter>(
        is_initial_migration_needed ? "InitialMigration" : "RollingMigration");
    PrepareForMigration();
  }
}

void BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref() {
  if (IsInitialMigrationNeeded(prefs_) &&
      sync_delegate_->IsSyncingPasswordsEnabled()) {
    // TODO(crbug.com/1302299): Drop metadata and only then update pref.
  }
  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                     features::kMigrationVersion.Get());
}

void BuiltInBackendToAndroidBackendMigrator::PrepareForMigration() {
  prefs_->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                    base::Time::Now().ToDoubleT());

  // For sync users, migrate local-only data from the built-in backend to the
  // Android backend. The synced passwords should otherwise be identical. Update
  // calls don't fail because they would add a password in the rare case that it
  // doesn't exist in the Android backend.
  if (IsInitialMigrationNeeded(prefs_) &&
      sync_delegate_->IsSyncingPasswordsEnabled()) {
    built_in_backend_->GetAllLoginsAsync(
        base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                           MigrateNonSyncableDataToAndroidBackend,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  auto barrier_callback = base::BarrierCallback<BackendAndLoginsResults>(
      2, base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                            MigratePasswordsBetweenAndroidAndBuiltInBackends,
                        weak_ptr_factory_.GetWeakPtr()));

  auto bind_backend_to_logins = [](PasswordStoreBackend* backend,
                                   LoginsResultOrError result) {
    return BackendAndLoginsResults(backend, std::move(result));
  };

  built_in_backend_->GetAllLoginsAsync(
      base::BindOnce(bind_backend_to_logins,
                     base::Unretained(built_in_backend_))
          .Then(barrier_callback));
  android_backend_->GetAllLoginsAsync(
      base::BindOnce(bind_backend_to_logins, base::Unretained(android_backend_))
          .Then(barrier_callback));
}

void BuiltInBackendToAndroidBackendMigrator::
    MigrateNonSyncableDataToAndroidBackend(
        LoginsResultOrError built_in_backend_logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(
          built_in_backend_logins_or_error)) {
    MigrationFinished(/*is_success=*/false);
    return;
  }

  // Like a stack, callbacks are chained by  by passing 'callback_chain' as a
  // completion for the next operation. At the end, update pref to mark
  // successful completion.
  base::OnceClosure callbacks_chain =
      base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::MigrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), /*is_success=*/true);

  callbacks_chain =
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref,
          weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callbacks_chain));

  for (const auto& login :
       absl::get<LoginsResult>(built_in_backend_logins_or_error)) {
    callbacks_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend,
        weak_ptr_factory_.GetWeakPtr(), android_backend_, *login,
        std::move(callbacks_chain));
  }
  std::move(callbacks_chain).Run();
}

void BuiltInBackendToAndroidBackendMigrator::
    MigratePasswordsBetweenAndroidAndBuiltInBackends(
        std::vector<BackendAndLoginsResults> results) {
  DCHECK(metrics_reporter_);
  DCHECK_EQ(2u, results.size());

  if (results[0].HasError() || results[1].HasError()) {
    MigrationFinished(/*is_success=*/false);
    return;
  }

  base::flat_set<const PasswordForm*, IsPasswordLess> built_in_backend_logins =
      (results[0].backend == built_in_backend_) ? results[0].GetLogins()
                                                : results[1].GetLogins();

  base::flat_set<const PasswordForm*, IsPasswordLess> android_logins =
      (results[0].backend == android_backend_) ? results[0].GetLogins()
                                               : results[1].GetLogins();

  if (IsInitialMigrationNeeded(prefs_)) {
    MergeAndroidBackendAndBuiltInBackend(std::move(built_in_backend_logins),
                                         std::move(android_logins));
  } else {
    MirrorAndroidBackendToBuiltInBackend(std::move(built_in_backend_logins),
                                         std::move(android_logins));
  }
}

void BuiltInBackendToAndroidBackendMigrator::
    MergeAndroidBackendAndBuiltInBackend(
        PasswordFormPtrFlatSet built_in_backend_logins,
        PasswordFormPtrFlatSet android_logins) {
  // For a form |F|, there are three cases to handle:
  // 1. If |F| exists only in the |built_in_backend_|, then |F| should be added
  //    to the |android_backend_|.
  // 2. If |F| exists only in the |android_backend_|, then |F| should be added
  // to
  //    the |built_in_backend_|
  // 3. If |F| exists in both |built_in_backend_| and |android_backend_|, then
  //    both versions should be merged by accepting the most recently created
  //    one*, and update either |built_in_backend_| and |android_backend_|
  //    accordingly.
  //    * it should happen only if password values differs between backends.

  // Callbacks are chained like in a stack way by passing 'callback_chain' as a
  // completion for the next operation. At the end, update pref to mark
  // successful completion.
  base::OnceClosure callbacks_chain =
      base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::MigrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), /*is_success=*/true);

  callbacks_chain =
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref,
          weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callbacks_chain));

  for (auto* const login : built_in_backend_logins) {
    auto android_login_iter = android_logins.find(login);

    if (android_login_iter == android_logins.end()) {
      // Password from the |built_in_backend_| doesn't exist in the
      // |android_backend_|.
      callbacks_chain = base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::AddLoginToBackend,
          weak_ptr_factory_.GetWeakPtr(), android_backend_, *login,
          std::move(callbacks_chain));

      continue;
    }

    // Password from the |built_in_backend_| exists in the |android_backend_|.
    auto* const android_login = (*android_login_iter);

    if (login->password_value == android_login->password_value) {
      // Passwords are identical, nothing else to do.
      continue;
    }

    // Passwords aren't identical. Pick the most recently created one. This
    // is aligned with the merge sync logic in PasswordSyncBridge.
    if (login->date_created > android_login->date_created) {
      callbacks_chain = base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend,
          weak_ptr_factory_.GetWeakPtr(), android_backend_, *login,
          std::move(callbacks_chain));
    } else {
      callbacks_chain = base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend,
          weak_ptr_factory_.GetWeakPtr(), built_in_backend_, *android_login,
          std::move(callbacks_chain));
    }
  }

  // At this point, we have processed all passwords from the |built_in_backend_|
  // In addition, we also have processed all passwords from the
  // |android_backend_| which exist in the |built_in_backend_|. What's remaining
  // is to process passwords from |android_backend_| that don't exist in the
  // |built_in_backend_|.
  for (auto* const android_login : android_logins) {
    if (built_in_backend_logins.contains(android_login)) {
      continue;
    }

    // Add to the |built_in_backend_| any passwords from the |android_backend_|
    // that doesn't exist in the |built_in_backend_|.
    callbacks_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::AddLoginToBackend,
        weak_ptr_factory_.GetWeakPtr(), built_in_backend_, *android_login,
        std::move(callbacks_chain));
  }

  std::move(callbacks_chain).Run();
}

void BuiltInBackendToAndroidBackendMigrator::
    MirrorAndroidBackendToBuiltInBackend(
        PasswordFormPtrFlatSet built_in_backend_logins,
        PasswordFormPtrFlatSet android_logins) {
  // For a form |F|, there are three cases to handle:
  // 1. If |F| exists only in the |built_in_backend_|, then |F| should be
  // removed from the |built_in_backend_|.
  // 2. |F| exists only in the |android_backend_|, then |F| should be added to
  // the |built_in_backend_|.
  // 3. |F| exists in both |built_in_backend_| and |android_backend_|, then
  // version from |built_in_backend_| should be updated with version from the
  // |android_backend_|.

  // Callbacks are chained like in a stack way by passing 'callback_chain' as a
  // completion for the next operation.
  base::OnceClosure callbacks_chain =
      base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::MigrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), /*is_success=*/true);

  for (auto* const login : built_in_backend_logins) {
    auto android_login_iter = android_logins.find(login);

    if (android_login_iter == android_logins.end()) {
      // Password from the |built_in_backend_| doesn't exist in the
      // |android_backend_|.
      callbacks_chain = base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RemoveLoginFromBackend,
          weak_ptr_factory_.GetWeakPtr(), built_in_backend_, *login,
          std::move(callbacks_chain));
      continue;
    }

    // Password from the |built_in_backend_| exists in the |android_backend_|.
    auto* const android_login = (*android_login_iter);

    if (login->password_value == android_login->password_value) {
      // Passwords are identical, nothing else to do.
      continue;
    }

    // Passwords aren't identical, update the built-in version to match the
    // Android version.
    callbacks_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend,
        weak_ptr_factory_.GetWeakPtr(), built_in_backend_, *android_login,
        std::move(callbacks_chain));
  }

  // At this point, we have processed all passwords from the |built_in_backend_|
  // In addition, we also have processed all passwords from the
  // |android_backend_| which exist in the |built_in_backend_|. What's remaining
  // is to process passwords from |android_backend_| that don't exist in the
  // |built_in_backend_|.
  for (auto* const android_login : android_logins) {
    if (built_in_backend_logins.contains(android_login)) {
      continue;
    }

    // Add to the |built_in_backend_| any passwords from the |android_backend_|
    // that doesn't exist in the |built_in_backend_|.
    callbacks_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::AddLoginToBackend,
        weak_ptr_factory_.GetWeakPtr(), built_in_backend_, *android_login,
        std::move(callbacks_chain));
  }

  std::move(callbacks_chain).Run();
}

void BuiltInBackendToAndroidBackendMigrator::AddLoginToBackend(
    PasswordStoreBackend* backend,
    const PasswordForm& form,
    base::OnceClosure callback) {
  backend->AddLoginAsync(
      form,
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend(
    PasswordStoreBackend* backend,
    const PasswordForm& form,
    base::OnceClosure callback) {
  backend->UpdateLoginAsync(
      form,
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BuiltInBackendToAndroidBackendMigrator::RemoveLoginFromBackend(
    PasswordStoreBackend* backend,
    const PasswordForm& form,
    base::OnceClosure callback) {
  backend->RemoveLoginAsync(
      form,
      base::BindOnce(
          &BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BuiltInBackendToAndroidBackendMigrator::RunCallbackOrAbortMigration(
    base::OnceClosure callback,
    absl::optional<PasswordStoreChangeList> changelist) {
  if (!changelist.has_value() || !changelist.value().empty()) {
    // The step was successful, continue the migration.
    std::move(callback).Run();
    return;
  }
  // Migration failed.
  MigrationFinished(/*is_success=*/false);
}

void BuiltInBackendToAndroidBackendMigrator::MigrationFinished(
    bool is_success) {
  DCHECK(metrics_reporter_);
  metrics_reporter_->ReportMetrics(is_success);
  metrics_reporter_.reset();
}

}  // namespace password_manager
