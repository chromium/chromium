// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class ProxyModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

class LoginDatabase;

struct PasswordForm;

class FieldInfoStore;
class SmartBubbleStatsStore;

using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;
using LoginsReply = base::OnceCallback<void(LoginsResult)>;
using PasswordStoreChangeListReply =
    base::OnceCallback<void(const PasswordStoreChangeList&)>;

// The backend is used by the `PasswordStore` to interact with the storage in a
// platform-dependent way (e.g. on Desktop, it calls a local database while on
// Android, it sends requests to a service).
// All methods are required to do their work asynchronously to prevent expensive
// IO operation from possibly blocking the main thread.
class PasswordStoreBackend {
 public:
  using OptionalLoginsReply =
      base::OnceCallback<void(absl::optional<LoginsResult>)>;
  using RemoteChangesReceived =
      base::RepeatingCallback<void(const PasswordStoreChangeList&)>;

  PasswordStoreBackend() = default;
  PasswordStoreBackend(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend(PasswordStoreBackend&&) = delete;
  PasswordStoreBackend& operator=(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend& operator=(PasswordStoreBackend&&) = delete;
  virtual ~PasswordStoreBackend() = default;

  // TODO(crbug.bom/1226042): Rename this to Init after PasswordStoreImpl no
  // longer inherits PasswordStore.
  virtual void InitBackend(RemoteChangesReceived remote_form_changes_received,
                           base::RepeatingClosure sync_enabled_or_disabled_cb,
                           base::OnceCallback<void(bool)> completion) = 0;

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status) and notify `consumer` on completion. Callback is called on the main
  // sequence.
  virtual void GetAllLoginsAsync(LoginsReply callback) = 0;

  // Returns the complete list of non-blocklist PasswordForms. Callback is
  // called on the main sequence.
  virtual void GetAutofillableLoginsAsync(LoginsReply callback) = 0;

  // Returns all PasswordForms with the same signon_realm as a form in |forms|.
  // If |include_psl|==true, the PSL-matched forms are also included.
  // If multiple forms are given, those will be concatenated.
  // Callback is called on the main sequence.
  // TODO(crbug.com/1217071): Check whether this needs OptionalLoginsReply, too.
  virtual void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) = 0;

  // For all methods below:
  // TODO(crbug.com/1217071): Make pure virtual.
  // TODO(crbug.com/1217071): Make PasswordStoreImpl implement it like above.
  // TODO(crbug.com/1217071): Move and Update doc from PasswordStore here.
  // TODO(crbug.com/1217071): Delete corresponding Impl method from
  //  PasswordStore and the async method on backend_ instead.

  virtual void AddLoginAsync(const PasswordForm& form,
                             PasswordStoreChangeListReply callback) = 0;
  virtual void UpdateLoginAsync(const PasswordForm& form,
                                PasswordStoreChangeListReply callback) = 0;
  virtual void RemoveLoginAsync(const PasswordForm& form,
                                PasswordStoreChangeListReply callback) = 0;
  virtual void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) = 0;
  virtual void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) = 0;
  virtual void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) = 0;

  virtual SmartBubbleStatsStore* GetSmartBubbleStatsStore() = 0;
  virtual FieldInfoStore* GetFieldInfoStore() = 0;

  // For sync codebase only: instantiates a proxy controller delegate to
  // react to sync events.
  virtual std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegateFactory() = 0;

  // Factory function for creating the backend. The Local backend requires the
  // provided `login_db` for storage and Android backend for migration purposes.
  static std::unique_ptr<PasswordStoreBackend> Create(
      std::unique_ptr<LoginDatabase> login_db);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_
