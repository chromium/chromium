// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/base/device_form_factor.h"
#endif

class GoogleURLLoaderThrottleTest : public testing::Test {
 public:
  GoogleURLLoaderThrottleTest() {
    scoped_feature_list_.InitWithFeatureList(
        std::make_unique<base::FeatureList>());
  }

  ~GoogleURLLoaderThrottleTest() override = default;

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_ANDROID)

TEST_F(GoogleURLLoaderThrottleTest, RequestDesktopHeaderForLargeScreen) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitAndEnableFeature(
      features::kRequestDesktopSiteForTablets);
  GoogleURLLoaderThrottle throttle(/* client_header= */ "",
                                   /* is_tab_large_enough= */ true,
                                   chrome::mojom::DynamicParams());

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  // Only set header for tablets, not for phone.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    std::string isTablet;
    EXPECT_TRUE(request.headers.GetHeader("X-Eligible-Tablet", &isTablet));
    EXPECT_EQ(isTablet, "1");
  } else {
    EXPECT_FALSE(request.headers.HasHeader("X-Eligible-Tablet"));
  }
}

TEST_F(GoogleURLLoaderThrottleTest, RequestDesktopHeaderForSmallScreen) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitAndEnableFeature(
      features::kRequestDesktopSiteForTablets);
  GoogleURLLoaderThrottle throttle(/* client_header= */ "",
                                   /* is_tab_large_enough= */ false,
                                   chrome::mojom::DynamicParams());

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  // Only set header for tablets, not for phone.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    std::string isTablet;
    EXPECT_TRUE(request.headers.GetHeader("X-Eligible-Tablet", &isTablet));
    EXPECT_EQ(isTablet, "0");
  } else {
    EXPECT_FALSE(request.headers.HasHeader("X-Eligible-Tablet"));
  }
}
#endif
