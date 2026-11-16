// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/motherboard_metrics_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
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

}  // namespace metrics
