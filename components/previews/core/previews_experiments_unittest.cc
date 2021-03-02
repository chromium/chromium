// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/previews/core/previews_features.h"
#include "components/variations/variations_associated_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const char kPreviewsFieldTrial[] = "Previews";

// Verifies that the default params are correct, and that custom params can be
// set,
TEST(PreviewsExperimentsTest, TestParams) {
  // Verify that the default params are correct.
  EXPECT_EQ(4u, params::MaxStoredHistoryLengthForPerHostBlockList());
  EXPECT_EQ(10u, params::MaxStoredHistoryLengthForHostIndifferentBlockList());
  EXPECT_EQ(100u, params::MaxInMemoryHostsInBlockList());
  EXPECT_EQ(2, params::PerHostBlockListOptOutThreshold());
  EXPECT_EQ(6, params::HostIndifferentBlockListOptOutThreshold());
  EXPECT_EQ(base::TimeDelta::FromDays(30), params::PerHostBlockListDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(30),
            params::HostIndifferentBlockListPerHostDuration());
  EXPECT_EQ(base::TimeDelta::FromSeconds(60 * 5),
            params::SingleOptOutDuration());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::GetECTThresholdForPreview(PreviewsType::DEFER_ALL_SCRIPT));
}

TEST(PreviewsExperimentsTest, TestDefaultShouldExcludeMediaSuffix) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExcludedMediaSuffixes);

  EXPECT_FALSE(
      params::ShouldExcludeMediaSuffix(GURL("http://chromium.org/path/")));

  std::vector<std::string> default_suffixes = {
      ".apk", ".avi",  ".gif", ".gifv", ".jpeg", ".jpg", ".mp3",
      ".mp4", ".mpeg", ".pdf", ".png",  ".webm", ".webp"};
  for (const std::string& suffix : default_suffixes) {
    GURL url("http://chromium.org/path/" + suffix);
    EXPECT_TRUE(params::ShouldExcludeMediaSuffix(url));
  }
}

TEST(PreviewsExperimentsTest, TestShouldExcludeMediaSuffix) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    std::string varaiation_value;
    std::vector<std::string> urls;
    bool want_return;
  };
  const TestCase kTestCases[]{
      {
          .msg = "Feature disabled, should always return false",
          .enable_feature = false,
          .varaiation_value = "",
          .urls = {"http://chromium.org/video.mp4"},
          .want_return = false,
      },
      {
          .msg = "Default values are overridden by variations",
          .enable_feature = true,
          .varaiation_value = ".html",
          .urls = {"http://chromium.org/video.mp4",
                   "http://chromium.org/image.png",
                   "http://chromium.org/image.jpg",
                   "http://chromium.org/audio.mp3"},
          .want_return = false,
      },
      {
          .msg = "Variation value whitespace should be trimmed",
          .enable_feature = true,
          .varaiation_value = " .mp4 , \t .png\n",
          .urls = {"http://chromium.org/video.mp4",
                   "http://chromium.org/image.png"},
          .want_return = true,
      },
      {
          .msg = "Variation value empty values should be excluded",
          .enable_feature = true,
          .varaiation_value = ".mp4,,.png,",
          .urls = {"http://chromium.org/video.mp4",
                   "http://chromium.org/image.png"},
          .want_return = true,
      },
      {
          .msg = "URLs should be compared case insensitive",
          .enable_feature = true,
          .varaiation_value = ".MP4,.png,",
          .urls = {"http://chromium.org/video.mP4",
                   "http://chromium.org/image.PNG"},
          .want_return = true,
      },
      {
          .msg = "Query params and fragments don't matter",
          .enable_feature = true,
          .varaiation_value = ".mp4,.png,",
          .urls = {"http://chromium.org/video.mp4?hello=world",
                   "http://chromium.org/image.png#test"},
          .want_return = true,
      },
      {
          .msg = "Query params and fragments shouldn't be considered",
          .enable_feature = true,
          .varaiation_value = ".mp4,.png,",
          .urls = {"http://chromium.org/?video=video.mp4",
                   "http://chromium.org/#image.png"},
          .want_return = false,
      },
  };
  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.enable_feature) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kExcludedMediaSuffixes,
          {{"excluded_path_suffixes", test_case.varaiation_value}});
    } else {
      scoped_feature_list.InitAndDisableFeature(
          features::kExcludedMediaSuffixes);
    }

    for (const std::string& url : test_case.urls) {
      EXPECT_EQ(test_case.want_return,
                params::ShouldExcludeMediaSuffix(GURL(url)));
    }
  }
}

TEST(PreviewsExperimentsTest, TestDeferAllScriptPreviewsCoinFlipExperiment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kDeferAllScriptPreviews, {{"version", "444"}}}}, {});

  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kPreviewsFieldTrial, "DeferAllScriptPreviewsCoinFlipExperimentGroup"));

  EXPECT_TRUE(params::IsDeferAllScriptPreviewsEnabled());
  EXPECT_EQ(444, params::DeferAllScriptPreviewsVersion());
}

TEST(PreviewsExperimentsTest, TestOverrideShouldShowPreviewCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPreviews, {{"override_should_show_preview_check", "true"}}}},
      {});

  EXPECT_TRUE(params::OverrideShouldShowPreviewCheck());
}

}  // namespace

}  // namespace previews
