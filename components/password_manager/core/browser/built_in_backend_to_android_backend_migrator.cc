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
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

constexpr base::TimeDelta kMigrationThreshold = base::Days(1);

struct IsPasswordLess {
  bool operator()(const PasswordForm* lhs, const PasswordForm* rhs) const {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  }
};

base::OnceCallback<void(PasswordStoreChangeList)>
IgnoreChangeListAndRunCallback(base::OnceClosure callback) {
  return base::BindOnce(
      [](base::OnceClosure callback, PasswordStoreChangeList) {
        std::move(callback).Run();
      },
      std::move(callback));
}

bool IsInitialMigrationNeeded(PrefService* prefs) {
  return features::kMigrationVersion.Get() >
         prefs->GetInteger(
             prefs::kCurrentMigrationVersionToGoogleMobileServices);
}

}  // namespace

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

BuiltInBackendToAndroidBackendMigrator::BuiltInBackendToAndroidBackendMigrator(
    PasswordStoreBackend* built_in_backend,
    PasswordStoreBackend* android_backend,
    PrefService* prefs,
    base::RepeatingCallback<bool()> is_syncing_passwords_callback)
    : built_in_backend_(built_in_backend),
      android_backend_(android_backend),
      prefs_(prefs),
      is_syncing_passwords_callback_(std::move(is_syncing_passwords_callback)) {
  DCHECK(built_in_backend_);
  DCHECK(android_backend_);
}

BuiltInBackendToAndroidBackendMigrator::
    ~BuiltInBackendToAndroidBackendMigrator() = default;

void BuiltInBackendToAndroidBackendMigrator::StartMigrationIfNecessary() {
  // For syncing users, we don't need to move passwords between the built-in
  // and the Android backends, since both backends should be able to
  // retrieve the same passwords from the sync server.
  if (is_syncing_passwords_callback_.Run() &&
      IsInitialMigrationNeeded(prefs_)) {
    // TODO:(crbug.com/1252443) Drop metadata and only then update pref.
    UpdateMigrationVersionInPref();
    return;
  }

  // Don't try to migrate passwords if there was an attempt earlier today.
  base::TimeDelta time_passed_since_last_migration_attempt =
      base::Time::Now() -
      base::Time::FromTimeT(prefs_->GetDouble(
          password_manager::prefs::kTimeOfLastMigrationAttempt));
  if (time_passed_since_last_migration_attempt < kMigrationThreshold)
    return;

  // Manually migrate passwords between backends if initial or rolling migration
  // is needed. Even for syncing users we still should do rolling migration to
  // ensure deletions aren’t resurrected.
  if (IsInitialMigrationNeeded(prefs_) ||
      base::FeatureList::IsEnabled(features::kUnifiedPasswordManagerAndroid)) {
    PrepareForMigration();
  }
}

void BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref() {
  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                     features::kMigrationVersion.Get());
}

void BuiltInBackendToAndroidBackendMigrator::PrepareForMigration() {
  prefs_->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                    base::Time::Now().ToDoubleT());

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
    MigratePasswordsBetweenAndroidAndBuiltInBackends(
        std::vector<BackendAndLoginsResults> results) {
  DCHECK_EQ(2u, results.size());
  // TODO:(crbug.com/1252443) Record that migration was canceled due to an
  // error.
  if (results[0].HasError() || results[1].HasError())
    return;

  base::flat_set<const PasswordForm*, IsPasswordLess> built_in_backend_logins =
      (results[0].backend == built_in_backend_) ? results[0].GetLogins()
                                                : results[1].GetLogins();

  base::flat_set<const PasswordForm*, IsPasswordLess> android_logins =
      (results[0].backend == android_backend_) ? results[0].GetLogins()
                                               : results[1].GetLogins();

  bool is_initial_migration = IsInitialMigrationNeeded(prefs_);

  // For a form |F|, there are three cases to handle:
  // 1. |F| exists only in the |built_in_backend_|
  // 2. |F| exists only in the |android_backend_|
  // 3. |F| exists in both |built_in_backend_| and |android_backend_|.
  //
  // In initial migration is required:
  // 1. |F| should be added to the |android_backend_|.
  // 2. |F| should be added to the |built_in_backend_|.
  // 3. Both versions should be merged by accepting the most recently created
  //    one*, and update either |built_in_backend_| and |android_backend_|
  //    accordingly.
  //    * it should happen only if password values differs between backends.
  // Otherwise:
  // 1. |F| should be removed from the |built_in_backend_|.
  // 2. |F| should be added to the |built_in_backend_|.
  // 3. version from |built_in_backend_| should be updated with version from the
  // |android_backend_|.

  // Callbacks are chained in a LIFO way by passing 'callback_chain' as a
  // completion for the next operation. If it is initial migration - update pref
  // to mark successful completion.
  base::OnceClosure callbacks_chain =
      is_initial_migration
          ? base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                               UpdateMigrationVersionInPref,
                           weak_ptr_factory_.GetWeakPtr())
          : base::DoNothing();
  for (auto* const login : built_in_backend_logins) {
    auto android_login_iter = android_logins.find(login);

    if (android_login_iter == android_logins.end()) {
      // Password from the |built_in_backend_| doesn't exist in the
      // |android_backend_|.
      if (is_initial_migration) {
        callbacks_chain = base::BindOnce(
            &PasswordStoreBackend::AddLoginAsync,
            android_backend_->GetWeakPtr(), *login,
            IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
      } else {
        callbacks_chain = base::BindOnce(
            &PasswordStoreBackend::RemoveLoginAsync,
            built_in_backend_->GetWeakPtr(), *login,
            IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
      }

      continue;
    }

    // Password from the |built_in_backend_| exists in the |android_backend_|.
    auto* const android_login = (*android_login_iter);

    if (login->password_value == android_login->password_value) {
      // Passwords are identical, nothing else to do.
      continue;
    }

    // Passwords aren't identical.
    if (is_initial_migration &&
        login->date_created > android_login->date_created) {
      // // During initial migration, pick the most recently created one. This
      // is aligned with the merge sync logic in PasswordSyncBridge.
      callbacks_chain = base::BindOnce(
          &PasswordStoreBackend::UpdateLoginAsync,
          android_backend_->GetWeakPtr(), *login,
          IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
    } else {
      // During rolling migration, update the built-in version to match the
      // Android version.
      callbacks_chain = base::BindOnce(
          &PasswordStoreBackend::UpdateLoginAsync,
          built_in_backend_->GetWeakPtr(), *android_login,
          IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
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
        &PasswordStoreBackend::AddLoginAsync, built_in_backend_->GetWeakPtr(),
        *android_login,
        IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
  }

  std::move(callbacks_chain).Run();
}

}  // namespace password_manager
