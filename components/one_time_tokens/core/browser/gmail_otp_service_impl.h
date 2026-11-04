// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_IMPL_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_IMPL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
#include "url/origin.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace one_time_tokens {

class GmailOtpBackend;
class GmailOtpSenderDomainMatcher;
enum class GmailOtpSenderDomainMatchType;

class GmailOtpServiceImpl : public GmailOtpService {
 public:
  // `backend` and `affiliation_service` must be non-null and must outlive
  // `this` (both are `KeyedService`s declared as dependencies in
  // `GmailOtpServiceFactory`).
  GmailOtpServiceImpl(
      GmailOtpBackend* backend,
      affiliations::AffiliationService* affiliation_service,
      std::optional<OneTimeTokenLogSink::LogHandler> log_handler);

  GmailOtpServiceImpl(const GmailOtpServiceImpl&) = delete;
  GmailOtpServiceImpl& operator=(const GmailOtpServiceImpl&) = delete;
  ~GmailOtpServiceImpl() override;

  // GmailOtpService:
  GmailOtpBackend* GetBackend() override;
  void GetFreshestCachedOtp(const url::Origin& origin,
                            bool allow_psl_matching,
                            GetFreshestCachedOtpCallback callback) override;
  [[nodiscard]] ExpiringSubscription Subscribe(
      const url::Origin& origin,
      bool allow_psl_matching,
      base::Time expiration,
      IncomingTokenCallback callback,
      base::OnceClosure expiration_callback) override;
  bool HasPendingRequests(
      const ExpiringSubscription& subscription) const override;
  OneTimeTokenLogSink* GetLogSink() override;

 private:
  struct ScopedDomainMatcherCleanup;

  void StartCachedOtpCheck(ExpiringSubscriptionHandle check_handle,
                           std::vector<OneTimeToken> tokens,
                           size_t current_index,
                           bool allow_psl_matching,
                           GetFreshestCachedOtpCallback callback);
  void OnCachedTokenMatchChecked(ExpiringSubscriptionHandle check_handle,
                                 std::vector<OneTimeToken> tokens,
                                 size_t current_index,
                                 bool allow_psl_matching,
                                 GetFreshestCachedOtpCallback callback,
                                 GmailOtpSenderDomainMatchType match_type);

  void EnsureBackendSubscription(base::Time expiration);

  void NotifySubscriber(
      const ScopedDomainMatcherCleanup* cleanup,
      bool allow_psl_matching,
      IncomingTokenCallback callback,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result);
  void OnBackendTokenReceived(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result);
  void OnBackendSubscriptionExpired();
  void OnSubscriberDomainMatchChecked(bool allow_psl_matching,
                                      IncomingTokenCallback callback,
                                      OneTimeToken token,
                                      GmailOtpSenderDomainMatchType match_type);

  raw_ptr<GmailOtpBackend> backend_;
  affiliations::DomainRelationChecker domain_relation_checker_;
  base::CallbackListSubscription log_subscription_;

  // Owns domain matchers for active subscriptions and in-flight
  // `GetFreshestCachedOtp` checks, and backs `HasPendingRequests()`. Must be
  // declared after `domain_relation_checker_` so matchers (and their
  // `raw_ref<DomainRelationChecker>`) are destroyed before it.
  base::flat_map<ExpiringSubscriptionHandle,
                 std::unique_ptr<GmailOtpSenderDomainMatcher>>
      domain_matchers_;

  // Active backend subscription and subscribers.
  ExpiringSubscription backend_subscription_;
  ExpiringSubscriptionManager<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
      subscription_manager_;

  base::WeakPtrFactory<GmailOtpServiceImpl> weak_ptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_IMPL_H_
