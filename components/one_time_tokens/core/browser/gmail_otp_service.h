// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace one_time_tokens {

class GmailOtpBackend;
class OneTimeTokenLogSink;

// Service to manage Gmail one-time passwords (OTP). One instance per profile.
class GmailOtpService : public KeyedService {
 public:
  using GetFreshestCachedOtpCallback =
      base::OnceCallback<void(std::optional<OneTimeToken>)>;
  using IncomingTokenCallback = base::RepeatingCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>;

  ~GmailOtpService() override = default;

  // Returns the underlying `GmailOtpBackend`.
  virtual GmailOtpBackend* GetBackend() = 0;

  // Asynchronously queries the backend cache for the freshest cached OTP
  // matching `origin`. Invokes `callback` with the freshest matching token if
  // one exists, or `std::nullopt` otherwise. If `origin` is opaque, posts
  // `std::nullopt` asynchronously. Records sender domain match UKM metrics
  // against `ukm_source_id` if valid.
  virtual void GetFreshestCachedOtp(const url::Origin& origin,
                                    ukm::SourceId ukm_source_id,
                                    bool allow_psl_matching,
                                    GetFreshestCachedOtpCallback callback) = 0;

  // Subscribes to incoming OTPs for `origin` until `expiration`.
  // `callback` is called for matching tokens or backend errors.
  // If `origin` is opaque, asynchronously notifies `callback` with
  // `OneTimeTokenRetrievalError::kGmailOtpUnknown`. Records sender domain match
  // UKM metrics against `ukm_source_id` if valid.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(
      const url::Origin& origin,
      ukm::SourceId ukm_source_id,
      bool allow_psl_matching,
      base::Time expiration,
      IncomingTokenCallback callback,
      base::OnceClosure expiration_callback) = 0;

  // Returns true if there are in-flight network requests for OTPs or pending
  // sender domain checks for `subscription`.
  virtual bool HasPendingRequests(
      const ExpiringSubscription& subscription) const = 0;

  // Returns the log sink.
  virtual OneTimeTokenLogSink* GetLogSink() = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_H_
