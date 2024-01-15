// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/client_hints.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Eq;
using ::testing::Optional;
using ::testing::Pointee;

const int mock_chrome_version = 115;

void AssertDeviceMetricsCommand(const Command& command,
                                const DeviceMetrics& device_metrics) {
  ASSERT_EQ("Emulation.setDeviceMetricsOverride", command.method);
  std::optional<int> width = command.params.FindInt("width");

  std::optional<int> height = command.params.FindInt("height");
  ASSERT_TRUE(width);
  ASSERT_TRUE(height);
  ASSERT_THAT(command.params.FindBool("mobile"),
              Optional(device_metrics.mobile));
  ASSERT_EQ(device_metrics.width, *width);
  ASSERT_EQ(device_metrics.height, *height);
  ASSERT_EQ(device_metrics.device_scale_factor,
            command.params.FindDouble("deviceScaleFactor").value());
}

void AssertBrandsAndVersions(
    std::optional<std::vector<BrandVersion>> expected_list,
    const base::Value::List* actual_list) {
  // Testing expected_list.has_value <=> actual_list != nullptr by parts:
  // Testing that expected_list.has_value => actual_list != nullptr
  ASSERT_TRUE(!expected_list.has_value() || (actual_list != nullptr));
  // Testing that actual_list != nullptr => expected_list.has_value
  ASSERT_TRUE(actual_list == nullptr || (expected_list.has_value()));

  if (!expected_list.has_value() || actual_list == nullptr) {
    return;
  }

  ASSERT_EQ(expected_list->size(), actual_list->size());

  for (size_t k = 0; k < expected_list->size(); ++k) {
    const base::Value::Dict* brand_version = (*actual_list)[k].GetIfDict();
    ASSERT_NE(nullptr, brand_version);
    ASSERT_THAT(brand_version->FindString("brand"),
                Pointee(Eq(expected_list->at(k).brand)));
    ASSERT_THAT(brand_version->FindString("version"),
                Pointee(Eq(expected_list->at(k).version)));
  }
}

void AssertClientHintsCommand(const Command& command,
                              const ClientHints& expected_client_hints,
                              const std::string& expected_user_agent) {
  ASSERT_EQ("Emulation.setUserAgentOverride", command.method);
  ASSERT_THAT(command.params.FindString("userAgent"),
              Pointee(Eq(expected_user_agent)));

  const base::Value::Dict* actual_client_hints =
      command.params.FindDict("userAgentMetadata");
  ASSERT_NE(nullptr, actual_client_hints);

  ASSERT_THAT(actual_client_hints->FindString("architecture"),
              Pointee(Eq(expected_client_hints.architecture)));
  ASSERT_THAT(actual_client_hints->FindString("bitness"),
              Pointee(Eq(expected_client_hints.bitness)));
  ASSERT_THAT(actual_client_hints->FindBool("mobile"),
              Optional(expected_client_hints.mobile));
  ASSERT_THAT(actual_client_hints->FindString("model"),
              Pointee(Eq(expected_client_hints.model)));
  ASSERT_THAT(actual_client_hints->FindString("platform"),
              Pointee(Eq(expected_client_hints.platform)));
  ASSERT_THAT(actual_client_hints->FindString("platformVersion"),
              Pointee(Eq(expected_client_hints.platform_version)));
  ASSERT_THAT(actual_client_hints->FindBool("wow64"),
              Optional(expected_client_hints.wow64));
  AssertBrandsAndVersions(expected_client_hints.brands,
                          actual_client_hints->FindList("brands"));
  AssertBrandsAndVersions(expected_client_hints.brands,
                          actual_client_hints->FindList("fullVersionList"));
}

}  // namespace

TEST(MobileEmulationOverrideManager, SendsCommandWithTouchOnConnect) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, true, true);
  MobileDevice mobile_device;
  mobile_device.device_metrics = device_metrics;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         mock_chrome_version);
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
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         mock_chrome_version);
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
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         mock_chrome_version);
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

