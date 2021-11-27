// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"
#include "base/memory/raw_ptr.h"

#include "base/barrier_callback.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

struct IsLess {
  bool operator()(const PasswordForm* lhs, const PasswordForm* rhs) const {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  }
};

base::OnceCallback<void(const PasswordStoreChangeList&)>
IgnoreChangeListAndRunCallback(base::OnceClosure callback) {
  return base::BindOnce(
      [](base::OnceClosure callback, const PasswordStoreChangeList&) {
        std::move(callback).Run();
      },
      std::move(callback));
}

}  // namespace

struct BuiltInBackendToAndroidBackendMigrator::BackendAndLoginsResults {
  raw_ptr<PasswordStoreBackend> backend;
  LoginsResultOrError logins_result;

  bool HasError() {
    return absl::holds_alternative<PasswordStoreBackendError>(logins_result);
  }

  base::flat_set<const PasswordForm*, IsLess> GetLogins() {
    DCHECK(!HasError());

    return base::MakeFlatSet<const PasswordForm*, IsLess>(
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
  bool is_initial_migration_needed =
      features::kMigrationVersion.Get() >
      prefs_->GetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices);

  // For syncing users, we don't need to move passwords between the built-in
  // and the Android backends, since both backends should be able to
  // retrieve the same passwords from the sync server.
  if (is_syncing_passwords_callback_.Run()) {
    if (is_initial_migration_needed) {
      // TODO:(crbug.com/1252443) Drop metadata and only then update pref.
      UpdateMigrationVersionInPref();
    }
    return;
  }

  // For non-syncing user migrate password from |built_in_backend_| to
  // |android_backend_|.
  if (is_initial_migration_needed) {
    PrepareForMigration();
  } else {
    // TODO:(crbug.com/1252443) Start rolling migration.
  }
}

void BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref() {
  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                     features::kMigrationVersion.Get());
}

void BuiltInBackendToAndroidBackendMigrator::PrepareForMigration() {
  auto barrier_callback = base::BarrierCallback<BackendAndLoginsResults>(
      2, base::BindOnce(&BuiltInBackendToAndroidBackendMigrator::
                            StartBuiltInToAndroidBackendMigration,
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
    StartBuiltInToAndroidBackendMigration(
        std::vector<BackendAndLoginsResults> results) {
  DCHECK_EQ(2u, results.size());
  // TODO:(crbug.com/1252443) Record that migration was canceled due to an
  // error.
  if (results[0].HasError() || results[1].HasError())
    return;

  auto built_in_backend_logins = (results[0].backend == built_in_backend_)
                                     ? results[0].GetLogins()
                                     : results[1].GetLogins();

  auto android_logins = (results[0].backend == android_backend_)
                            ? results[0].GetLogins()
                            : results[1].GetLogins();

  // This method merges password from the built in backend and password from the
  // android backend based on their primary keys. For a form |F|, there are
  // three cases to handle:
  // 1. |F| exists only in the built in backend --> |F| should be added to the
  // 'android_backend'.
  // 2. |F| exists only in the android backend --> |F| should be added to the
  // 'built_in_backend'.
  // 3. |F| exists in both the built in and android backends --> both versions
  //    should be merged by accepting the most recently created one, and update
  //    built in and android backends accordingly.
  // In most of the cases there shouldn't be any passwords in 'android_backend'.

  // After all operations are finished we should update preference to mark the
  // completion of the migration. Callbacks are chained in a LIFO way. Callbacks
  // are chained by passing 'callback_chain' as a completion for the next
  // operation.
  base::OnceClosure callbacks_chain = base::BindOnce(
      &BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref,
      weak_ptr_factory_.GetWeakPtr());
  for (auto* const login : built_in_backend_logins) {
    auto android_login_iter = android_logins.find(login);

    if (android_login_iter == android_logins.end()) {
      // Local password doesn't exist in the android backend, add it to the
      // 'android_backend_'.
      callbacks_chain = base::BindOnce(
          &PasswordStoreBackend::AddLoginAsync, android_backend_->GetWeakPtr(),
          *login, IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
      continue;
    }

    // Local password exists in the android backend as well. A merge is
    // required.
    auto* const android_login = (*android_login_iter);

    if (login->password_value == android_login->password_value) {
      // Passwords are identical, nothing else to do.
      continue;
    }

    // Passwords aren't identical, pick the most recently created one.
    if (login->date_created > android_login->date_created) {
      callbacks_chain = base::BindOnce(
          &PasswordStoreBackend::AddLoginAsync, android_backend_->GetWeakPtr(),
          *login, IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
    } else {
      callbacks_chain = base::BindOnce(
          &PasswordStoreBackend::AddLoginAsync, built_in_backend_->GetWeakPtr(),
          *android_login,
          IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
    }
  }

  // At this point, we have processed all passwords from the built in backend.
  // In addition, we also have processed all passwords from 'android_backend_'
  // that exist in the 'built_in_backend_'. What's remaining is to process
  // passwords from 'android_backend_' that don't exist in the
  // 'built_in_backend_'.
  for (auto* const android_login : android_logins) {
    if (built_in_backend_logins.contains(android_login)) {
      continue;
    }

    // Add to 'built_in_backend_' any passwords from 'android_backend_' that
    // doesn't exist in the 'built_in_backend_'
    callbacks_chain = base::BindOnce(
        &PasswordStoreBackend::AddLoginAsync, built_in_backend_->GetWeakPtr(),
        *android_login,
        IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
  }

  std::move(callbacks_chain).Run();
}

}  // namespace password_manager
