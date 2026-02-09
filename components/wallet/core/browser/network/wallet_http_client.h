// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "components/wallet/core/browser/proto/pass.pb.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"

namespace wallet {

struct WalletPass;

// WalletHttpClient issues requests to the Wallet backend.
class WalletHttpClient {
 public:
  enum class WalletRequestError {
    kGenericError = 1,
    kAccessTokenFetchFailed = 2,
    kParseResponseFailed = 3,
    // TODO(crbug.com/468915960): Add more error codes.
  };

  // Callback for UpsertPass requests. On success, it returns the pass_id
  // as it is stored in the Wallet backend.
  using UpsertPublicPassCallback = base::OnceCallback<void(
      const base::expected<std::string, WalletRequestError>&)>;

  // Callback for UpsertPrivatePass requests. On success, it returns the
  // `PrivatePass` as it is stored in the Wallet backend.
  using UpsertPrivatePassCallback = base::OnceCallback<void(
      const base::expected<PrivatePass, WalletRequestError>&)>;

  // Callback for GetUnmaskedPass requests. On success, it returns the
  // `PrivatePass` corresponding to the requested `pass_id`.
  using GetUnmaskedPassCallback = base::OnceCallback<void(
      const base::expected<PrivatePass, WalletRequestError>&)>;

  using HttpResponse = base::expected<std::string, WalletRequestError>;

  virtual ~WalletHttpClient() = default;

  // Upserts a pass to the Wallet backend. If the `pass.id` is missing, it
  // will save a new pass. If the `pass.id` is present, it will attempt to
  // update the existing pass.
  virtual void UpsertPublicPass(Pass pass,
                                UpsertPublicPassCallback callback) = 0;

  // Upserts a pass to the Wallet backend. If the `pass.id` is missing, it
  // will save a new pass. If the `pass.id` is present, it will attempt to
  // update the existing pass.
  virtual void UpsertPrivatePass(PrivatePass pass,
                                 UpsertPrivatePassCallback callback) = 0;

  // Retrieves the unmasked version of the pass for the given `pass_id`.
  // TODO(crbug.com/478783796): Update to use protos.
  virtual void GetUnmaskedPass(std::string_view pass_id,
                               GetUnmaskedPassCallback callback) = 0;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_H_
