// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "realbox_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class RealboxHandlerTest : public ::testing::Test {
 public:
  RealboxHandlerTest() = default;

  RealboxHandlerTest(const RealboxHandlerTest&) = delete;
  RealboxHandlerTest& operator=(const RealboxHandlerTest&) = delete;
  ~RealboxHandlerTest() override = default;

  content::TestWebUIDataSource* source() { return source_.get(); }
  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestWebUIDataSource> source_;
  std::unique_ptr<TestingProfile> profile_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  void SetUp() override {
    source_ = content::TestWebUIDataSource::Create("test-data-source");

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();

    ASSERT_EQ(
        variations::VariationsIdsProvider::ForceIdsResult::SUCCESS,
        variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(
            /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));
  }
};

TEST_F(RealboxHandlerTest, RealboxLensSearchIsFalseWhenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{},
      /*disabled_features=*/{ntp_features::kNtpRealboxLensSearch});

  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_FALSE(
      source()->GetLocalizedStrings()->FindBool("realboxLensSearch").value());
}

TEST_F(RealboxHandlerTest, RealboxLensSearchIsTrueWhenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{ntp_features::kNtpRealboxLensSearch, {{}}}},
      /*disabled_features=*/{});

  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_TRUE(
      source()->GetLocalizedStrings()->FindBool("realboxLensSearch").value());
}

TEST_F(RealboxHandlerTest, RealboxLensVariationsContainsVariations) {
  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_EQ("CGQ", *source()->GetLocalizedStrings()->FindString(
                       "realboxLensVariations"));
}
