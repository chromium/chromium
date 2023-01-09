// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_device_monitor.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "media/audio/audio_device_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MockDevicesChangedObserver
    : public MediaNotificationDeviceMonitor::DevicesChangedObserver {
 public:
  MOCK_METHOD(void, OnDevicesChanged, (), (override));
};

class MockMediaNotificationDeviceProvider
    : public MediaNotificationDeviceProvider {
 public:
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterOutputDeviceDescriptionsCallback,
              (GetOutputDevicesCallback cb),
              (override));

  void GetOutputDeviceDescriptions(
      media::AudioSystem::OnDeviceDescriptionsCallback cb) override {
    std::move(cb).Run(device_descriptions);
  }

  media::AudioDeviceDescriptions device_descriptions;
};

class PollingDeviceMonitorImplTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};
}  // anonymous namespace

TEST_F(PollingDeviceMonitorImplTest, DeviceChangeNotifiesObserver) {
  // When the list of audio devices changes, observers should be notified the
  // next time the monitor polls.
  MockDevicesChangedObserver observer;
  MockMediaNotificationDeviceProvider provider;
  PollingDeviceMonitorImpl monitor(&provider);
  monitor.AddDevicesChangedObserver(&observer);

  provider.device_descriptions.emplace_back("1", "1", "1");
  EXPECT_CALL(observer, OnDevicesChanged).Times(1);
  monitor.StartMonitoring();
  task_environment.FastForwardBy(base::Seconds(
      PollingDeviceMonitorImpl::get_polling_interval_for_testing()));

  // When the monitor polls a second time, the observer should not be notified
  // as the list of devices hasn't changed.
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_CALL(observer, OnDevicesChanged).Times(0);
  task_environment.FastForwardBy(base::Seconds(
      PollingDeviceMonitorImpl::get_polling_interval_for_testing()));
  testing::Mock::VerifyAndClearExpectations(&observer);
}
