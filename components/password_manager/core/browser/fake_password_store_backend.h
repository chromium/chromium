// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_STORE_BACKEND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"

namespace password_manager {

struct PasswordForm;

class SmartBubbleStatsStore;

using PasswordMap = std::
    map<std::string /* signon_realm */, std::vector<PasswordForm>, std::less<>>;

// Fake password store backend to be used in tests.
class FakePasswordStoreBackend : public PasswordStoreBackend {
 public:
  FakePasswordStoreBackend();
  ~FakePasswordStoreBackend() override;

  const PasswordMap& stored_passwords() const { return stored_passwords_; }

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

  LoginsResult GetAllLoginsInternal();
  LoginsResult GetAutofillableLoginsInternal();
  LoginsResult FillMatchingLoginsInternal(
      const std::vector<PasswordFormDigest>& forms,
      bool include_psl);
  LoginsResult FillMatchingLoginsHelper(const PasswordFormDigest& form,
                                        bool include_psl);
  PasswordStoreChangeList AddLoginInternal(const PasswordForm& form);
  PasswordStoreChangeList UpdateLoginInternal(const PasswordForm& form);
  void DisableAutoSignInForOriginsInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);
  PasswordStoreChangeList RemoveLoginInternal(const PasswordForm& form);

  PasswordMap stored_passwords_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_STORE_BACKEND_H_
