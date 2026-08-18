// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_retriever.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace one_time_tokens {

namespace {

using ::affiliations::AffiliatedFacets;
using ::affiliations::Facet;
using ::affiliations::FacetURI;

class FakeOneTimeTokenService : public OneTimeTokenService {
 public:
  FakeOneTimeTokenService() = default;
  ~FakeOneTimeTokenService() override = default;

  OneTimeTokenLogSink* log_sink() override { return nullptr; }

  void GetRecentOneTimeTokens(Callback callback) override {}

  std::vector<OneTimeToken> GetCachedOneTimeTokens() const override {
    return cached_tokens_;
  }

  ExpiringSubscription Subscribe(
      OneTimeTokenSource source,
      base::Time expiration,
      Callback callback,
      base::OnceClosure expiration_callback) override {
    return subscription_manager_.Subscribe(expiration, std::move(callback),
                                           std::move(expiration_callback));
  }

  void RequestOneTimeToken(
      base::TimeDelta timeout,
      base::OnceCallback<void(std::optional<OneTimeToken>)> callback) override {
  }

  void FetchUserDataProcessingConsent(
      FetchUserDataProcessingConsentCallback callback) override {
    std::move(callback).Run(/*consent_states=*/std::nullopt);
  }

  void SetCachedTokens(std::vector<OneTimeToken> tokens) {
    cached_tokens_ = std::move(tokens);
  }

  template <typename... Args>
  void NotifySubscribers(Args&&... args) {
    subscription_manager_.Notify(std::forward<Args>(args)...);
  }

 private:
  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;
  std::vector<OneTimeToken> cached_tokens_;
};

class GmailOtpRetrieverTest : public testing::Test {
 public:
  GmailOtpRetrieverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~GmailOtpRetrieverTest() override = default;

  void SetUp() override {}

