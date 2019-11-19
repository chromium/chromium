// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_UBERTOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_UBERTOKEN_FETCHER_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"

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
  virtual ~UbertokenFetcher() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(UbertokenFetcher);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_UBERTOKEN_FETCHER_H_
