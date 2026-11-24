// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/motherboard_metrics_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/metrics/early_metrics_guard.h"
#include "components/metrics/environment_recorder.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {
namespace {

class TestMotherboardMetricsProvider : public MotherboardMetricsProvider {
 public:
  TestMotherboardMetricsProvider() = default;

  TestMotherboardMetricsProvider(const TestMotherboardMetricsProvider&) = delete;
  TestMotherboardMetricsProvider& operator=(
      const TestMotherboardMetricsProvider&) = delete;

  ~TestMotherboardMetricsProvider() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST(MotherboardMetricsProviderTest, ProvideSystemProfileMetrics) {
  TestMotherboardMetricsProvider provider;
  SystemProfileProto system_profile;

  base::RunLoop run_loop;
  provider.AsyncInit(run_loop.QuitClosure());
  run_loop.Run();

  provider.ProvideSystemProfileMetrics(&system_profile);

  // Verify that the system profile has the motherboard info set.
  const SystemProfileProto::Hardware& hardware = system_profile.hardware();
  ASSERT_TRUE(hardware.has_motherboard());
  ASSERT_TRUE(hardware.motherboard().has_manufacturer());
  ASSERT_TRUE(hardware.motherboard().has_model());
  ASSERT_TRUE(hardware.motherboard().has_bios_manufacturer());
  ASSERT_TRUE(hardware.motherboard().has_bios_version());
  ASSERT_TRUE(hardware.motherboard().has_bios_type());
}

TEST(MotherboardMetricsProviderTest,
     ProvideSystemProfileMetrics_NotInitialized) {
  // AsyncInit() is not called, so motherboard info is not available yet.
  TestMotherboardMetricsProvider provider;
  SystemProfileProto system_profile;

  provider.ProvideSystemProfileMetrics(&system_profile);

  // Outside early startup, nothing is written until motherboard info is
  // available.
  EXPECT_FALSE(system_profile.hardware().has_motherboard());
}

TEST(MotherboardMetricsProviderTest, ReadsFromCacheWhenEarlyMetrics) {
  TestingPrefServiceSimple local_state;
  EnvironmentRecorder::RegisterPrefs(local_state.registry());

  SystemProfileProto cached_profile;
  auto* cached_motherboard =
      cached_profile.mutable_hardware()->mutable_motherboard();
  cached_motherboard->set_manufacturer("TestManufacturer");
  cached_motherboard->set_model("TestModel");
  cached_motherboard->set_bios_type(
      SystemProfileProto::Hardware::BIOS_TYPE_UEFI);
  EnvironmentRecorder(&local_state)
      .SerializeAndRecordEnvironmentToPrefs(cached_profile);

  EarlyMetricsGuard early_guard(&local_state);

  TestMotherboardMetricsProvider provider;
  SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  const SystemProfileProto::Hardware& hardware = system_profile.hardware();
  ASSERT_TRUE(hardware.has_motherboard());
  EXPECT_EQ("TestManufacturer", hardware.motherboard().manufacturer());
  EXPECT_EQ("TestModel", hardware.motherboard().model());
  EXPECT_EQ(SystemProfileProto::Hardware::BIOS_TYPE_UEFI,
            hardware.motherboard().bios_type());
}

}  // namespace metrics
