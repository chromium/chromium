// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PROXY_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PROXY_BACKEND_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/password_manager/core/browser/password_store_backend.h"

namespace password_manager {

// This backend forwards requests to two backends in order to compare and record
// their results and time differences. The main backend fulfills the  request
// while the shadow backend is only queried to record shadow traffic.
class PasswordStoreProxyBackend : public PasswordStoreBackend {
 public:
  PasswordStoreProxyBackend(
      std::unique_ptr<PasswordStoreBackend> main_backend,
      std::unique_ptr<PasswordStoreBackend> shadow_backend);
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
  void GetAllLoginsAsync(LoginsReply callback) override;
  void GetAutofillableLoginsAsync(LoginsReply callback) override;
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
  CreateSyncControllerDelegateFactory() override;

  base::RepeatingCallback<void(bool)> pending_initialization_calls_;
  base::RepeatingClosure pending_shutdown_calls_;

  std::unique_ptr<PasswordStoreBackend> main_backend_;
  std::unique_ptr<PasswordStoreBackend> shadow_backend_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PROXY_BACKEND_H_
