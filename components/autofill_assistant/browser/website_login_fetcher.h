// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Common interface for implementations that fetch login details for websites.
class WebsiteLoginFetcher {
 public:
  // Uniquely represents a particular login.
  struct Login {
    Login(const GURL& origin, const std::string& username);
    Login(const Login& other);
    ~Login();

    // The origin of the login website.
    GURL origin;
    std::string username;
  };

  WebsiteLoginFetcher() = default;
  virtual ~WebsiteLoginFetcher() = default;

  // Asynchronously returns all matching login details for |url| in the
  // specified callback.
  virtual void GetLoginsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<Login>)> callback) = 0;

  // Retrieves the password for |login| in the specified |callback|, or |false|
  // if the password could not be retrieved.
  virtual void GetPasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback) = 0;

  DISALLOW_COPY_AND_ASSIGN(WebsiteLoginFetcher);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_FETCHER_H_
