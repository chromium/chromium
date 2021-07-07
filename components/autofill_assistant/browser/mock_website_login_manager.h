// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEBSITE_LOGIN_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEBSITE_LOGIN_MANAGER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

// Mock login fetcher for unit tests.
class MockWebsiteLoginManager : public WebsiteLoginManager {
 public:
  MockWebsiteLoginManager();
  ~MockWebsiteLoginManager() override;

  void GetLoginsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<Login>)> callback) override {
    OnGetLoginsForUrl(url, callback);
  }

  MOCK_METHOD2(OnGetLoginsForUrl,
               void(const GURL& domain,
                    base::OnceCallback<void(std::vector<Login>)>&));

  void GetPasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback) override {
    OnGetPasswordForLogin(login, callback);
  }

  MOCK_METHOD2(OnGetPasswordForLogin,
               void(const Login& login,
                    base::OnceCallback<void(bool, std::string)>&));

  void DeletePasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool)> callback) override {
    OnDeletePasswordForLogin(login, callback);
  }

  MOCK_METHOD2(OnDeletePasswordForLogin,
               void(const Login& login, base::OnceCallback<void(bool)>&));

  void EditPasswordForLogin(const Login& login,
                            const std::string& new_password,
                            base::OnceCallback<void(bool)> callback) override {
    OnEditPasswordForLogin(login, new_password, callback);
  }

  MOCK_METHOD3(OnEditPasswordForLogin,
               void(const Login& login,
                    const std::string&,
                    base::OnceCallback<void(bool)>&));

  std::string GeneratePassword(autofill::FormSignature form_signature,
                               autofill::FieldSignature field_signature,
                               uint64_t max_length) override {
    return GetGeneratedPassword();
  }

  MOCK_METHOD0(GetGeneratedPassword, std::string());

  void PresaveGeneratedPassword(const Login& login,
                                const std::string& password,
                                const autofill::FormData& form_data,
                                base::OnceCallback<void()> callback) override {
    OnPresaveGeneratedPassword(login, password, form_data, callback);
  }

  MOCK_METHOD4(OnPresaveGeneratedPassword,
               void(const Login& login,
                    const std::string& password,
                    const autofill::FormData& form_data,
                    base::OnceCallback<void()>&));

  bool ReadyToCommitGeneratedPassword() override {
    return OnReadyToCommitGeneratedPassword();
  }

  MOCK_METHOD0(OnReadyToCommitGeneratedPassword, bool());

  void CommitGeneratedPassword() override { OnCommitGeneratedPassword(); }

  MOCK_METHOD0(OnCommitGeneratedPassword, void());

  DISALLOW_COPY_AND_ASSIGN(MockWebsiteLoginManager);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEBSITE_LOGIN_MANAGER_H_
