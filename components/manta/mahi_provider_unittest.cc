// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/mahi_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/types/expected.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {
namespace {

constexpr char kMockEndpoint[] = "https://my-endpoint.com";

// `MahiProvider` test double that routes `RequestInternal` to `kMockEndpoint`.
class FakeMahiProvider : public MahiProvider {
 public:
  FakeMahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : MahiProvider(test_url_loader_factory,
                     identity_manager,
                     ProviderParams()) {}

 protected:
  // MahiProvider:
  void RequestInternal(const GURL& url,
                       const net::NetworkTrafficAnnotationTag& annotation_tag,
                       manta::proto::Request& request,
                       const MantaMetricType metric_type,
                       MantaProtoResponseCallback done_callback,
                       const base::TimeDelta timeout) override {
    MahiProvider::RequestInternal(GURL{kMockEndpoint}, annotation_tag, request,
                                  metric_type, std::move(done_callback),
                                  timeout);
  }
};

// `MagicBoostState` test double for configuring Help Me Read preconditions.
class FakeMagicBoostState : public chromeos::MagicBoostState {
 public:
  FakeMagicBoostState() {
    UpdateMagicBoostEnabled(true);
    UpdateHMREnabled(true);
    UpdateHMRConsentStatus(chromeos::HMRConsentStatus::kApproved);
  }

  ~FakeMagicBoostState() override = default;

  void SetUserEligible(base::expected<bool, Error> eligible) {
    expected_eligibility_ = eligible;
    if (eligible.has_value()) {
      UpdateUserEligibleForGenAIFeatures(eligible.value());
    }
  }

  // chromeos::MagicBoostState:
  bool CanShowNoticeBannerForHMR() override { return false; }

  int32_t AsyncIncrementHMRConsentWindowDismissCount() override { return 0; }

  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override {
    UpdateHMRConsentStatus(consent_status);
  }

  void AsyncWriteHMREnabled(bool enabled) override {
    UpdateHMREnabled(enabled);
  }

  bool ShouldIncludeOrcaInOptInSync() override { return false; }

  void DisableOrcaFeature() override {}

  void DisableLobsterSettings() override {}

  void SetMagicBoostEnabledForTest(bool enabled) {
    UpdateMagicBoostEnabled(enabled);
  }

 protected:
  base::expected<bool, Error> IsUserEligibleForGenAIFeaturesExpected()
      const override {
    return expected_eligibility_;
  }

 private:
  base::expected<bool, Error> expected_eligibility_ = base::ok(true);
};

}  // namespace

class MahiProviderTest : public BaseProviderTest {
 public:
  MahiProviderTest() = default;
  MahiProviderTest(const MahiProviderTest&) = delete;
  MahiProviderTest& operator=(const MahiProviderTest&) = delete;
  ~MahiProviderTest() override = default;

  void SetUp() override {
    BaseProviderTest::SetUp();
    magic_boost_state_ = std::make_unique<FakeMagicBoostState>();
    magic_boost_state_->SetUserEligible(base::ok(true));
  }

  void TearDown() override {
    magic_boost_state_.reset();
    BaseProviderTest::TearDown();
  }

  std::unique_ptr<FakeMahiProvider> CreateMahiProvider() {
    return std::make_unique<FakeMahiProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }

 protected:
  std::unique_ptr<FakeMagicBoostState> magic_boost_state_;
};

