// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_count_metrics_provider.h"

#include <map>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class DeviceCountMetricsProviderTest : public testing::Test {
 public:
  DeviceCountMetricsProviderTest()
      : metrics_provider_(
            base::BindRepeating(&DeviceCountMetricsProviderTest::GetTrackers,
                                base::Unretained(this))) {}

  void AddTracker(const std::map<DeviceInfo::FormFactor, int>& count) {
    auto tracker = std::make_unique<FakeDeviceInfoTracker>();
    tracker->OverrideActiveDeviceCount(count);
    trackers_.emplace_back(std::move(tracker));
  }

  void GetTrackers(std::vector<const DeviceInfoTracker*>* trackers) {
    for (const auto& tracker : trackers_) {
      trackers->push_back(tracker.get());
    }
  }

  struct ExpectedCount {
    int total;
    int desktop_count;
    int phone_count;
    int tablet_count;
  };
  void TestProvider(const ExpectedCount& expected_count) {
    base::HistogramTester histogram_tester;
    metrics_provider_.ProvideCurrentSessionData(nullptr);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2",
                                        expected_count.total, 1);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2.Desktop",
                                        expected_count.desktop_count, 1);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2.Phone",
                                        expected_count.phone_count, 1);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2.Tablet",
                                        expected_count.tablet_count, 1);
  }

 private:
  DeviceCountMetricsProvider metrics_provider_;
  std::vector<std::unique_ptr<DeviceInfoTracker>> trackers_;
};

namespace {

TEST_F(DeviceCountMetricsProviderTest, NoTrackers) {
  TestProvider(ExpectedCount{});
}

TEST_F(DeviceCountMetricsProviderTest, SingleTracker) {
  AddTracker({{DeviceInfo::FormFactor::kDesktop, 1},
              {DeviceInfo::FormFactor::kPhone, 1}});
  TestProvider(ExpectedCount{
      .total = 2, .desktop_count = 1, .phone_count = 1, .tablet_count = 0});
}

TEST_F(DeviceCountMetricsProviderTest, MultipileTrackers) {
  AddTracker({{DeviceInfo::FormFactor::kPhone, 1}});
  AddTracker({{DeviceInfo::FormFactor::kTablet, 3},
              {DeviceInfo::FormFactor::kDesktop, 2}});
  AddTracker({{DeviceInfo::FormFactor::kDesktop, -120}});
  AddTracker({{DeviceInfo::FormFactor::kDesktop, 3}});
  TestProvider(ExpectedCount{
      .total = 5, .desktop_count = 3, .phone_count = 1, .tablet_count = 3});
}

TEST_F(DeviceCountMetricsProviderTest, OnlyNegative) {
  AddTracker({{DeviceInfo::FormFactor::kPhone, -121}});
  TestProvider(ExpectedCount{
      .total = 0, .desktop_count = 0, .phone_count = 0, .tablet_count = 0});
}

TEST_F(DeviceCountMetricsProviderTest, VeryLarge) {
  AddTracker({{DeviceInfo::FormFactor::kDesktop, 123456789},
              {DeviceInfo::FormFactor::kPhone, 1}});
  TestProvider(ExpectedCount{
      .total = 100, .desktop_count = 100, .phone_count = 1, .tablet_count = 0});
}

}  // namespace

}  // namespace syncer