  FakeOneTimeTokenService& otp_service() { return otp_service_; }
  affiliations::FakeAffiliationService& affiliation_service() {
    return affiliation_service_;
  }
  std::unique_ptr<affiliations::DomainRelationChecker>
  domain_relation_checker() {
    return std::make_unique<affiliations::DomainRelationChecker>(
        affiliation_service_);
  }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
  affiliations::FakeAffiliationService affiliation_service_;
  FakeOneTimeTokenService otp_service_;
};

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_SuccessFromCache) {
  base::HistogramTester histogram_tester;
  const std::string kOtp = "123456";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "sender@example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
  EXPECT_EQ(future.Get()->source, GmailOtpRetriever::Source::kCache);

  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason.Cached",
      0);
  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason."
      "Received",
      0);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_SuccessFromSubscription) {
  base::HistogramTester histogram_tester;
  const std::string kOtp = "654321";

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
                   "sender@example.com"));

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
  EXPECT_EQ(future.Get()->source, GmailOtpRetriever::Source::kReceived);

  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason.Cached",
      0);
  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason."
      "Received",
      0);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_Superseded) {
  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future2;

  auto retriever1 = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future1.GetCallback());

  // Destroying retriever1 cancels the first request.
  retriever1.reset();
  EXPECT_FALSE(future1.IsReady());

  auto retriever2 = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future2.GetCallback());

  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   "sender@example.com"));

  ASSERT_TRUE(future2.Get().has_value());
  EXPECT_EQ(future2.Get()->otp, "123456");
  EXPECT_FALSE(future1.IsReady());
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_Timeout) {
  otp_service().SetCachedTokens({});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kSubscriptionExpired);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_CachedToken_ExactMatch) {
  const std::string kOtp = "123456";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "no-reply@example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_CachedToken_WwwExactMatch) {
  const std::string kOtp = "123456";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "no-reply@example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  // Frame origin has www., sender domain does not.
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://www.example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_CachedToken_WwwSender_Rejected) {
  const std::string kOtp = "123456";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "no-reply@www.example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  // Frame origin does not have www., sender domain does.
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Fast forward to trigger timeout since no matching token is received.
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kSubscriptionExpired);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_CachedToken_WwwSender_AllowedForLoginFlow) {
  const std::string kOtp = "123456";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "no-reply@www.example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  // Frame origin does not have www., sender domain does. This is a PSL match,
  // allowed for login flow.
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/true, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_CachedToken_WwwSenderWwwFrame_RejectedForNonLoginFlow) {
  const std::string kOtp = "123456";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "no-reply@www.example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  // Both frame origin and sender domain have www., but because frame is
  // normalized (stripped of www), they evaluate to a PSL match and are rejected
  // for non-login flows.
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://www.example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Fast forward to trigger timeout.
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kSubscriptionExpired);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_CachedToken_AffiliatedMatch) {
  const std::string kOtp = "999888";
  affiliation_service().AddAffiliationGroup(AffiliatedFacets{
      {Facet{FacetURI::FromCanonicalSpec("https://example.com")},
       Facet{FacetURI::FromCanonicalSpec("https://affiliated.com")}}});

  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "auth@affiliated.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_CachedToken_PslMatch_AllowedForLoginFlow) {
  const std::string kOtp = "555444";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "service@sub.example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/true, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kOtp);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_SubscriptionToken_PslMatch_AllowedForLoginFlow) {
  const std::string kPslOtp = "654321";
  otp_service().SetCachedTokens({});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/true, future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  // Simulate receiving a PSL matched token from subscription.
  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, kPslOtp, base::TimeTicks::Now(),
                   "sender@sub.example.com"));

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kPslOtp);
  EXPECT_EQ(future.Get()->source, GmailOtpRetriever::Source::kReceived);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_CachedToken_PslMatch_RejectedForNonLoginFlow) {
  base::HistogramTester histogram_tester;
  affiliation_service().AddAffiliationGroup(AffiliatedFacets{
      {Facet{FacetURI::FromCanonicalSpec("https://example.com")}}});
  const std::string kOtp = "555444";
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, kOtp, base::TimeTicks::Now(),
        "service@sub.example.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Ensure the cached token check finishes and records its rejection before
  // the incoming email arrives.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](FakeOneTimeTokenService* service) {
            service->NotifySubscribers(
                OneTimeTokenSource::kGmail,
                OneTimeToken(OneTimeTokenType::kGmail, "111222",
                             base::TimeTicks::Now(), "auth@example.com"));
          },
          base::Unretained(&otp_service())));

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, "111222");

  histogram_tester.ExpectUniqueSample(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason.Cached",
      GmailOtpSenderDomainMatchRejectionReason::kPslMatchDisallowed, 1);
  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason."
      "Received",
      0);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_MultipleTokens_SortedByArrivalTime) {
  const std::string kOldGmailOtp = "222222";
  const std::string kRecentGmailOtp = "333333";

  base::TimeTicks now = base::TimeTicks::Now();

  std::vector<OneTimeToken> cached_tokens = {
      {OneTimeTokenType::kGmail, kOldGmailOtp, now - base::Minutes(2),
       "sender@example.com"},
      {OneTimeTokenType::kGmail, kRecentGmailOtp, now - base::Minutes(1),
       "sender@example.com"}};

  otp_service().SetCachedTokens(cached_tokens);

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, kRecentGmailOtp);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_MultiplePendingChecks_TimeoutFiresOnlyAfterAllComplete) {
  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;

  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                   "sender@different.com"));
  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, "222222", base::TimeTicks::Now(),
                   "sender2@different.com"));
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kSubscriptionExpired);
}

TEST_F(
    GmailOtpRetrieverTest,
    RetrieveOtp_CachedTokenCheckPendingDuringTimeout_ResolvesMatchCorrectly) {
  affiliation_service().AddAffiliationGroup(
      {affiliations::Facet(
           affiliations::FacetURI::FromCanonicalSpec("https://example.com")),
       affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(
           "https://different.com"))});

  std::vector<OneTimeToken> items;
  items.emplace_back(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                     "sender@different.com");
  otp_service().SetCachedTokens(items);

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;

  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));

  // The match succeeded despite the timeout, we expect to get the token!
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, "111111");
  EXPECT_EQ(future.Get()->source, GmailOtpRetriever::Source::kCache);
}

