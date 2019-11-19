// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEBSITE_LOGIN_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEBSITE_LOGIN_FETCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

// Mock login fetcher for unit tests.
class MockWebsiteLoginFetcher : public WebsiteLoginFetcher {
 public:
  MockWebsiteLoginFetcher();
  ~MockWebsiteLoginFetcher() override;

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

  DISALLOW_COPY_AND_ASSIGN(MockWebsiteLoginFetcher);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEBSITE_LOGIN_FETCHER_H_
