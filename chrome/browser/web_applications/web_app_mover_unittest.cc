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

class WebAppMoverTestWithParams : public ::testing::Test,
                                  public ::testing::WithParamInterface<
                                      std::pair<const char*, const char*>> {
 public:
  WebAppMoverTestWithParams() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kMoveWebApp,
          {{features::kMoveWebAppUninstallStartUrlPrefix.name,
            GetParam().first},
           {features::kMoveWebAppInstallStartUrl.name, GetParam().second}}}},
        {});
  }
  ~WebAppMoverTestWithParams() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using WebAppMoverTestWithInvalidParams = WebAppMoverTestWithParams;

TEST_P(WebAppMoverTestWithInvalidParams, VerifyInvalidParams) {
  std::unique_ptr<WebAppMover> mover =
      WebAppMover::CreateIfNeeded(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_FALSE(mover);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidInputs,
    WebAppMoverTestWithInvalidParams,
    ::testing::Values(
        std::make_pair("", ""),
        std::make_pair("test", "test"),
        std::make_pair("www.google.com/a", "www.google.com/b"),
        std::make_pair("https://www.google.com/a", "https://www.google.com/a"),
        std::make_pair("https://www.google.com/", "https://www.google.com/a"),
        std::make_pair("https://www.google.com/foo",
                       "https://www.google.com/foobar")));

using WebAppMoverTestWithValidParams = WebAppMoverTestWithParams;

TEST_P(WebAppMoverTestWithValidParams, VerifyValidParams) {
  std::unique_ptr<WebAppMover> mover =
      WebAppMover::CreateIfNeeded(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_TRUE(mover);
}

INSTANTIATE_TEST_SUITE_P(
    ValidInputs,
    WebAppMoverTestWithValidParams,
    ::testing::Values(std::make_pair("https://www.google.com/a",
                                     "https://www.google.com/b")));

}  // namespace
}  // namespace web_app
