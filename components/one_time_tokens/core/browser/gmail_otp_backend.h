// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace one_time_tokens {

// Abstract interface for fetching OTPs from Gmail.
class GmailOtpBackend : public KeyedService {
 public:
  using CallbackSignature =
      void(base::expected<OneTimeToken, OneTimeTokenRetrievalError>);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  using EncryptedMessageReference =
      base::StrongAlias<class EncryptedMessageReferenceTag, std::string>;

  ~GmailOtpBackend() override;

  // Creates a new instance of the backend.
  static std::unique_ptr<GmailOtpBackend> Create();

  // Creates a subscription for new incoming OTPs.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(base::Time expiration,
                                                       Callback callback) = 0;

  // Called when a new OTP is received via the OneTimeToken notification.
  virtual void OnIncomingOneTimeTokenBackendTickle(
      const EncryptedMessageReference& encrypted_message_reference) = 0;
};

// Concrete implementation of GmailOtpBackend that provides a fake OTP
// response. This is intended for use in testing and development environments
// where a real backend is not available.
class GmailOtpBackendImpl : public GmailOtpBackend {
 public:
  GmailOtpBackendImpl();
  ~GmailOtpBackendImpl() override;

  // GmailOtpBackend:
  ExpiringSubscription Subscribe(base::Time expiration,
                                 Callback callback) override;

  void OnIncomingOneTimeTokenBackendTickle(
      const GmailOtpBackend::EncryptedMessageReference&
          encrypted_message_reference) override;

 private:
  // Queries the backend for recently received OTPs.
  void RetrieveGmailOtpIfNeeded();
  void OnResponseFromGmailOtpBackend(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);

  // Handles subscriptions to the `GmailOtpBackend`.
  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;

  // Indicates whether there is currently a request in flight to retrieve a
  // Gmail OTP. This prevents multiple concurrent requests. Timeouts for the OTP
  // itself are handled by the consumer of the ExpiringSubscription, not by this
  // flag.
  bool has_pending_request_ = false;

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<GmailOtpBackendImpl> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_BACKEND_H_
