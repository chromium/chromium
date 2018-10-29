// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACCESS_TOKEN_FETCHER_H_

#include <string>

namespace autofill_assistant {

// An interface that abstracts the steps needed to choose a user to sign in,
// authenticate and fetch an appropriate oauth token.
class AccessTokenFetcher {
 public:
  virtual ~AccessTokenFetcher() = default;

  // Gets an oauth token, for the appropriate user and scope.
  //
  // If successful, |callback| is called with true and a token.
  virtual void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)>) = 0;

  // Invalidates the given oauth token.
  virtual void InvalidateAccessToken(const std::string& access_token) = 0;

 protected:
  AccessTokenFetcher() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACCESS_TOKEN_FETCHER_H_
