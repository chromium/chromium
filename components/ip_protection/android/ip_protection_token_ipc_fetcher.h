// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_IP_PROTECTION_TOKEN_IPC_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_IP_PROTECTION_TOKEN_IPC_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/ip_protection/android/blind_sign_message_android_impl.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "components/ip_protection/common/ip_protection_token_fetcher_helper.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"

namespace ip_protection {

// An implementation of IpProtectionTokenFetcher that makes IPC calls to
// service implementing IP Protection in the `quiche::BlindSignAuth` library for
// retrieving blind-signed authentication tokens for IP Protection.
class IpProtectionTokenIpcFetcher : public IpProtectionTokenFetcher {
 public:
  // A delegate to support making requests.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Checks if IP Protection is disabled via user settings.
    virtual bool IsTokenFetchEnabled() = 0;
  };

  explicit IpProtectionTokenIpcFetcher(
      Delegate* delegate,
      std::unique_ptr<quiche::BlindSignAuthInterface>
          blind_sign_auth_for_testing = nullptr);

  ~IpProtectionTokenIpcFetcher() override;

  // IpProtectionTokenFetcher implementation:
  void TryGetAuthTokens(uint32_t batch_size,
                        ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;

 private:
  void OnFetchBlindSignedTokenCompleted(
      base::TimeTicks bsa_get_tokens_start_time,
      TryGetAuthTokensCallback callback,
      absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens);

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      std::optional<std::vector<ip_protection::BlindSignedAuthToken>>
          bsa_tokens,
      TryGetAuthTokensCallback callback,
      ip_protection::TryGetAuthTokensAndroidResult result,
      std::optional<base::TimeDelta> duration = std::nullopt);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  std::optional<base::TimeDelta> CalculateBackoff(
      ip_protection::TryGetAuthTokensAndroidResult result);

  raw_ptr<Delegate> delegate_;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`. `last_try_get_auth_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver.
  ip_protection::TryGetAuthTokensAndroidResult
      last_try_get_auth_tokens_result_ =
          ip_protection::TryGetAuthTokensAndroidResult::kSuccess;
  std::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The thread pool task runner on which BSA token generation takes place,
  // isolating this CPU-intensive process from the UI thread.
  scoped_refptr<base::SequencedTaskRunner> thread_pool_task_runner_;

  // The helper, sequence-bound to `thread_pool_task_runner_`.
  base::SequenceBound<ip_protection::IpProtectionTokenFetcherHelper> helper_;

  // The BlindSignAuth implementation used to fetch blind-signed auth tokens.
  std::unique_ptr<ip_protection::BlindSignMessageAndroidImpl>
      blind_sign_message_android_impl_;
  std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_;

  SEQUENCE_CHECKER(sequence_checker_);

  // This must be the last member in this class.
  base::WeakPtrFactory<IpProtectionTokenIpcFetcher> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_IP_PROTECTION_TOKEN_IPC_FETCHER_H_
