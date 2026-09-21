// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_RETRIEVER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_RETRIEVER_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "url/origin.h"

namespace affiliations {
class DomainRelationChecker;
}  // namespace affiliations

namespace one_time_tokens {

class OneTimeTokenService;
enum class OneTimeTokenSource;

// Manages a single request to retrieve a Gmail One-Time Password (OTP) for a
// target origin frame.
//
// It queries cached tokens and listens for incoming ones from
// `OneTimeTokenService`. Only tokens whose sender is accepted by
// `GmailOtpSenderDomainMatcher` for the target frame origin are accepted. The
// retriever keeps listening for incoming tokens
// either until it finds a match (and all pending backend requests complete) or
// the subscription times out.
//
// This is a single-use object. On destruction, it cancels pending domain
// checks, unsubscribes from `OneTimeTokenService`, and discards any pending
// callback.
class GmailOtpRetriever {
 public:
  // Indicates the source of a successfully retrieved matching OTP.
  // Used by callers (e.g. `ActorOneTimeTokenFillingServiceImpl`) to log
  // metrics.
  enum class Source {
    kCache,
    kReceived,
  };

  // Holds the retrieved OTP value and its source (cache vs. live subscription).
  struct Result {
    std::string otp;
    Source source;
  };

  friend std::ostream& operator<<(std::ostream& os,
                                  GmailOtpRetriever::Source source);

  using ResultCallback = base::OnceCallback<void(
      base::expected<Result, OneTimeTokenRetrievalError>)>;

  // Creates a `GmailOtpRetriever` instance and immediately starts the retrieval
  // and domain matching flow.
  static std::unique_ptr<GmailOtpRetriever> CreateAndStart(
      OneTimeTokenService& service,
      std::unique_ptr<affiliations::DomainRelationChecker>
          domain_relation_checker,
      const url::Origin& otp_frame_origin,
      bool is_login_flow,
      ResultCallback callback);

  ~GmailOtpRetriever();

  GmailOtpRetriever(const GmailOtpRetriever&) = delete;
  GmailOtpRetriever& operator=(const GmailOtpRetriever&) = delete;

 private:
  struct Candidate {
    std::string otp;
    Source source;
    base::Time email_received_timestamp;
  };

  GmailOtpRetriever(OneTimeTokenService& service,
                    std::unique_ptr<affiliations::DomainRelationChecker>
                        domain_relation_checker,
                    const url::Origin& otp_frame_origin,
                    bool is_login_flow,
                    ResultCallback callback);

  void Start();
  void SubscribeForOneTimeToken();
  // Asks `sender_domain_matcher_` whether `sender_address` may fill the frame.
  // The matcher is owned by this object, so destroying `this` cancels the
  // check.
  void StartSenderDomainCheck(
      std::string_view sender_address,
      GmailOtpSenderDomainMatcher::ResultCallback callback);
  void CheckCachedTokenMatch(std::vector<OneTimeToken> cached_tokens,
                             size_t index);
  bool IsMatchTypeAllowed(GmailOtpSenderDomainMatchType match_type) const;
  void OnCachedTokenMatchChecked(std::vector<OneTimeToken> cached_tokens,
                                 size_t index,
                                 GmailOtpSenderDomainMatchType match_type);
  void OnOneTimeTokenReceived(
      OneTimeTokenSource source,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result);
  void OnReceivedTokenMatchChecked(OneTimeToken token,
                                   GmailOtpSenderDomainMatchType match_type);
  void OnOneTimeTokenTimeout();
  void MaybeCompleteOrWaitForPendingRequests();
  void OnOpaqueOriginDetected();

  const raw_ref<OneTimeTokenService> one_time_token_service_;
  const url::Origin otp_frame_origin_;
  const bool is_login_flow_;
  // Answers all sender domain checks of this retrieval.
  GmailOtpSenderDomainMatcher sender_domain_matcher_;
  size_t pending_sender_domain_checks_ = 0;
  std::optional<OneTimeTokenRetrievalError> error_;
  ExpiringSubscription subscription_;
  ResultCallback retrieve_otp_callback_;
  std::optional<Candidate> best_candidate_;

  base::WeakPtrFactory<GmailOtpRetriever> weak_ptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_RETRIEVER_H_
