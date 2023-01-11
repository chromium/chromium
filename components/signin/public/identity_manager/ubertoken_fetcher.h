// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_UBERTOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_UBERTOKEN_FETCHER_H_

#include <string>

#include "base/functional/callback_forward.h"

class GoogleServiceAuthError;

namespace signin {

// Opaque interface that fetches ubertokens for a given account. Clients must
// go through IdentityManager to create a functioning instance.
class UbertokenFetcher {
 public:
  using CompletionCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              const std::string& token)>;

  // Constructs an instance and start fetching the token for |account_id|.
  UbertokenFetcher() = default;

  UbertokenFetcher(const UbertokenFetcher&) = delete;
  UbertokenFetcher& operator=(const UbertokenFetcher&) = delete;

  virtual ~UbertokenFetcher() = 0;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_UBERTOKEN_FETCHER_H_