TEST_F(MahiProviderTest, RequestsSucceedWhenPreconditionsMet) {
  proto::Response response;
  response.add_output_data()->set_text("response text");
  SetEndpointMockResponse(GURL{kMockEndpoint}, response.SerializeAsString(),
                          net::HTTP_OK, net::OK);

  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();

  {
    base::RunLoop run_loop;
    mahi_provider->Summarize(
        "input", "title", "context", "https://example.com",
        base::BindLambdaForTesting(
            [&run_loop](base::DictValue dict, MantaStatus status) {
              EXPECT_EQ(status.status_code, MantaStatusCode::kOk);
              const std::string* output = dict.FindString("outputData");
              ASSERT_TRUE(output);
              EXPECT_EQ(*output, "response text");
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    mahi_provider->Elucidate(
        "input", "context", "title", "https://example.com",
        base::BindLambdaForTesting(
            [&run_loop](base::DictValue dict, MantaStatus status) {
              EXPECT_EQ(status.status_code, MantaStatusCode::kOk);
              const std::string* output = dict.FindString("outputData");
              ASSERT_TRUE(output);
              EXPECT_EQ(*output, "response text");
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    mahi_provider->QuestionAndAnswer(
        "content", "title", "https://example.com", {{"prev_q", "prev_a"}},
        "question",
        base::BindLambdaForTesting(
            [&run_loop](base::DictValue dict, MantaStatus status) {
              EXPECT_EQ(status.status_code, MantaStatusCode::kOk);
              const std::string* output = dict.FindString("outputData");
              ASSERT_TRUE(output);
              EXPECT_EQ(*output, "response text");
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

class MahiProviderDeathTest : public MahiProviderTest {
 protected:
  void SetUp() override {
    GTEST_FLAG_SET(death_test_style, "fast");
    MahiProviderTest::SetUp();
  }

 private:
  base::debug::OverrideStackTraceOutputForTesting suppress_stack_traces_{
      base::debug::OverrideStackTraceOutputForTesting::Mode::kSuppressOutput};
};

TEST_F(MahiProviderDeathTest, CrashesWhenMagicBoostStateIsNull) {
  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();
  magic_boost_state_.reset();

  EXPECT_CHECK_DEATH(mahi_provider->Summarize("input", "title", std::nullopt,
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->Elucidate("input", "context", "title",
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->QuestionAndAnswer(
      "content", "title", std::nullopt, {}, "question", base::DoNothing()));
}

TEST_F(MahiProviderDeathTest, CrashesWhenUserIsIneligibleMinor) {
  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();
  magic_boost_state_->SetUserEligible(base::ok(false));

  EXPECT_CHECK_DEATH(mahi_provider->Summarize("input", "title", std::nullopt,
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->Elucidate("input", "context", "title",
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->QuestionAndAnswer(
      "content", "title", std::nullopt, {}, "question", base::DoNothing()));
}

TEST_F(MahiProviderDeathTest, CrashesWhenEligibilityIsUninitialized) {
  magic_boost_state_.reset();
  magic_boost_state_ = std::make_unique<FakeMagicBoostState>();
  magic_boost_state_->SetUserEligible(
      base::unexpected(chromeos::MagicBoostState::Error::kUninitialized));
  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();

  EXPECT_CHECK_DEATH(mahi_provider->Summarize("input", "title", std::nullopt,
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->Elucidate("input", "context", "title",
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->QuestionAndAnswer(
      "content", "title", std::nullopt, {}, "question", base::DoNothing()));
}

TEST_F(MahiProviderDeathTest, CrashesWhenMagicBoostIsDisabled) {
  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();
  magic_boost_state_->SetMagicBoostEnabledForTest(false);

  EXPECT_CHECK_DEATH(mahi_provider->Summarize("input", "title", std::nullopt,
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->Elucidate("input", "context", "title",
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->QuestionAndAnswer(
      "content", "title", std::nullopt, {}, "question", base::DoNothing()));
}

TEST_F(MahiProviderDeathTest, CrashesWhenHmrIsDisabled) {
  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();
  magic_boost_state_->AsyncWriteHMREnabled(false);

  EXPECT_CHECK_DEATH(mahi_provider->Summarize("input", "title", std::nullopt,
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->Elucidate("input", "context", "title",
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->QuestionAndAnswer(
      "content", "title", std::nullopt, {}, "question", base::DoNothing()));
}

TEST_F(MahiProviderDeathTest, CrashesWhenConsentIsNotApproved) {
  std::unique_ptr<MahiProvider> mahi_provider = CreateMahiProvider();
  magic_boost_state_->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kPendingDisclaimer);

  EXPECT_CHECK_DEATH(mahi_provider->Summarize("input", "title", std::nullopt,
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->Elucidate("input", "context", "title",
                                              std::nullopt, base::DoNothing()));
  EXPECT_CHECK_DEATH(mahi_provider->QuestionAndAnswer(
      "content", "title", std::nullopt, {}, "question", base::DoNothing()));
}

}  // namespace manta
