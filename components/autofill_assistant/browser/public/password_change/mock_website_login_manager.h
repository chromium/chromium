// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_MOCK_WEBSITE_LOGIN_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_MOCK_WEBSITE_LOGIN_MANAGER_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

// Mock login fetcher for unit tests.
class MockWebsiteLoginManager : public WebsiteLoginManager {
 public:
  MockWebsiteLoginManager();

  MockWebsiteLoginManager(const MockWebsiteLoginManager&) = delete;
  MockWebsiteLoginManager& operator=(const MockWebsiteLoginManager&) = delete;

  ~MockWebsiteLoginManager() override;

  MOCK_METHOD(void,
              GetLoginsForUrl,
              (const GURL& url,
               base::OnceCallback<void(std::vector<Login>)> callback),
              (override));

  MOCK_METHOD(void,
              GetPasswordForLogin,
              (const Login& login,
               base::OnceCallback<void(bool, std::string)> callback),
              (override));

  MOCK_METHOD(void,
              DeletePasswordForLogin,
              (const Login& login, base::OnceCallback<void(bool)> callback),
              (override));

  MOCK_METHOD(void,
              EditPasswordForLogin,
              (const Login& login,
               const std::string& new_password,
               base::OnceCallback<void(bool)> callback),
              (override));

  MOCK_METHOD(absl::optional<std::string>,
              GeneratePassword,
              (content::RenderFrameHost * rfh,
               autofill::FormSignature form_signature,
               autofill::FieldSignature field_signature,
               uint64_t max_length),
              (override));

  MOCK_METHOD(const std::string&, GetGeneratedPassword, (), (override));

  MOCK_METHOD(void,
              PresaveGeneratedPassword,
              (const Login& login,
               const std::string& password,
               const autofill::FormData& form_data,
               base::OnceCallback<void()> callback),
              (override));

  MOCK_METHOD(void,
              GetGetLastTimePasswordUsed,
              (const Login& login,
               base::OnceCallback<void(absl::optional<base::Time>)> callback),
              (override));

  MOCK_METHOD2(OnDeletePasswordForLogin,
               void(const Login& login, base::OnceCallback<void(bool)>&));

  MOCK_METHOD(bool, ReadyToSaveGeneratedPassword, (), (override));

  MOCK_METHOD(void, SaveGeneratedPassword, (), (override));

  MOCK_METHOD(void, ResetPendingCredentials, (), (override));

  MOCK_METHOD(bool, ReadyToSaveSubmittedPassword, (), (override));

  MOCK_METHOD(bool, SubmittedPasswordIsSame, (), (override));

  MOCK_METHOD(void,
              CheckWhetherSubmittedCredentialIsLeaked,
              (SavePasswordLeakDetectionDelegate::Callback, base::TimeDelta),
              (override));

  MOCK_METHOD(bool, SaveSubmittedPassword, (), (override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_MOCK_WEBSITE_LOGIN_MANAGER_H_
