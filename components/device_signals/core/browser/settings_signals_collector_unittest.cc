// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/settings_signals_collector.h"

#include <array>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/device_signals/core/browser/mock_settings_client.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace device_signals {
namespace {

SignalsAggregationRequest CreateRequest(SignalName signal_name,
                                        bool with_settings_parameter = true) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(signal_name);

  if (with_settings_parameter) {
    GetSettingsOptions options1;
    options1.path = "test_path1";
    options1.key = "test_setting1";
    options1.get_value = true;
    options1.hive = std::nullopt;

    GetSettingsOptions options2;
    options2.path = "test_path2";
    options2.key = "test_setting2";
    options2.get_value = false;
    options2.hive = std::nullopt;

    request.settings_signal_parameters.push_back(options1);
    request.settings_signal_parameters.push_back(options2);
  }

  return request;
}
}  // namespace

using GetSettingsSignalsCallback =
    MockSettingsClient::GetSettingsSignalsCallback;

class SettingsSignalsCollectorTest : public testing::Test {
 protected:
  SettingsSignalsCollectorTest() {
    auto settings_client = std::make_unique<StrictMock<MockSettingsClient>>();
    settings_client_ = settings_client.get();
    signal_collector_ =
        std::make_unique<SettingsSignalsCollector>(std::move(settings_client));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SettingsSignalsCollector> signal_collector_;
  raw_ptr<StrictMock<MockSettingsClient>> settings_client_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(SettingsSignalsCollectorTest, SupportedSettingsSignalNames) {
  const std::array<SignalName, 1> supported_signals{
      {SignalName::kSystemSettings}};

  const auto names_set = signal_collector_->GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(SettingsSignalsCollectorTest, GetSettingsSignal_Unsupported) {
  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(signal_name, CreateRequest(signal_name),
                               response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests that the request does not contain the required parameters for the
// settings signal.
TEST_F(SettingsSignalsCollectorTest, GetSignal_Settings_MissingParameters) {
  SignalName signal_name = SignalName::kSystemSettings;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(
      signal_name,
      CreateRequest(signal_name, /*with_settings_parameter=*/false), response,
      run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.settings_response.has_value());
  ASSERT_TRUE(response.settings_response->collection_error.has_value());
  EXPECT_EQ(response.settings_response->collection_error.value(),
            SignalCollectionError::kMissingParameters);
}

// Tests a successful Settings signal retrieval.
TEST_F(SettingsSignalsCollectorTest, GetSignal_SettingsInfo) {
  // Can be any value really.
  SettingsItem retrieved_item;
  retrieved_item.path = "test_path";
  retrieved_item.key = "test_key";
  retrieved_item.presence = PresenceValue::kFound;
  retrieved_item.hive = std::nullopt;
  retrieved_item.setting_json_value = std::nullopt;

  std::vector<SettingsItem> settings_items;
  settings_items.push_back(retrieved_item);

  SignalName signal_name = SignalName::kSystemSettings;
  auto request = CreateRequest(signal_name);

  EXPECT_CALL(*settings_client_,
              GetSettings(ContainerEq(request.settings_signal_parameters), _))
      .WillOnce(
          Invoke([&settings_items](
                     const std::vector<GetSettingsOptions> signal_parameters,
                     GetSettingsSignalsCallback signal_callback) {
            std::move(signal_callback).Run(settings_items);
          }));

  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(signal_name, request, response,
                               run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.settings_response.has_value());
  EXPECT_FALSE(response.settings_response->collection_error.has_value());
  EXPECT_EQ(response.settings_response->settings_items.size(),
            settings_items.size());
  EXPECT_EQ(response.settings_response->settings_items[0], settings_items[0]);
}

}  // namespace device_signals
