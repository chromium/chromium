// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

#include "base/barrier_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace password_manager {

namespace {

using sync_util::IsPasswordSyncEnabled;

// Threshold for the next migration attempt. This is needed in order to prevent
// clients from spamming GMS Core API.
constexpr base::TimeDelta kMigrationThreshold = base::Days(1);

// The required migration version. If the version saved in
// `prefs::kCurrentMigrationVersionToGoogleMobileServices` is lower than
// 'kRequiredMigrationVersion', passwords will be re-uploaded. Currently set to
// the initial migration version.
constexpr int kRequiredMigrationVersion = 1;

// Returns true if the initial migration to the android backend has happened.
// The pref is updated after any type of migration, but the initial migration
// happens first.
bool HasMigratedToTheAndroidBackend(PrefService* prefs) {
  return prefs->GetInteger(
             prefs::kCurrentMigrationVersionToGoogleMobileServices) >=
         kRequiredMigrationVersion;
}

bool IsBlacklistedFormWithValues(const PasswordForm& form) {
  return form.blocked_by_user &&
         (!form.username_value.empty() || !form.password_value.empty());
}

// Converts the type of migration into a string used for metrics recording.
std::string MigrationTypeToString(
    BuiltInBackendToAndroidBackendMigrator::MigrationType migration_type) {
  switch (migration_type) {
    case BuiltInBackendToAndroidBackendMigrator::MigrationType::
        kInitialForSyncUsers:
      return "InitialMigrationForSyncUsers";
    case BuiltInBackendToAndroidBackendMigrator::MigrationType::
        kNonSyncableToAndroidBackend:
      return "NonSyncableDataMigrationToAndroidBackend";
    case BuiltInBackendToAndroidBackendMigrator::MigrationType::
        kNonSyncableToBuiltInBackend:
      return "NonSyncableDataMigrationToBuiltInBackend";
    case BuiltInBackendToAndroidBackendMigrator::MigrationType::kForLocalUsers:
      return "MigrationForLocalUsers";
    case BuiltInBackendToAndroidBackendMigrator::MigrationType::
        kReenrollmentAttempt:
      return "ReenrollmentAttemptMigration";
    case BuiltInBackendToAndroidBackendMigrator::MigrationType::kNone:
      NOTREACHED() << "No migration should be executed.";
      return std::string();
  }
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
  const std::string metric_infix_;
};

BuiltInBackendToAndroidBackendMigrator::BuiltInBackendToAndroidBackendMigrator(
    PasswordStoreBackend* built_in_backend,
    PasswordStoreBackend* android_backend,
    PrefService* prefs)
    : built_in_backend_(built_in_backend),
      android_backend_(android_backend),
      prefs_(prefs) {
  DCHECK(built_in_backend_);
  DCHECK(android_backend_);
  base::UmaHistogramBoolean(
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone",
      HasMigratedToTheAndroidBackend(prefs_));
}

BuiltInBackendToAndroidBackendMigrator::
    ~BuiltInBackendToAndroidBackendMigrator() = default;

void BuiltInBackendToAndroidBackendMigrator::StartMigrationIfNecessary(
    bool should_attempt_upm_reenrollment) {
  DCHECK(features::RequiresMigrationForUnifiedPasswordManager());

  // Don't try to migrate passwords if there was an attempt earlier today.
  base::TimeDelta time_passed_since_last_migration_attempt =
      base::Time::Now() -
      base::Time::FromTimeT(prefs_->GetDouble(
          password_manager::prefs::kTimeOfLastMigrationAttempt));
  if (time_passed_since_last_migration_attempt < kMigrationThreshold)
    return;

  // Do not migrate if a migration is already running. By the time it ends, the
  // two backends will be identical, therefore the second migration won't be
  // needed.
  if (migration_in_progress_type_ != MigrationType::kNone)
    return;

  MigrationType migration_type =
      GetMigrationType(should_attempt_upm_reenrollment);
  if (migration_type != MigrationType::kNone)
    PrepareForMigration(migration_type);
}

void BuiltInBackendToAndroidBackendMigrator::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
}

base::WeakPtr<BuiltInBackendToAndroidBackendMigrator>
BuiltInBackendToAndroidBackendMigrator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref() {
  if (!HasMigratedToTheAndroidBackend(prefs_) &&
      IsPasswordSyncEnabled(sync_service_)) {
    // TODO(crbug.com/1302299): Drop the sync metadata and only then update
    // pref.
  }
  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                     kRequiredMigrationVersion);
}

