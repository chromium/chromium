// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

class AffiliatedMatchHelper;
class SmartBubbleStatsStore;

using LoginsReply = base::OnceCallback<void(LoginsResult)>;
using PasswordChangesOrErrorReply =
    base::OnceCallback<void(PasswordChangesOrError)>;
using LoginsOrErrorReply = base::OnceCallback<void(LoginsResultOrError)>;

// The backend is used by the `PasswordStore` to interact with the storage in a
// platform-dependent way (e.g. on Desktop, it calls a local database while on
// Android, it sends requests to a service).
// All methods are required to do their work asynchronously to prevent expensive
// IO operation from possibly blocking the main thread.
class PasswordStoreBackend {
 public:
  using RemoteChangesReceived =
      base::RepeatingCallback<void(std::optional<PasswordStoreChangeList>)>;

  PasswordStoreBackend() = default;
  PasswordStoreBackend(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend(PasswordStoreBackend&&) = delete;
  PasswordStoreBackend& operator=(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend& operator=(PasswordStoreBackend&&) = delete;
  virtual ~PasswordStoreBackend() = default;

  // TODO(crbug.bom/1226042): Rename this to Init after PasswordStoreImpl no
  // longer inherits PasswordStore.
  virtual void InitBackend(AffiliatedMatchHelper* affiliated_match_helper,
                           RemoteChangesReceived remote_form_changes_received,
                           base::RepeatingClosure sync_enabled_or_disabled_cb,
                           base::OnceCallback<void(bool)> completion) = 0;

  // Shuts down the store asynchronously. The callback is run on the main thread
  // after the shutdown has concluded and it is safe to delete the backend.
  // Please invalidate the weak pointers whenever defining this method.
  // Otherwise, some prefs might be set after the backend is shut down, leading
  // to a crash.
  virtual void Shutdown(base::OnceClosure shutdown_completed) = 0;

  // Necessary condition to offer saving passwords.
  virtual bool IsAbleToSavePasswords() = 0;

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status). Callback is called on the main sequence.
  virtual void GetAllLoginsAsync(LoginsOrErrorReply callback) = 0;

  // Returns the complete list of PasswordForms and fills in affiliation and
  // branding information for Android credentials. Callback is called on the
  // main sequence.
  virtual void GetAllLoginsWithAffiliationAndBrandingAsync(
      LoginsOrErrorReply callback) = 0;

  // Returns the complete list of non-blocklist PasswordForms. Callback is
  // called on the main sequence.
  virtual void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) = 0;

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status) saved in the given sync |account|. The passed account should be a
  // current or former syncing account, otherwise |callback| will be
  // called with an error result. Callback is called on the main sequence.
  // TODO(crbug.com/40833594): Clean up/refactor to avoid having methods
  // introduced for a specific backend in this interface.
  virtual void GetAllLoginsForAccountAsync(std::string account,
                                           LoginsOrErrorReply callback) = 0;

  // Returns all PasswordForms with the same signon_realm as a form in |forms|.
  // If |include_psl|==true, the PSL-matched forms are also included.
  // If multiple forms are given, those will be concatenated.
  // Callback is called on the main sequence.
  // TODO(crbug.com/40262259): Remove and replace with
  // GetGroupedMatchingLoginsAsync().
  virtual void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) = 0;

  // Returns all PasswordForms related to |form_digest.signon_realm|.
  // This includes:
  // * PasswordForms exactly matching a given |signon_realm|,
  // * PasswordForms matched through PSL,
  // * PasswordForms with affiliated signon_realm (this might include android
  // apps).
  // * PasswordForms with signon_realm from the same group (this might include
  // android apps).
  // All the forms are unique meaning if PasswordForm was matched
  // through multiple sources all the sources will be mentioned.
  // Callback is called on the main sequence.
  virtual void GetGroupedMatchingLoginsAsync(
      const PasswordFormDigest& form_digest,
      LoginsOrErrorReply callback) = 0;

  // For all methods below:
  // TODO(crbug.com/40185050): Make pure virtual.
  // TODO(crbug.com/40185050): Make PasswordStoreImpl implement it like above.
  // TODO(crbug.com/40185050): Move and Update doc from PasswordStore here.
  // TODO(crbug.com/40185050): Delete corresponding Impl method from
  //  PasswordStore and the async method on backend_ instead.

  // The completion callback in each of the write operations below receive a
  // variant of optional PasswordStoreChangeList or PasswordStoreBackendError.
  // In case of success that the changelist will be populated with the executed
  // changes. The absence of the changelist indicates that the used backend
  // (e.g. on Android) cannot confirm of the execution and a re-fetch is
  // required to know the current state of the backend.
  virtual void AddLoginAsync(const PasswordForm& form,
                             PasswordChangesOrErrorReply callback) = 0;
  virtual void UpdateLoginAsync(const PasswordForm& form,
                                PasswordChangesOrErrorReply callback) = 0;
  virtual void RemoveLoginAsync(const base::Location& location,
                                const PasswordForm& form,
                                PasswordChangesOrErrorReply callback) = 0;
  virtual void RemoveLoginsByURLAndTimeAsync(
      const base::Location& location,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) = 0;
  virtual void RemoveLoginsCreatedBetweenAsync(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) = 0;
  virtual void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) = 0;

  virtual SmartBubbleStatsStore* GetSmartBubbleStatsStore() = 0;

  // For sync codebase only: instantiates a proxy controller delegate to
  // react to sync events.
  virtual std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() = 0;

  // Propagates sync initialization event.
  virtual void OnSyncServiceInitialized(syncer::SyncService* sync_service) = 0;

  // Records calls to the `AddLoginAsync()` from the password store.
  // TODO: crbug.com/327126704 - Remove this method after UPM is launched.
  virtual void RecordAddLoginAsyncCalledFromTheStore() = 0;

  // Records calls to the `UpdateLoginAsync()` from the password store.
  // TODO: crbug.com/327126704 - Remove this method after UPM is launched.
  virtual void RecordUpdateLoginAsyncCalledFromTheStore() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<PasswordStoreBackend> AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_H_
