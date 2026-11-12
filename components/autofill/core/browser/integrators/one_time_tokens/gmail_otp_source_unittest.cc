// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_source.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service_impl.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::one_time_tokens::FakeGmailOtpService;
using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenRetrievalError;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Test;

auto IsGmailCandidate(const url::Origin& expected_origin,
                      const std::string& expected_code,
                      LocalFrameToken expected_frame = LocalFrameToken()) {
  return AllOf(
      Field(
          "token", &OtpCandidate::token,
          AllOf(Property("type", &OneTimeToken::type, OneTimeTokenType::kGmail),
                Property("value", &OneTimeToken::value, expected_code))),
      Field("verified_origin", &OtpCandidate::verified_origin, expected_origin),
      Field("verified_frame", &OtpCandidate::verified_frame, expected_frame));
}

class TestObserver : public OtpSource::Observer {
 public:
  using Result = base::expected<OtpCandidate, OneTimeTokenRetrievalError>;

  void OnSourceCandidateAvailable(std::optional<OtpTarget> target,
                                  Result candidate_or_error) override {
    last_target_ = std::move(target);
    last_result_ = std::move(candidate_or_error);
  }

  void Reset() {
    last_target_.reset();
    last_result_.reset();
  }

  const std::optional<OtpTarget>& last_target() const { return last_target_; }
  const std::optional<Result>& last_result() const { return last_result_; }

 private:
  std::optional<OtpTarget> last_target_;
  std::optional<Result> last_result_;
};

class GmailOtpSourceTest : public Test {
 protected:
  void SetUp() override {
    prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
    auto service = std::make_unique<FakeGmailOtpService>();
    service_ = service.get();
    client_.set_gmail_otp_service(std::move(service));
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    service_ = nullptr;
    client_.set_gmail_otp_service(nullptr);
  }

  OtpTarget MakeTarget(std::string_view url = "https://a.test") const {
    return MakeTarget(url::Origin::Create(GURL(url)));
  }

  OtpTarget MakeTarget(url::Origin origin) const {
    const LocalFrameToken frame = test::MakeLocalFrameToken();
    return OtpTarget(std::move(origin), frame,
                     FormGlobalId(frame, test::MakeFormRendererId()),
                     FieldGlobalId(frame, test::MakeFieldRendererId()), frame);
  }

  OtpTarget MakeOpaqueTarget() const {
    return MakeTarget(
        url::Origin::Create(GURL("https://a.test")).DeriveNewOpaqueOrigin());
  }

  OneTimeToken MakeToken(
      const std::string& code,
      const std::string& sender = "otp@a.test",
      base::Time email_timestamp = base::Time::Now(),
      base::TimeTicks arrival_ticks = base::TimeTicks::Now()) const {
    return OneTimeToken(OneTimeTokenType::kGmail, code, arrival_ticks, sender,
                        email_timestamp);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  raw_ptr<FakeGmailOtpService> service_ = nullptr;
  TestAutofillClient client_;
  TestObserver observer_;
};

// -----------------------------------------------------------------------------
// 1. Cached OTP emission on `Start()`
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, EmitsCachedTokenOnStart_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(observer_.last_target(), std::nullopt);
  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target.origin, "111111"))));
  EXPECT_THAT(
      source.GetAvailable(target),
      ElementsAre(IsGmailCandidate(target.origin, "111111", target.frame)));
}

TEST_F(GmailOtpSourceTest, EmitsCachedTokenOnStart_TwoOrigins) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target_a.origin, "111111"))));
  EXPECT_THAT(
      source.GetAvailable(target_a),
      ElementsAre(IsGmailCandidate(target_a.origin, "111111", target_a.frame)));
  EXPECT_THAT(source.GetAvailable(target_b), IsEmpty());
}

TEST_F(GmailOtpSourceTest, EmitsCachedTokenOnStart_OpaqueOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(opaque_target);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_->num_get_freshest_cached_otp_calls(), 0u);
  EXPECT_EQ(service_->num_subscribe_calls(), 0u);
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

