// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Optional;

void AssertDeviceMetricsCommand(const Command& command,
                                const DeviceMetrics& device_metrics) {
  ASSERT_EQ("Page.setDeviceMetricsOverride", command.method);
  absl::optional<int> width = command.params.FindInt("width");

  absl::optional<int> height = command.params.FindInt("height");
  ASSERT_TRUE(width);
  ASSERT_TRUE(height);
  ASSERT_THAT(command.params.FindBool("mobile"),
              Optional(device_metrics.mobile));
  ASSERT_THAT(command.params.FindBool("fitWindow"),
              Optional(device_metrics.fit_window));
  ASSERT_THAT(command.params.FindBool("textAutosizing"),
              Optional(device_metrics.text_autosizing));
  ASSERT_EQ(device_metrics.width, *width);
  ASSERT_EQ(device_metrics.height, *height);
  ASSERT_EQ(device_metrics.device_scale_factor,
            command.params.FindDouble("deviceScaleFactor").value());
  ASSERT_EQ(device_metrics.font_scale_factor,
            command.params.FindDouble("fontScaleFactor").value());
}

}  // namespace

TEST(MobileEmulationOverrideManager, SendsCommandWithTouchOnConnect) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, true, true);
  MobileDevice mobile_device;
  mobile_device.device_metrics = device_metrics;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device));
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(4u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDeviceMetricsCommand(client.commands_[2], device_metrics));
}

TEST(MobileEmulationOverrideManager, SendsCommandWithoutTouchOnConnect) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, false, true);
  MobileDevice mobile_device;
  mobile_device.device_metrics = device_metrics;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device));
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  ASSERT_EQ(1u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDeviceMetricsCommand(client.commands_[1], device_metrics));
}

TEST(MobileEmulationOverrideManager, SendsCommandOnNavigation) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, true, true);
  MobileDevice mobile_device;
  mobile_device.device_metrics = device_metrics;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device));
  base::Value::Dict main_frame_params;
  ASSERT_EQ(kOk,
            manager.OnEvent(&client, "Page.frameNavigated", main_frame_params)
                .code());
  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_EQ(kOk,
            manager.OnEvent(&client, "Page.frameNavigated", main_frame_params)
                .code());
  ASSERT_EQ(4u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDeviceMetricsCommand(client.commands_[2], device_metrics));

  base::Value::Dict sub_frame_params;
  sub_frame_params.SetByDottedPath("frame.parentId", "id");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.frameNavigated", sub_frame_params).code());
  ASSERT_EQ(4u, client.commands_.size());
}
