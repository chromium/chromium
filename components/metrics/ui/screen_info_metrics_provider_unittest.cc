// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/ui/screen_info_metrics_provider.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "ui/gfx/geometry/size.h"

namespace metrics {

namespace {

const int kScreenWidth = 1024;
const int kScreenHeight = 768;
const int kScreenCount = 3;
const float kScreenScaleFactor = 2;

class TestScreenInfoMetricsProvider : public ScreenInfoMetricsProvider {
 public:
  TestScreenInfoMetricsProvider() {}
  ~TestScreenInfoMetricsProvider() override {}

 private:
  base::Optional<gfx::Size> GetScreenSize() const override {
    return base::make_optional(gfx::Size(kScreenWidth, kScreenHeight));
  }

  float GetScreenDeviceScaleFactor() const override {
    return kScreenScaleFactor;
  }

  int GetScreenCount() const override { return kScreenCount; }

  DISALLOW_COPY_AND_ASSIGN(TestScreenInfoMetricsProvider);
};

}  // namespace

class ScreenInfoMetricsProviderTest : public testing::Test {
 public:
  ScreenInfoMetricsProviderTest() {}
  ~ScreenInfoMetricsProviderTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenInfoMetricsProviderTest);
};

TEST_F(ScreenInfoMetricsProviderTest, ProvideSystemProfileMetrics) {
  TestScreenInfoMetricsProvider provider;
  ChromeUserMetricsExtension uma_proto;

  provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());

  // Check that the system profile has the correct values set.
  const SystemProfileProto::Hardware& hardware =
      uma_proto.system_profile().hardware();
  EXPECT_EQ(kScreenWidth, hardware.primary_screen_width());
  EXPECT_EQ(kScreenHeight, hardware.primary_screen_height());
  EXPECT_EQ(kScreenScaleFactor, hardware.primary_screen_scale_factor());
  EXPECT_EQ(kScreenCount, hardware.screen_count());
}

}  // namespace metrics