// -----------------------------------------------------------------------------
// 2. Incoming OTP emission on active subscription
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, EmitsIncomingToken_SingleOrigin) {
  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();

  service_->ProcessCallbacks(MakeToken("111111", "otp@a.test"));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(observer_.last_target(), std::nullopt);
  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target.origin, "111111"))));
  EXPECT_THAT(
      source.GetAvailable(target),
      ElementsAre(IsGmailCandidate(target.origin, "111111", target.frame)));
}

TEST_F(GmailOtpSourceTest, EmitsIncomingToken_TwoOrigins) {
  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  // `otp@a.test` matches `target_a` and triggers
  // `kGmailOtpSenderDomainMismatch` on `target_b`'s subscription, which must be
  // silently ignored rather than overwriting `observer_.last_result()` with an
  // error.
  service_->ProcessCallbacks(MakeToken("111111", "otp@a.test"));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target_a.origin, "111111"))));
  EXPECT_THAT(
      source.GetAvailable(target_a),
      ElementsAre(IsGmailCandidate(target_a.origin, "111111", target_a.frame)));
  EXPECT_THAT(source.GetAvailable(target_b), IsEmpty());
}

TEST_F(GmailOtpSourceTest, EmitsIncomingToken_OpaqueOrigin) {
  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(opaque_target);
  service_->ProcessCallbacks(MakeToken("111111", "otp@a.test"));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_->num_subscribe_calls(), 0u);
  EXPECT_EQ(service_->num_get_freshest_cached_otp_calls(), 0u);
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

// -----------------------------------------------------------------------------
// 3. Fresher token wins over cached token
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, FresherTokenWins_SingleOrigin) {
  const base::Time t0 = base::Time::Now();
  const base::TimeTicks ticks0 = base::TimeTicks::Now();
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test", t0, ticks0)});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();

  service_->ProcessCallbacks(MakeToken("222222", "otp@a.test",
                                       t0 + base::Seconds(10),
                                       ticks0 + base::Seconds(10)));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target.origin, "222222"))));
  EXPECT_THAT(
      source.GetAvailable(target),
      ElementsAre(IsGmailCandidate(target.origin, "222222", target.frame)));
}

TEST_F(GmailOtpSourceTest, FresherTokenWins_TwoOrigins) {
  const base::Time t0 = base::Time::Now();
  const base::TimeTicks ticks0 = base::TimeTicks::Now();
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test", t0, ticks0),
                             MakeToken("222222", "otp@b.test", t0, ticks0)});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  service_->ProcessCallbacks(MakeToken("333333", "otp@a.test",
                                       t0 + base::Seconds(10),
                                       ticks0 + base::Seconds(10)));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      source.GetAvailable(target_a),
      ElementsAre(IsGmailCandidate(target_a.origin, "333333", target_a.frame)));
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

TEST_F(GmailOtpSourceTest, FresherTokenWins_OpaqueOrigin) {
  const base::Time t0 = base::Time::Now();
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test", t0)});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(opaque_target);
  service_->ProcessCallbacks(
      MakeToken("222222", "otp@a.test", t0 + base::Seconds(10)));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

// -----------------------------------------------------------------------------
// 4. Outdated and equal-timestamp tokens are ignored
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, IgnoresOutdatedToken_SingleOrigin) {
  const base::Time t0 = base::Time::Now();
  const base::TimeTicks ticks0 = base::TimeTicks::Now();
  service_->SetCachedTokens({MakeToken("222222", "otp@a.test", t0, ticks0)});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();

  observer_.Reset();
  service_->ProcessCallbacks(MakeToken("333333", "otp@a.test", t0, ticks0));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(
      source.GetAvailable(target),
      ElementsAre(IsGmailCandidate(target.origin, "222222", target.frame)));

  service_->ProcessCallbacks(MakeToken("000000", "otp@a.test",
                                       t0 - base::Seconds(5),
                                       ticks0 + base::Seconds(10)));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(
      source.GetAvailable(target),
      ElementsAre(IsGmailCandidate(target.origin, "222222", target.frame)));
}

