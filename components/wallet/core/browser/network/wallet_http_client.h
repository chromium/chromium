// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"

namespace wallet {

struct WalletPass;

// WalletHttpClient issues requests to the Wallet backend.
class WalletHttpClient {
 public:
  enum class WalletRequestError {
    kGenericError = 1,
    kAccessTokenFetchFailed = 2,
    // TODO(crbug.com/468915960): Add more error codes.
  };
  struct SavePassResult {
    std::string pass_id;
  };
  using SavePassCallback = base::OnceCallback<void(
      base::expected<SavePassResult, WalletRequestError>)>;

  virtual ~WalletHttpClient() = default;

  // Save a pass to the Wallet backend.
  virtual void SavePass(const WalletPass& pass, SavePassCallback callback) = 0;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_H_
