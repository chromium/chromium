// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"

namespace web_app {

enum class ProviderType { kBookmarkApps, kWebApps };

class WebAppProviderUnitTest
    : public WebAppTest,
      public ::testing::WithParamInterface<ProviderType> {
 public:
  WebAppProviderUnitTest() {
    if (GetParam() == ProviderType::kWebApps) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }
  ~WebAppProviderUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = WebAppProvider::Get(profile());
  }

  WebAppProvider* provider() { return provider_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  WebAppProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(WebAppProviderUnitTest);
};

TEST_P(WebAppProviderUnitTest, Registrar) {
  AppRegistrar& registrar = provider()->registrar();
  EXPECT_FALSE(registrar.IsInstalled("unknown"));
}

INSTANTIATE_TEST_SUITE_P(,
                         WebAppProviderUnitTest,
                         ::testing::ValuesIn({ProviderType::kBookmarkApps,
                                              ProviderType::kWebApps}));

}  // namespace web_app
