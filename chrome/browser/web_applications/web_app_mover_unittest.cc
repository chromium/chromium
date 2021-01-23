// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_mover.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class WebAppMoverTestWithPrefixParams
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::pair<const char*, const char*>> {
 public:
  WebAppMoverTestWithPrefixParams() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kMoveWebApp,
          {{features::kMoveWebAppUninstallStartUrlPrefix.name,
            GetParam().first},
           {features::kMoveWebAppInstallStartUrl.name, GetParam().second}}}},
        {});
  }
  ~WebAppMoverTestWithPrefixParams() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using WebAppMoverTestWithInvalidPrefixParams = WebAppMoverTestWithPrefixParams;

TEST_P(WebAppMoverTestWithInvalidPrefixParams, VerifyInvalidParams) {
  std::unique_ptr<WebAppMover> mover =
      WebAppMover::CreateIfNeeded(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_FALSE(mover);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidInputs,
    WebAppMoverTestWithInvalidPrefixParams,
    ::testing::Values(
        std::make_pair("", ""),
        std::make_pair("test", "test"),
        std::make_pair("www.google.com/a", "www.google.com/b"),
        std::make_pair("https://www.google.com/a", "https://www.google.com/a"),
        std::make_pair("https://www.google.com/", "https://www.google.com/a"),
        std::make_pair("https://www.google.com/foo",
                       "https://www.google.com/foobar")));

using WebAppMoverTestWithValidPrefixParams = WebAppMoverTestWithPrefixParams;

TEST_P(WebAppMoverTestWithValidPrefixParams, VerifyValidParams) {
  std::unique_ptr<WebAppMover> mover =
      WebAppMover::CreateIfNeeded(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_TRUE(mover);
}

INSTANTIATE_TEST_SUITE_P(
    ValidInputs,
    WebAppMoverTestWithValidPrefixParams,
    ::testing::Values(std::make_pair("https://www.google.com/a",
                                     "https://www.google.com/b")));

class WebAppMoverTestWithPatternParams
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::pair<const char*, const char*>> {
 public:
  WebAppMoverTestWithPatternParams() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kMoveWebApp,
          {{features::kMoveWebAppUninstallStartUrlPattern.name,
            GetParam().first},
           {features::kMoveWebAppInstallStartUrl.name, GetParam().second}}}},
        {});
  }
  ~WebAppMoverTestWithPatternParams() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using WebAppMoverTestWithInvalidPatternParams =
    WebAppMoverTestWithPatternParams;

TEST_P(WebAppMoverTestWithInvalidPatternParams, VerifyInvalidParams) {
  std::unique_ptr<WebAppMover> mover =
      WebAppMover::CreateIfNeeded(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_FALSE(mover);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidInputs,
    WebAppMoverTestWithInvalidPatternParams,
    ::testing::Values(std::make_pair("", ""),
                      std::make_pair("test", ""),
                      std::make_pair("www.google.com/a", ""),
                      std::make_pair("https://www.google.com/a",
                                     "https://www.google.com/a"),
                      std::make_pair("https://www\\.google\\.com/.*",
                                     "https://www.google.com/a"),
                      std::make_pair("https://www\\.google\\.com/foo.*",
                                     "https://www.google.com/foobar"),
                      std::make_pair(".*", "https://www.google.com/foobar"),
                      std::make_pair("https://www\\.google\\.com/[a-z]+",
                                     "https://www.google.com/foobar")));

using WebAppMoverTestWithValidPatternParams = WebAppMoverTestWithPatternParams;

TEST_P(WebAppMoverTestWithValidPatternParams, VerifyValidParams) {
  std::unique_ptr<WebAppMover> mover =
      WebAppMover::CreateIfNeeded(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_TRUE(mover);
}

INSTANTIATE_TEST_SUITE_P(
    ValidInputs,
    WebAppMoverTestWithValidPatternParams,
    ::testing::Values(std::make_pair("https://www\\.google\\.com/a.*",
                                     "https://www.google.com/b"),
                      std::make_pair("https://www\\.google\\.com/[ac].*",
                                     "https://www.google.com/b")));

}  // namespace
}  // namespace web_app