BuiltInBackendToAndroidBackendMigrator::MigrationType
BuiltInBackendToAndroidBackendMigrator::GetMigrationType(
    bool should_attempt_upm_reenrollement) const {
  if (should_attempt_upm_reenrollement)
    return MigrationType::kReenrollmentAttempt;

  // Checks that pref and sync state indicate that the user needs an initial
  // migration to the android backend after enrolling into the UPM experiment.
  if (!HasMigratedToTheAndroidBackend(prefs_) &&
      IsPasswordSyncEnabled(sync_service_)) {
    return MigrationType::kInitialForSyncUsers;
  }

  // TODO(crbug.com/1445497): Re-evaluate migration code for local passwords.
  bool upm_for_local_active = base::FeatureList::IsEnabled(
      password_manager::features::kUnifiedPasswordManagerLocalPasswordsAndroid);

  // If the user enables or disables password sync, the new active backend needs
  // non-syncable data from the previously active backend, as logins are
  // already transmitted through sync.
  // Once the local storage is supported, android backend becomes the only
  // active backend and there is no need to do this migration.
  if (prefs_->GetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange) &&
      !upm_for_local_active) {
    return IsPasswordSyncEnabled(sync_service_)
               ? MigrationType::kNonSyncableToAndroidBackend
               : MigrationType::kNonSyncableToBuiltInBackend;
  }

  // Once the local storage is supported, the migration to ensure the
  // consistency of the two backends is needed.
  if (upm_for_local_active) {
    return MigrationType::kForLocalUsers;
  }

  // No other migration should be executed.
  return MigrationType::kNone;
}

void BuiltInBackendToAndroidBackendMigrator::PrepareForMigration(
    MigrationType migration_type) {
  prefs_->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                    base::Time::Now().ToDoubleT());
  migration_in_progress_type_ = migration_type;

  metrics_reporter_ = std::make_unique<MigrationMetricsReporter>(
      MigrationTypeToString(migration_type));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords",
                                    "UnifiedPasswordManagerMigration", this);

  if (migration_type == MigrationType::kInitialForSyncUsers ||
      migration_type == MigrationType::kNonSyncableToAndroidBackend ||
      migration_type == MigrationType::kReenrollmentAttempt) {
    // Sync is enabled. The synced passwords in the two backend should be
    // identical. Non-syncable data is not synced and needs to be migrated to
    // the android backend after enrolling into the experiment or after a sync
    // status change.
    // Update calls won't fail because they would add a password in the rare
    // case that it doesn't exist in the target backend.
    // During the migration username and password values are also cleaned up
    // from the blacklisted entries stored in the built in backend.
    auto callback_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::MigrateNonSyncableData,
        weak_ptr_factory_.GetWeakPtr(), android_backend_);
    callback_chain = base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                                        RemoveBlacklistedFormsWithValues,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::Unretained(built_in_backend_),
                                    std::move(callback_chain));
    built_in_backend_->GetAllLoginsAsync(std::move(callback_chain));
    return;
  }

  if (migration_type == MigrationType::kNonSyncableToBuiltInBackend) {
    // Sync was disabled, while the local GMS storage is not supported.
    // Migrate non-syncable data that is associated with a previously
    // synced account from the android backend to the built-in backend.
    android_backend_->GetAllLoginsForAccountAsync(
        prefs_->GetString(::prefs::kGoogleServicesLastUsername),
        base::BindOnce(
            &BuiltInBackendToAndroidBackendMigrator::MigrateNonSyncableData,
            weak_ptr_factory_.GetWeakPtr(), built_in_backend_));
    return;
  }

  if (migration_type == MigrationType::kForLocalUsers)
    RunMigrationForLocalUsers();
}

void BuiltInBackendToAndroidBackendMigrator::MigrateNonSyncableData(
    PasswordStoreBackend* target_backend,
    LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
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

  // All credentials are processed, because it's not possible to filter
  // only those that have non-syncable data.
  for (const auto& login : absl::get<LoginsResult>(logins_or_error)) {
    callbacks_chain = base::BindOnce(
        &BuiltInBackendToAndroidBackendMigrator::UpdateLoginInBackend,
        weak_ptr_factory_.GetWeakPtr(), target_backend, *login,
        std::move(callbacks_chain));
  }

  std::move(callbacks_chain).Run();
}

