// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_FETCHER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_FETCHER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace autofill_assistant {

// Native implementation of the autofill assistant website login fetcher, which
// wraps access to the Chrome password manager.
class WebsiteLoginFetcherImpl : public WebsiteLoginFetcher {
 public:
  WebsiteLoginFetcherImpl(
      const password_manager::PasswordManagerClient* client);
  ~WebsiteLoginFetcherImpl() override;

  // From WebsiteLoginFetcher:
  void GetLoginsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<Login>)> callback) override;
  void GetPasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback) override;

 private:
  class PendingRequest;
  class PendingFetchLoginsRequest;
  class PendingFetchPasswordRequest;

  void OnRequestFinished(const PendingRequest* request);

  const password_manager::PasswordManagerClient* client_;

  // Fetch requests owned by the password manager, released when they are
  // finished.
  std::vector<std::unique_ptr<PendingRequest>> pending_requests_;

  // Needs to be the last member.
  base::WeakPtrFactory<WebsiteLoginFetcherImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteLoginFetcherImpl);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_FETCHER_IMPL_H_
