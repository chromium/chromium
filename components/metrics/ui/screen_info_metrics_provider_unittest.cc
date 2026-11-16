// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/ui/screen_info_metrics_provider.h"

#include "build/build_config.h"
#include "components/metrics/early_metrics_guard.h"
#include "components/metrics/environment_recorder.h"
#include "components/prefs/testing_pref_service.h"
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
  TestScreenInfoMetricsProvider() = default;

  TestScreenInfoMetricsProvider(const TestScreenInfoMetricsProvider&) = delete;
  TestScreenInfoMetricsProvider& operator=(
      const TestScreenInfoMetricsProvider&) = delete;

  ~TestScreenInfoMetricsProvider() override = default;

  void set_screen_available(bool screen_available) {
    screen_available_ = screen_available;
  }

 private:
  std::optional<gfx::Size> GetScreenSize() const override {
    if (!screen_available_) {
      return std::nullopt;
    }
    return std::make_optional(gfx::Size(kScreenWidth, kScreenHeight));
  }

  float GetScreenDeviceScaleFactor() const override {
    return kScreenScaleFactor;
  }

  int GetScreenCount() const override { return kScreenCount; }

  bool screen_available_ = true;
};

}  // namespace

class ScreenInfoMetricsProviderTest : public testing::Test {
 public:
  ScreenInfoMetricsProviderTest() = default;

  ScreenInfoMetricsProviderTest(const ScreenInfoMetricsProviderTest&) = delete;
  ScreenInfoMetricsProviderTest& operator=(
      const ScreenInfoMetricsProviderTest&) = delete;

  ~ScreenInfoMetricsProviderTest() override = default;
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

TEST_F(ScreenInfoMetricsProviderTest,
       ProvideSystemProfileMetrics_NotInitialized) {
  TestScreenInfoMetricsProvider provider;
  provider.set_screen_available(false);
  ChromeUserMetricsExtension uma_proto;

  provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());

  // Outside early startup, nothing is written until screen info is available.
  const SystemProfileProto::Hardware& hardware =
      uma_proto.system_profile().hardware();
  EXPECT_FALSE(hardware.has_primary_screen_width());
  EXPECT_FALSE(hardware.has_primary_screen_height());
  EXPECT_FALSE(hardware.has_primary_screen_scale_factor());
  EXPECT_FALSE(hardware.has_screen_count());
}

TEST_F(ScreenInfoMetricsProviderTest, ReadsFromCacheWhenEarlyMetrics) {
  TestingPrefServiceSimple local_state;
  EnvironmentRecorder::RegisterPrefs(local_state.registry());

  constexpr int kCachedScreenWidth = 800;
  constexpr int kCachedScreenHeight = 600;
  constexpr float kCachedScreenScaleFactor = 1.5f;
  constexpr int kCachedScreenCount = 1;

  SystemProfileProto cached_profile;
  auto* cached_hardware = cached_profile.mutable_hardware();
  cached_hardware->set_primary_screen_width(kCachedScreenWidth);
  cached_hardware->set_primary_screen_height(kCachedScreenHeight);
  cached_hardware->set_primary_screen_scale_factor(kCachedScreenScaleFactor);
  cached_hardware->set_screen_count(kCachedScreenCount);
#if BUILDFLAG(IS_WIN)
  constexpr float kCachedMaxDpiX = 96.0f;
  constexpr float kCachedMaxDpiY = 120.0f;
  cached_hardware->set_max_dpi_x(kCachedMaxDpiX);
  cached_hardware->set_max_dpi_y(kCachedMaxDpiY);
#endif
  EnvironmentRecorder(&local_state)
      .SerializeAndRecordEnvironmentToPrefs(cached_profile);

  EarlyMetricsGuard early_guard(&local_state);

  TestScreenInfoMetricsProvider provider;
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());

  const SystemProfileProto::Hardware& hardware =
      uma_proto.system_profile().hardware();
  EXPECT_EQ(kCachedScreenWidth, hardware.primary_screen_width());
  EXPECT_EQ(kCachedScreenHeight, hardware.primary_screen_height());
  EXPECT_EQ(kCachedScreenScaleFactor, hardware.primary_screen_scale_factor());
  EXPECT_EQ(kCachedScreenCount, hardware.screen_count());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(kCachedMaxDpiX, hardware.max_dpi_x());
  EXPECT_EQ(kCachedMaxDpiY, hardware.max_dpi_y());
#endif
}

}  // namespace metrics
