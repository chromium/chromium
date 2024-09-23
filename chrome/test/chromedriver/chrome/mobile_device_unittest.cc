// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_device.h"

#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/client_hints.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::Pointee;

namespace {

template <int Code>
testing::AssertionResult StatusCodeIs(const Status& status) {
  if (status.code() == Code) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

testing::AssertionResult StatusOk(const Status& status) {
  return StatusCodeIs<kOk>(status);
}

std::vector<std::string> GetDeviceNames() {
  std::vector<std::string> device_names;
  MobileDevice::GetKnownMobileDeviceNamesForTesting(&device_names);
  return device_names;
}

}  // namespace

TEST(MobileDevicePreset, KnownMobileDeviceNamesIsNotEmpty) {
  std::vector<std::string> device_names;
  EXPECT_TRUE(StatusOk(
      MobileDevice::GetKnownMobileDeviceNamesForTesting(&device_names)));
  EXPECT_FALSE(device_names.empty());
}

TEST(MobileDevicePreset, CanFindKnownDevices) {
  std::vector<std::string> device_names;
  MobileDevice::GetKnownMobileDeviceNamesForTesting(&device_names);
  for (const std::string& device_name : device_names) {
    MobileDevice device;
    EXPECT_TRUE(StatusOk(MobileDevice::FindMobileDevice(device_name, &device)));
  }
}

TEST(MobileDevicePreset, CheckAnyDeviceWithClientHints) {
  // Check that we parse client hints of any known device correctly
  MobileDevice device;
  ASSERT_TRUE(StatusOk(MobileDevice::FindMobileDevice("Nexus 5", &device)));
  ASSERT_TRUE(device.client_hints.has_value());
  const ClientHints& client_hints = device.client_hints.value();
  EXPECT_EQ("Android", client_hints.platform);
  EXPECT_EQ("6.0", client_hints.platform_version);
  EXPECT_EQ("Nexus 5", client_hints.model);
  EXPECT_EQ(true, client_hints.mobile);
}

class MobileDevicePresetPerDeviceName
    : public testing::TestWithParam<std::string> {};

TEST_P(MobileDevicePresetPerDeviceName, ValidatePresets) {
  std::string device_name = GetParam();
  MobileDevice device;
  MobileDevice::FindMobileDevice(device_name, &device);
  ASSERT_TRUE(device.device_metrics.has_value());

  bool mobile_ua = false;

  if (device.user_agent.has_value()) {
    std::string user_agent = device.user_agent.value();
    mobile_ua =
        std::string_view{user_agent}.find("Mobile") != std::string_view::npos;
  }
  const DeviceMetrics& device_metrics = device.device_metrics.value();
  // Testing the implication: mobile_ua => device_metrics.mobile
  EXPECT_TRUE(!mobile_ua || device_metrics.mobile);

  ASSERT_TRUE(device.user_agent.has_value() || device.client_hints.has_value());
  if (device.client_hints.has_value()) {
    const ClientHints& client_hints = device.client_hints.value();
    if (client_hints.platform == "Android") {
      // This implies from GetUserAgentMetadata and GetReducedAgent functions
      // code in components/embedder_support/user_agent_utils.cc.
      // S/A: crbug.com/1442468, crbug.com/1442784
      EXPECT_EQ(mobile_ua, client_hints.mobile);
    } else if (!client_hints.platform.empty()) {
      // Testing the implication: mobile_ua => client_hints.mobile
      EXPECT_TRUE(!mobile_ua || client_hints.mobile);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Presets,
                         MobileDevicePresetPerDeviceName,
                         testing::ValuesIn(GetDeviceNames()));
