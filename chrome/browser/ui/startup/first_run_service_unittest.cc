// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include "base/strings/stringprintf.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "Unsupported platform"
#endif

struct FirstRunFieldTrialTestParams {
  double entropy_value;
  version_info::Channel channel;

  bool expect_study_enabled;
  bool expect_feature_enabled;
};

class FirstRunFieldTrialCreatorTest
    : public testing::Test,
      public testing::WithParamInterface<FirstRunFieldTrialTestParams> {
 public:
  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(FirstRunFieldTrialCreatorTest, SetUpFromClientSide) {
  {
    base::MockEntropyProvider low_entropy_provider{GetParam().entropy_value};
    auto feature_list = std::make_unique<base::FeatureList>();

    FirstRunService::SetUpClientSideFieldTrial(
        low_entropy_provider, feature_list.get(), GetParam().channel);

    // Substitute the existing feature list with the one with field trial
    // configurations we are testing, so we can check the assertions.
    scoped_feature_list().InitWithFeatureList(std::move(feature_list));
  }

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("ForYouFreStudy"));

  EXPECT_EQ(GetParam().expect_study_enabled,
            base::FeatureList::IsEnabled(kForYouFreSyntheticTrialRegistration));
  EXPECT_EQ(GetParam().expect_feature_enabled,
            base::FeatureList::IsEnabled(kForYouFre));

  EXPECT_EQ(true, kForYouFreCloseShouldProceed.Get());
  EXPECT_EQ(SigninPromoVariant::kSignIn, kForYouFreSignInPromoVariant.Get());
  EXPECT_EQ(GetParam().expect_study_enabled
                ? (GetParam().expect_feature_enabled ? "ClientSideEnabled"
                                                     : "ClientSideDisabled")
                : "",
            kForYouFreStudyGroup.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FirstRunFieldTrialCreatorTest,
    testing::Values(
        FirstRunFieldTrialTestParams{.entropy_value = 0.6,
                                     .channel = version_info::Channel::BETA,
                                     .expect_study_enabled = true,
                                     .expect_feature_enabled = false},
        FirstRunFieldTrialTestParams{.entropy_value = 0.01,
                                     .channel = version_info::Channel::BETA,
                                     .expect_study_enabled = true,
                                     .expect_feature_enabled = true},
        FirstRunFieldTrialTestParams{.entropy_value = 0.99,
                                     .channel = version_info::Channel::STABLE,
                                     .expect_study_enabled = false,
                                     .expect_feature_enabled = false},
        FirstRunFieldTrialTestParams{.entropy_value = 0.01,
                                     .channel = version_info::Channel::STABLE,
                                     .expect_study_enabled = false,
                                     .expect_feature_enabled = false}),

    [](const ::testing::TestParamInfo<FirstRunFieldTrialTestParams>& params) {
      return base::StringPrintf(
          "%02.0fpctEntropy%s", params.param.entropy_value * 100,
          version_info::GetChannelString(params.param.channel).c_str());
    });