void BuiltInBackendToAndroidBackendMigrator::RunMigrationForLocalUsers() {
  auto barrier_callback = base::BarrierCallback<BackendAndLoginsResults>(
      2, base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                            MigratePasswordsBetweenAndroidAndBuiltInBackends,
                        weak_ptr_factory_.GetWeakPtr()));

  auto bind_backend_to_logins = [](PasswordStoreBackend* backend,
                                   LoginsResultOrError result) {
    return BackendAndLoginsResults(backend, std::move(result));
  };

  auto builtin_backend_callback_chain =
      base::BindOnce(bind_backend_to_logins,
                     base::Unretained(built_in_backend_))
          .Then(barrier_callback);

  // Cleanup blacklisted forms in the built in backend before binding.
  builtin_backend_callback_chain = base::BindOnce(
      &BuiltInBackendToAndroidBackendMigrator::RemoveBlacklistedFormsWithValues,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(built_in_backend_),
      std::move(builtin_backend_callback_chain));

  built_in_backend_->GetAllLoginsAsync(
      std::move(builtin_backend_callback_chain));

  auto android_backend_callback_chain =
      base::BindOnce(bind_backend_to_logins, base::Unretained(android_backend_))
          .Then(barrier_callback);

  // Cleanup blacklisted forms in the android backend before binding.
  android_backend_callback_chain = base::BindOnce(
      &BuiltInBackendToAndroidBackendMigrator::RemoveBlacklistedFormsWithValues,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(android_backend_),
      std::move(android_backend_callback_chain));

  android_backend_->GetAllLoginsAsync(
      std::move(android_backend_callback_chain));
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

  if (!HasMigratedToTheAndroidBackend(prefs_)) {
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
  //    to the |built_in_backend_|.
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
    PasswordChangesOrError changes_or_error) {
  PasswordChanges* changes = absl::get_if<PasswordChanges>(&changes_or_error);
  if (absl::holds_alternative<PasswordStoreBackendError>(changes_or_error)) {
    MigrationFinished(/*is_success=*/false);
    return;
  }

  // Nullopt changelist is returned on success by the backends that do not
  // provide exact changelist (e.g. Android). This indicates success operation
  // as well as non-empty changelist.
  if (!changes->has_value() || !changes->value().empty()) {
    // The step was successful, continue the migration.
    std::move(callback).Run();
    return;
  }
  // Migration failed (changelist is present but empty).
  MigrationFinished(/*is_success=*/false);
}

void BuiltInBackendToAndroidBackendMigrator::MigrationFinished(
    bool is_success) {
  DCHECK(metrics_reporter_);
  metrics_reporter_->ReportMetrics(is_success);
  metrics_reporter_.reset();

  // Reenroll previously evicted user into the experiment if the migration has
  // succeeded.
  if (migration_in_progress_type_ == MigrationType::kReenrollmentAttempt &&
      is_success) {
    prefs_->ClearPref(password_manager::prefs::
                          kUnenrolledFromGoogleMobileServicesDueToErrors);
    prefs_->ClearPref(password_manager::prefs::
                          kUnenrolledFromGoogleMobileServicesAfterApiErrorCode);
    prefs_->SetInteger(
        prefs::kTimesReenrolledToGoogleMobileServices,
        prefs_->GetInteger(prefs::kTimesReenrolledToGoogleMobileServices) + 1);
    prefs_->SetInteger(prefs::kTimesAttemptedToReenrollToGoogleMobileServices,
                       0);
  }

  migration_in_progress_type_ = MigrationType::kNone;
  prefs_->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, false);
  TRACE_EVENT_NESTABLE_ASYNC_END0("passwords",
                                  "UnifiedPasswordManagerMigration", this);
}

void BuiltInBackendToAndroidBackendMigrator::RemoveBlacklistedFormsWithValues(
    PasswordStoreBackend* backend,
    LoginsOrErrorReply result_callback,
    LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    std::move(result_callback).Run(std::move(logins_or_error));
    return;
  }

  auto& forms = absl::get<LoginsResult>(logins_or_error);

  LoginsResult clean_forms;
  clean_forms.reserve(forms.size());

  for (std::unique_ptr<PasswordForm>& form : forms) {
    if (IsBlacklistedFormWithValues(*form)) {
      RemoveLoginFromBackend(backend, *form, base::DoNothing());
      continue;
    }
    clean_forms.push_back(std::move(form));
  }

  std::move(result_callback).Run(std::move(clean_forms));
}

}  // namespace password_manager