TEST_F(GmailOtpRetrieverTest,
       RetrieveOtp_TimeoutWhileCheckPending_ResolvesMatchCorrectly) {
  affiliation_service().AddAffiliationGroup(
      {affiliations::Facet(
           affiliations::FacetURI::FromCanonicalSpec("https://example.com")),
       affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(
           "https://different.com"))});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;

  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                   "sender@different.com"));
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));

  // The match succeeded after the timeout fired, and it was picked up
  // successfully.
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, "111111");
  EXPECT_EQ(future.Get()->source, GmailOtpRetriever::Source::kReceived);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_OpaqueOrigin_ReturnsError) {
  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;

  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(), url::Origin(),
      /*is_login_flow=*/false, future.GetCallback());

  EXPECT_NE(retriever, nullptr);
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), OneTimeTokenRetrievalError::kGmailOtpUnknown);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_SubscriptionError_ReturnsError) {
  otp_service().SetCachedTokens({});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      base::unexpected(OneTimeTokenRetrievalError::kGmailOtpBackendAuthError));

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendAuthError);
}

TEST_F(
    GmailOtpRetrieverTest,
    RetrieveOtp_CachedTokenCheckPendingDuringSubscriptionError_ResolvesMatchCorrectly) {
  affiliation_service().AddAffiliationGroup(
      {affiliations::Facet(
           affiliations::FacetURI::FromCanonicalSpec("https://example.com")),
       affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(
           "https://different.com"))});

  std::vector<OneTimeToken> items;
  items.emplace_back(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                     "sender@different.com");
  otp_service().SetCachedTokens(items);

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;

  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  // Simulate subscription returning an error.
  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      base::unexpected(OneTimeTokenRetrievalError::kGmailOtpBackendAuthError));

  // The match succeeded despite the subscription error, the token should
  // be returned and not the error.
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->otp, "111111");
  EXPECT_EQ(future.Get()->source, GmailOtpRetriever::Source::kCache);
}

TEST_F(
    GmailOtpRetrieverTest,
    RetrieveOtp_CachedTokenCheckPendingDuringSubscriptionError_ResolvesMatchAsFailed) {
  std::vector<OneTimeToken> items;
  items.emplace_back(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                     "sender@different.com");
  otp_service().SetCachedTokens(items);

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;

  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  // Simulate subscription returning an error.
  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      base::unexpected(OneTimeTokenRetrievalError::kGmailOtpBackendAuthError));

  // The match failed, the subscription error should be returned.
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendAuthError);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_CachedToken_NoMatch_LogsRejection) {
  base::HistogramTester histogram_tester;
  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
        "sender@nomatch.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  // Fast forward to trigger timeout. This resolves the future.
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason.Cached",
      GmailOtpSenderDomainMatchRejectionReason::kNoMatch, 1);
  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason."
      "Received",
      0);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_CachedToken_Grouped_LogsRejection) {
  base::HistogramTester histogram_tester;

  affiliations::GroupedFacets group;
  group.facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://example.com"));
  group.facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://grouped.com"));
  affiliation_service().AddGroupedFacets(group);

  otp_service().SetCachedTokens(
      {{OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
        "sender@grouped.com"}});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  // Fast forward to trigger timeout. This resolves the future.
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason.Cached",
      GmailOtpSenderDomainMatchRejectionReason::kGrouped, 1);
}

TEST_F(GmailOtpRetrieverTest, RetrieveOtp_ReceivedToken_RejectionsLogged) {
  base::HistogramTester histogram_tester;
  otp_service().SetCachedTokens({});

  base::test::TestFuture<
      base::expected<GmailOtpRetriever::Result, OneTimeTokenRetrievalError>>
      future;
  auto retriever = GmailOtpRetriever::CreateAndStart(
      otp_service(), domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")),
      /*is_login_flow=*/false, future.GetCallback());

  // Notify with a non-matching token.
  otp_service().NotifySubscribers(
      OneTimeTokenSource::kGmail,
      OneTimeToken(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                   "sender@nomatch.com"));

  // Fast forward to trigger timeout. This resolves the future.
  task_environment().FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason."
      "Received",
      GmailOtpSenderDomainMatchRejectionReason::kNoMatch, 1);
  histogram_tester.ExpectTotalCount(
      "OneTimeTokens.GmailOtpRetriever.SenderDomainMatchRejectionReason.Cached",
      0);
}

}  // namespace
}  // namespace one_time_tokens