TEST_F(GmailOtpSourceTest, IgnoresOutdatedToken_TwoOrigins) {
  const base::Time t0 = base::Time::Now();
  const base::TimeTicks ticks0 = base::TimeTicks::Now();
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test", t0, ticks0),
                             MakeToken("222222", "otp@b.test", t0, ticks0)});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  observer_.Reset();
  service_->ProcessCallbacks(MakeToken("000000", "otp@a.test",
                                       t0 - base::Seconds(5),
                                       ticks0 + base::Seconds(10)));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(
      source.GetAvailable(target_a),
      ElementsAre(IsGmailCandidate(target_a.origin, "111111", target_a.frame)));
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

TEST_F(GmailOtpSourceTest, IgnoresOutdatedToken_OpaqueOrigin) {
  const base::Time t0 = base::Time::Now();
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test", t0)});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(opaque_target);
  service_->ProcessCallbacks(
      MakeToken("000000", "otp@a.test", t0 - base::Seconds(5)));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

// -----------------------------------------------------------------------------
// 5. `MarkConsumed()` excludes consumed tokens from `GetAvailable()`
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, MarkConsumedExcludesToken_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();

  std::vector<OtpCandidate> available = source.GetAvailable(target);
  ASSERT_EQ(available.size(), 1u);
  source.MarkConsumed(available[0].identity);

  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, MarkConsumedExcludesToken_TwoOrigins) {
  const base::Time t0 = base::Time::Now();
  const base::TimeTicks ticks0 = base::TimeTicks::Now();
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test", t0, ticks0),
                             MakeToken("222222", "otp@b.test", t0, ticks0)});

  GmailOtpSource source(client_);
  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(source.GetAvailable(target_a).size(), 1u);
  source.MarkConsumed(source.GetAvailable(target_a)[0].identity);

  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

// -----------------------------------------------------------------------------
// 6. Disabling consent stops subscriptions and clears cache
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest,
       DisablingConsentStopsSubscriptionAndClearsCache_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(service_->num_callbacks(), 1u);
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  observer_.Reset();
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());

  service_->ProcessCallbacks(
      MakeToken("222222", "otp@a.test", base::Time::Now() + base::Seconds(10)));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer_.last_result().has_value());
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
}

TEST_F(GmailOtpSourceTest,
       DisablingConsentStopsSubscriptionAndClearsCache_TwoOrigins) {
  service_->SetCachedTokens(
      {MakeToken("111111", "otp@a.test"), MakeToken("222222", "otp@b.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(service_->num_callbacks(), 2u);

  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(source.GetAvailable(target_b), IsEmpty());
}

// -----------------------------------------------------------------------------
// 7. Enabling consent mid-page starts subscription and emits via observer
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest,
       EnablingConsentMidPageStartsSubscription_SingleOrigin) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  service_->ProcessCallbacks(MakeToken("444444", "otp@a.test"));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_FALSE(observer_.last_result().has_value());

  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_->num_callbacks(), 1u);
  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target.origin, "444444"))));
}

TEST_F(GmailOtpSourceTest,
       EnablingConsentMidPageStartsSubscription_TwoOrigins) {
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), false);
  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  service_->ProcessCallbacks(MakeToken("444444", "otp@a.test"));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_FALSE(observer_.last_result().has_value());

  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_->num_callbacks(), 2u);
  EXPECT_THAT(observer_.last_result(),
              Optional(ValueIs(IsGmailCandidate(target_a.origin, "444444"))));
}

// -----------------------------------------------------------------------------
// 8. `ClearVerifiedCacheForOrigin()` & `ClearAllVerifiedCache()`
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, ClearVerifiedCacheForOrigin_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  source.ClearVerifiedCacheForOrigin(target.origin);
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
  EXPECT_EQ(service_->num_callbacks(), 1u);
}

