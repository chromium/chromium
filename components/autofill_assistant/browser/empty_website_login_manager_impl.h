// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EMPTY_WEBSITE_LOGIN_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EMPTY_WEBSITE_LOGIN_MANAGER_IMPL_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Native implementation of the autofill assistant website login fetcher, which
// does nothing.
class EmptyWebsiteLoginManagerImpl : public WebsiteLoginManager {
 public:
  EmptyWebsiteLoginManagerImpl();

  EmptyWebsiteLoginManagerImpl(const EmptyWebsiteLoginManagerImpl&) = delete;
  EmptyWebsiteLoginManagerImpl& operator=(const EmptyWebsiteLoginManagerImpl&) =
      delete;

  ~EmptyWebsiteLoginManagerImpl() override;

  // From WebsiteLoginManager:
  void GetLoginsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<Login>)> callback) override;
  void GetPasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback) override;
  void DeletePasswordForLogin(const Login& login,
                              base::OnceCallback<void(bool)> callback) override;
  void GetGetLastTimePasswordUsed(
      const Login& login,
      base::OnceCallback<void(absl::optional<base::Time>)> callback) override;
  void EditPasswordForLogin(const Login& login,
                            const std::string& new_password,
                            base::OnceCallback<void(bool)> callback) override;
  absl::optional<std::string> GeneratePassword(
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature,
      uint64_t max_length) override;
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
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_MANAGER_IMPL_H_
