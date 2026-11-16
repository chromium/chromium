// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cached_metrics_provider.h"

#include "components/metrics/early_metrics_guard.h"
#include "components/metrics/environment_recorder.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {
namespace {

class TestCachedMetricsProvider : public CachedMetricsProvider {
 public:
  TestCachedMetricsProvider() = default;

  void ProvideSystemProfileMetricsFromCache(
      const SystemProfileProto& cached_system_profile,
      SystemProfileProto* system_profile) override {
    provided_from_cache_ = true;
  }

  void ProvideSystemProfileMetricsNormal(
      SystemProfileProto* system_profile) override {
    provided_normal_ = true;
  }

  bool provided_from_cache() const { return provided_from_cache_; }
  bool provided_normal() const { return provided_normal_; }

 private:
  bool provided_from_cache_ = false;
  bool provided_normal_ = false;
};

}  // namespace

class CachedMetricsProviderTest : public testing::Test {
 public:
  CachedMetricsProviderTest() {
    EnvironmentRecorder::RegisterPrefs(prefs_.registry());
    SystemProfileProto profile;
    profile.set_client_uuid("test_uuid");
    EnvironmentRecorder(&prefs_).SerializeAndRecordEnvironmentToPrefs(profile);
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(CachedMetricsProviderTest, NormalStartup) {
  TestCachedMetricsProvider provider;
  SystemProfileProto system_profile;

  // Early guard is NOT active by default.
  EXPECT_FALSE(EarlyMetricsGuard::IsEarlyMetricsRecordingActive());

  provider.ProvideSystemProfileMetrics(&system_profile);

  // A saved system profile exists (see the fixture), but it must only be read
  // during early startup.
  EXPECT_FALSE(provider.provided_from_cache());
  EXPECT_TRUE(provider.provided_normal());
}

TEST_F(CachedMetricsProviderTest, EarlyStartup) {
  TestCachedMetricsProvider provider;
  SystemProfileProto system_profile;

  // Simulate early startup.
  EarlyMetricsGuard guard(&prefs_);
  EXPECT_TRUE(EarlyMetricsGuard::IsEarlyMetricsRecordingActive());

  provider.ProvideSystemProfileMetrics(&system_profile);

  // Since early startup is active AND local_state was provided,
  // it should ONLY read from cache.
  EXPECT_TRUE(provider.provided_from_cache());
  EXPECT_FALSE(provider.provided_normal());
}

TEST_F(CachedMetricsProviderTest, EarlyStartupNoLocalState) {
  TestCachedMetricsProvider provider;
  SystemProfileProto system_profile;

  EarlyMetricsGuard guard;

  provider.ProvideSystemProfileMetrics(&system_profile);

  // If local_state is null, it skips cache reading entirely.
  EXPECT_FALSE(provider.provided_from_cache());
  EXPECT_FALSE(provider.provided_normal());
}

}  // namespace metrics