TEST_F(GmailOtpSourceTest, ClearVerifiedCacheForOrigin_TwoOrigins) {
  service_->SetCachedTokens(
      {MakeToken("111111", "otp@a.test"), MakeToken("222222", "otp@b.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  source.ClearVerifiedCacheForOrigin(target_a.origin);
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

TEST_F(GmailOtpSourceTest, ClearVerifiedCacheForOrigin_OpaqueOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(target_a);
  task_environment_.RunUntilIdle();

  source.ClearVerifiedCacheForOrigin(opaque_target.origin);

  EXPECT_EQ(service_->num_callbacks(), 1u);
  EXPECT_THAT(
      source.GetAvailable(target_a),
      ElementsAre(IsGmailCandidate(target_a.origin, "111111", target_a.frame)));
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, ClearAllVerifiedCache_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  source.ClearAllVerifiedCache();
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
  EXPECT_EQ(service_->num_callbacks(), 1u);
}

TEST_F(GmailOtpSourceTest, ClearAllVerifiedCache_TwoOrigins) {
  service_->SetCachedTokens(
      {MakeToken("111111", "otp@a.test"), MakeToken("222222", "otp@b.test")});

  GmailOtpSource source(client_);
  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  source.ClearAllVerifiedCache();
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(source.GetAvailable(target_b), IsEmpty());
  EXPECT_EQ(service_->num_callbacks(), 2u);
}

// -----------------------------------------------------------------------------
// 9. `Stop()` tears down subscriptions and clears verified cache
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, Stop_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(service_->num_callbacks(), 1u);
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  source.Stop();
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, Stop_TwoOrigins) {
  service_->SetCachedTokens(
      {MakeToken("111111", "otp@a.test"), MakeToken("222222", "otp@b.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(service_->num_callbacks(), 2u);

  source.Stop();
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(source.GetAvailable(target_b), IsEmpty());
}

// -----------------------------------------------------------------------------
// 10. `StopListeningForOrigin()` tears down only the specified origin
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, StopListeningForOrigin_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(service_->num_callbacks(), 1u);

  source.StopListeningForOrigin(target.origin);
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, StopListeningForOrigin_TwoOrigins) {
  service_->SetCachedTokens(
      {MakeToken("111111", "otp@a.test"), MakeToken("222222", "otp@b.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(service_->num_callbacks(), 2u);

  source.StopListeningForOrigin(target_a.origin);
  EXPECT_EQ(service_->num_callbacks(), 1u);
  EXPECT_FALSE(
      source.HasActiveSubscriptionForOriginForTesting(target_a.origin));
  EXPECT_TRUE(source.HasActiveSubscriptionForOriginForTesting(target_b.origin));
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

TEST_F(GmailOtpSourceTest, StopListeningForOrigin_OpaqueOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(target_a);
  task_environment_.RunUntilIdle();

  source.StopListeningForOrigin(opaque_target.origin);
  EXPECT_EQ(service_->num_callbacks(), 1u);
  EXPECT_THAT(
      source.GetAvailable(target_a),
      ElementsAre(IsGmailCandidate(target_a.origin, "111111", target_a.frame)));
}

// -----------------------------------------------------------------------------
// 11. Subscription renewal on repeated `Start()` and across expiration
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest,
       RenewsSubscriptionOnRepeatedStartAndExpiration_SingleOrigin) {
  GmailOtpSource source(client_);
  const OtpTarget target = MakeTarget("https://a.test");

  source.Start(target);
  EXPECT_EQ(service_->num_callbacks(), 1u);

  const base::TimeDelta period =
      one_time_tokens::features::kGmailOtpSubscriptionPeriodParam.Get();

  // Repeated `Start()` renews in place without creating a second subscription.
  task_environment_.FastForwardBy(period - base::Seconds(20));
  source.Start(target);
  EXPECT_EQ(service_->num_callbacks(), 1u);

  // Advancing across multiple expiration periods keeps the subscription alive.
  task_environment_.FastForwardBy(period * 2);
  EXPECT_EQ(service_->num_callbacks(), 1u);
  EXPECT_TRUE(source.HasActiveSubscriptionForOriginForTesting(target.origin));
}

TEST_F(GmailOtpSourceTest,
       RenewsSubscriptionOnRepeatedStartAndExpiration_TwoOrigins) {
  GmailOtpSource source(client_);
  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");

  source.Start(target_a);
  source.Start(target_b);
  EXPECT_EQ(service_->num_callbacks(), 2u);

  const base::TimeDelta period =
      one_time_tokens::features::kGmailOtpSubscriptionPeriodParam.Get();
  task_environment_.FastForwardBy(period * 2);
  EXPECT_EQ(service_->num_callbacks(), 2u);
  EXPECT_TRUE(source.HasActiveSubscriptionForOriginForTesting(target_a.origin));
  EXPECT_TRUE(source.HasActiveSubscriptionForOriginForTesting(target_b.origin));
}

TEST_F(GmailOtpSourceTest,
       RenewsSubscriptionOnRepeatedStartAndExpiration_OpaqueOrigin) {
  GmailOtpSource source(client_);
  const OtpTarget opaque_target = MakeOpaqueTarget();

  source.Start(opaque_target);
  const base::TimeDelta period =
      one_time_tokens::features::kGmailOtpSubscriptionPeriodParam.Get();
  task_environment_.FastForwardBy(period * 2);
  source.Start(opaque_target);

  EXPECT_EQ(service_->num_subscribe_calls(), 0u);
  EXPECT_EQ(service_->num_callbacks(), 0u);
  EXPECT_FALSE(
      source.HasActiveSubscriptionForOriginForTesting(opaque_target.origin));
}

// -----------------------------------------------------------------------------
// 12. Token expiration after `kGmailOtpCacheDuration`
// -----------------------------------------------------------------------------

TEST_F(GmailOtpSourceTest, ExpiresCachedTokenAfterCacheDuration_SingleOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  task_environment_.FastForwardBy(kGmailOtpCacheDuration + base::Seconds(1));
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, ExpiresCachedTokenAfterCacheDuration_TwoOrigins) {
  const base::Time t0 = base::Time::Now();
  const base::TimeTicks ticks0 = base::TimeTicks::Now();
  service_->SetCachedTokens(
      {MakeToken("111111", "otp@a.test", t0, ticks0),
       MakeToken("222222", "otp@b.test", t0 + base::Seconds(20),
                 ticks0 + base::Seconds(20))});

  GmailOtpSource source(client_);
  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  // Advance so `target_a`'s cached token expires while `target_b`'s cached
  // token is still within `kGmailOtpCacheDuration`.
  task_environment_.FastForwardBy(kGmailOtpCacheDuration + base::Seconds(10));
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

TEST_F(GmailOtpSourceTest, ExpiresCachedTokenAfterCacheDuration_OpaqueOrigin) {
  service_->SetCachedTokens({MakeToken("111111", "otp@a.test")});

  GmailOtpSource source(client_);
  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(opaque_target);
  task_environment_.FastForwardBy(kGmailOtpCacheDuration + base::Seconds(1));

  EXPECT_EQ(service_->num_get_freshest_cached_otp_calls(), 0u);
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

TEST_F(GmailOtpSourceTest,
       ExpiresIncomingTokenAfterCacheDuration_SingleOrigin) {
  GmailOtpSource source(client_);
  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  task_environment_.RunUntilIdle();

  service_->ProcessCallbacks(MakeToken("111111", "otp@a.test"));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(source.GetAvailable(target).size(), 1u);

  task_environment_.FastForwardBy(kGmailOtpCacheDuration + base::Seconds(1));
  EXPECT_THAT(source.GetAvailable(target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, ExpiresIncomingTokenAfterCacheDuration_TwoOrigins) {
  GmailOtpSource source(client_);
  const OtpTarget target_a = MakeTarget("https://a.test");
  const OtpTarget target_b = MakeTarget("https://b.test");
  source.Start(target_a);
  source.Start(target_b);
  task_environment_.RunUntilIdle();

  service_->ProcessCallbacks(MakeToken("111111", "otp@a.test"));
  task_environment_.FastForwardBy(base::Seconds(20));
  service_->ProcessCallbacks(MakeToken("222222", "otp@b.test"));
  task_environment_.RunUntilIdle();

  // Advance so `target_a`'s incoming token expires while `target_b`'s incoming
  // token is still within `kGmailOtpCacheDuration`.
  task_environment_.FastForwardBy(kGmailOtpCacheDuration - base::Seconds(10));
  EXPECT_THAT(source.GetAvailable(target_a), IsEmpty());
  EXPECT_THAT(
      source.GetAvailable(target_b),
      ElementsAre(IsGmailCandidate(target_b.origin, "222222", target_b.frame)));
}

TEST_F(GmailOtpSourceTest,
       ExpiresIncomingTokenAfterCacheDuration_OpaqueOrigin) {
  GmailOtpSource source(client_);
  const OtpTarget opaque_target = MakeOpaqueTarget();
  source.Start(opaque_target);
  service_->ProcessCallbacks(MakeToken("111111", "otp@a.test"));
  task_environment_.FastForwardBy(kGmailOtpCacheDuration + base::Seconds(1));

  EXPECT_EQ(service_->num_subscribe_calls(), 0u);
  EXPECT_THAT(source.GetAvailable(opaque_target), IsEmpty());
}

TEST_F(GmailOtpSourceTest, ForwardsBackendRetrievalErrorToObserver) {
  GmailOtpSource source(client_);
  source.SetObserver(&observer_);

  const OtpTarget target = MakeTarget("https://a.test");
  source.Start(target);
  service_->ProcessCallbacks(
      base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(observer_.last_target(), std::nullopt);
  EXPECT_THAT(observer_.last_result(),
              Optional(ErrorIs(OneTimeTokenRetrievalError::kGmailOtpUnknown)));
}

TEST_F(GmailOtpSourceTest, RecordsSenderDomainMatchUkmAgainstPage) {
  using UkmEntry = ukm::builders::OneTimeTokens;
  affiliations::FakeAffiliationService affiliation_service;
  one_time_tokens::FakeGmailOtpBackend backend;
  service_ = nullptr;
  client_.set_gmail_otp_service(
      std::make_unique<one_time_tokens::GmailOtpServiceImpl>(
          &backend, &affiliation_service, std::nullopt));

  auto owned_driver = std::make_unique<TestAutofillDriver>(&client_);
  owned_driver->set_autofill_manager(
      std::make_unique<TestBrowserAutofillManager>(owned_driver.get()));
  TestAutofillDriver& driver =
      client_.GetAutofillDriverFactory().TakeOwnership(std::move(owned_driver));

  {
    GmailOtpSource source(client_);
    source.SetObserver(&observer_);

    const OtpTarget target = MakeTarget("https://a.test");
    source.Start(target);
    backend.ProcessCallbacks(MakeToken("654321", "otp@mail.a.test"));
    task_environment_.RunUntilIdle();

    EXPECT_THAT(observer_.last_result(),
                Optional(ValueIs(IsGmailCandidate(target.origin, "654321"))));
  }

  auto entries =
      client_.GetUkmRecorder()->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id, driver.GetPageUkmSourceId());
  client_.GetUkmRecorder()->ExpectEntryMetric(
      entries[0],
      UkmEntry::kGmail_SenderDomainMatchAcceptedMatchType_ReceivedName,
      std::to_underlying(one_time_tokens::GmailOtpSenderDomainMatchType::kPsl));

  client_.set_gmail_otp_service(nullptr);
}

}  // namespace
}  // namespace autofill