TEST(MobileEmulationOverrideManager, SendsClientHintsExplicitUA) {
  RecorderDevToolsClient client;
  MobileDevice mobile_device;
  mobile_device.device_metrics = DeviceMetrics{};
  ClientHints client_hints;
  client_hints.architecture = "CustomArch";
  client_hints.bitness = "CustomBitness";
  client_hints.mobile = mobile_device.device_metrics->mobile;
  client_hints.model = "CustomModel";
  client_hints.platform = "CustomPlatform";
  client_hints.platform_version = "100500";
  client_hints.wow64 = true;
  mobile_device.client_hints = client_hints;
  mobile_device.user_agent = "Custom UA";
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         mock_chrome_version);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertClientHintsCommand(client.commands_[2], client_hints, "Custom UA"));
}

TEST(MobileEmulationOverrideManager, SendsClientHintsTemplatedUA) {
  RecorderDevToolsClient client;
  MobileDevice mobile_device;
  mobile_device.device_metrics = DeviceMetrics{};
  ClientHints client_hints;
  client_hints.architecture = "CustomArch";
  client_hints.bitness = "CustomBitness";
  client_hints.mobile = mobile_device.device_metrics->mobile;
  client_hints.model = "CustomModel";
  client_hints.platform = "CustomPlatform";
  client_hints.platform_version = "100500";
  client_hints.wow64 = true;
  mobile_device.client_hints = client_hints;
  mobile_device.user_agent = "Custom UA/%s XYZ";
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         112);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(AssertClientHintsCommand(
      client.commands_[2], client_hints, "Custom UA/112.0.0.0 XYZ"));
}

TEST(MobileEmulationOverrideManager, SendsClientHintsInferredMobileUA) {
  RecorderDevToolsClient client;
  MobileDevice mobile_device;
  mobile_device.device_metrics = DeviceMetrics{};
  mobile_device.device_metrics->mobile = true;
  ClientHints client_hints;
  client_hints.architecture = "arm";
  client_hints.bitness = "64";
  client_hints.mobile = mobile_device.device_metrics->mobile;
  client_hints.model = "Example Phone";
  client_hints.platform = "Android";
  client_hints.platform_version = "15";
  client_hints.wow64 = false;
  mobile_device.client_hints = client_hints;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         113);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  const std::string expected_ua =
      "Mozilla/5.0 (Linux; Android 10; K) "
      "AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/113.0.0.0 Mobile Safari/537.36";
  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertClientHintsCommand(client.commands_[2], client_hints, expected_ua));
}

TEST(MobileEmulationOverrideManager, SendsClientHintsInferredNonMobileUA) {
  RecorderDevToolsClient client;
  MobileDevice mobile_device;
  mobile_device.device_metrics = DeviceMetrics{};
  mobile_device.device_metrics->mobile = false;
  ClientHints client_hints;
  client_hints.architecture = "arm";
  client_hints.bitness = "64";
  client_hints.mobile = mobile_device.device_metrics->mobile;
  client_hints.model = "Example Phone";
  client_hints.platform = "Android";
  client_hints.platform_version = "15";
  client_hints.wow64 = false;
  mobile_device.client_hints = client_hints;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         114);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  const std::string expected_ua =
      "Mozilla/5.0 (Linux; Android 10; K) "
      "AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/114.0.0.0 Safari/537.36";
  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertClientHintsCommand(client.commands_[2], client_hints, expected_ua));
}

TEST(MobileEmulationOverrideManager, SendsClientHintsWithoutDeviceMetricsUA) {
  RecorderDevToolsClient client;
  MobileDevice mobile_device;
  ClientHints client_hints;
  client_hints.architecture = "arm";
  client_hints.bitness = "64";
  client_hints.mobile = false;
  client_hints.model = "Example Phone";
  client_hints.platform = "Android";
  client_hints.platform_version = "15";
  client_hints.wow64 = false;
  mobile_device.client_hints = client_hints;
  MobileEmulationOverrideManager manager(&client, std::move(mobile_device),
                                         114);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  const std::string expected_ua =
      "Mozilla/5.0 (Linux; Android 10; K) "
      "AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/114.0.0.0 Safari/537.36";
  ASSERT_EQ(1u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertClientHintsCommand(client.commands_[0], client_hints, expected_ua));
}
