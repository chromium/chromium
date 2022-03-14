// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PROXY_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PROXY_BACKEND_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"

class PrefService;

namespace password_manager {

// This backend forwards requests to two backends in order to compare and record
// their results and time differences. The main backend fulfills the  request
// while the shadow backend is only queried to record shadow traffic.
class PasswordStoreProxyBackend : public PasswordStoreBackend {
 public:
  // `main_backend` and `shadow_backend` must not be null and must outlive this
  // object as long as Shutdown() is not called.
  PasswordStoreProxyBackend(PasswordStoreBackend* main_backend,
                            PasswordStoreBackend* shadow_backend,
                            PrefService* prefs,
                            SyncDelegate* sync_delegate);
  PasswordStoreProxyBackend(const PasswordStoreProxyBackend&) = delete;
  PasswordStoreProxyBackend(PasswordStoreProxyBackend&&) = delete;
  PasswordStoreProxyBackend& operator=(const PasswordStoreProxyBackend&) =
      delete;
  PasswordStoreProxyBackend& operator=(PasswordStoreProxyBackend&&) = delete;
  ~PasswordStoreProxyBackend() override;

 private:
  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordStoreChangeListReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

  const raw_ptr<PasswordStoreBackend> main_backend_;
  const raw_ptr<PasswordStoreBackend> shadow_backend_;
  raw_ptr<PrefService> const prefs_ = nullptr;
  const raw_ptr<SyncDelegate> sync_delegate_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PROXY_BACKEND_H_
