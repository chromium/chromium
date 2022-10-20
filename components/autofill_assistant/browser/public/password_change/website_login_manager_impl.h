// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_WEBSITE_LOGIN_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_WEBSITE_LOGIN_MANAGER_IMPL_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace autofill_assistant {

// Native implementation of the autofill assistant website login fetcher, which
// wraps access to the Chrome password manager.
class WebsiteLoginManagerImpl : public WebsiteLoginManager {
 public:
  WebsiteLoginManagerImpl(password_manager::PasswordManagerClient* client,
                          content::WebContents* web_contents);

  WebsiteLoginManagerImpl(const WebsiteLoginManagerImpl&) = delete;
  WebsiteLoginManagerImpl& operator=(const WebsiteLoginManagerImpl&) = delete;

  ~WebsiteLoginManagerImpl() override;

  // From WebsiteLoginManager:
  void GetLoginsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<Login>)> callback) override;
  void GetPasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback) override;

  void GetGetLastTimePasswordUsed(
      const Login& login,
      base::OnceCallback<void(absl::optional<base::Time>)> callback) override;

  absl::optional<std::string> GeneratePassword(
      content::RenderFrameHost* rfh,
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature,
      uint64_t max_length) override;

  const std::string& GetGeneratedPassword() override;

  void PresaveGeneratedPassword(const Login& login,
                                const std::string& password,
                                const autofill::FormData& form_data,
                                base::OnceCallback<void()> callback) override;

  bool ReadyToSaveGeneratedPassword() override;

  void SaveGeneratedPassword() override;

  void ResetPendingCredentials() override;

  bool ReadyToSaveSubmittedPassword() override;

  bool SubmittedPasswordIsSame() override;

  void CheckWhetherSubmittedCredentialIsLeaked(
      SavePasswordLeakDetectionDelegate::Callback callback,
      base::TimeDelta timeout) override;

  bool SaveSubmittedPassword() override;

 private:
  class PendingRequest;
  class PendingFetchLoginsRequest;
  class PendingFetchPasswordRequest;
  class UpdatePasswordRequest;
  class PendingFetchLastTimePasswordUseRequest;
  class WebsiteLeakDetectionDelegate;

  void OnRequestFinished(const PendingRequest* request);

  const raw_ptr<password_manager::PasswordManagerClient, DanglingUntriaged>
      client_;

  const raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

  // Update password request will be created in PresaveGeneratedPassword and
  // released in CommitGeneratedPassword after committing presaved password to
  // password store.
  std::unique_ptr<UpdatePasswordRequest> update_password_request_;

  // Fetch requests owned by the password manager, released when they are
  // finished.
  std::vector<std::unique_ptr<PendingRequest>> pending_requests_;

  // LeakDetection requests are created, owned and their results received by
  // a SavePasswordLeakDetectionDelegate.
  std::unique_ptr<SavePasswordLeakDetectionDelegate> leak_delegate_;

  // Generated password suggested to the user.
  std::string generated_password_;

  // Needs to be the last member.
  base::WeakPtrFactory<WebsiteLoginManagerImpl> weak_ptr_factory_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_WEBSITE_LOGIN_MANAGER_IMPL_H_
