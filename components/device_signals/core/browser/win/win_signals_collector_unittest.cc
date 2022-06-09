// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/win/win_signals_collector.h"

#include <array>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/device_signals/core/browser/mock_system_signals_service_host.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace device_signals {

using GetAntiVirusSignalsCallback =
    MockSystemSignalsService::GetAntiVirusSignalsCallback;
using GetHotfixSignalsCallback =
    MockSystemSignalsService::GetHotfixSignalsCallback;

class WinSignalsCollectorTest : public testing::Test {
 protected:
  WinSignalsCollectorTest() : win_collector_(&service_host_) {
    ON_CALL(service_host_, GetService()).WillByDefault(Return(&service_));
  }

  base::test::TaskEnvironment task_environment_;

  StrictMock<MockSystemSignalsServiceHost> service_host_;
  StrictMock<MockSystemSignalsService> service_;
  WinSignalsCollector win_collector_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(WinSignalsCollectorTest, SupportedSignalNames) {
  const std::array<std::string, 2> supported_signals{
      {names::kAntiVirusInfo, names::kInstalledHotfixes}};

  const auto names_set = win_collector_.GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(WinSignalsCollectorTest, GetSignal_Unsupported) {
  base::test::TestFuture<base::Value> future;
  win_collector_.GetSignal(names::kDeviceId, base::Value(),
                           future.GetCallback());
  EXPECT_EQ(future.Get(), base::Value(errors::kUnsupported));
}

// Tests that not being able to retrieve a pointer to the SystemSignalsService
// returns an error.
TEST_F(WinSignalsCollectorTest, GetSignal_MissingSystemSignalsService) {
  for (const auto& signal_name : win_collector_.GetSupportedSignalNames()) {
    EXPECT_CALL(service_host_, GetService()).WillOnce(Return(nullptr));

    base::test::TestFuture<base::Value> future;
    win_collector_.GetSignal(signal_name, base::Value(), future.GetCallback());
    EXPECT_EQ(future.Get(), base::Value(errors::kMissingSystemService));
  }
}

// Tests a successful AV signal retrieval.
TEST_F(WinSignalsCollectorTest, GetSignal_AV) {
  std::vector<AvProduct> av_products;
  av_products.push_back(
      {"AV Product Name", AvProductState::kOn, "some product id"});

  EXPECT_CALL(service_host_, GetService()).Times(1);
  EXPECT_CALL(service_, GetAntiVirusSignals(_))
      .WillOnce(
          Invoke([&av_products](GetAntiVirusSignalsCallback signal_callback) {
            std::move(signal_callback).Run(av_products);
          }));

  base::test::TestFuture<base::Value> future;
  win_collector_.GetSignal(names::kAntiVirusInfo, base::Value(),
                           future.GetCallback());

  const base::Value& response = future.Get();
  ASSERT_TRUE(response.is_list());
  const base::Value::List& list_value = response.GetList();
  ASSERT_EQ(list_value.size(), av_products.size());
  EXPECT_EQ(list_value[0], av_products[0].ToValue());
}

// Tests a successful hotfix signal retrieval.
TEST_F(WinSignalsCollectorTest, GetSignal_Hotfix) {
  std::vector<InstalledHotfix> hotfixes;
  hotfixes.push_back({"hotfix id"});

  EXPECT_CALL(service_host_, GetService()).Times(1);
  EXPECT_CALL(service_, GetHotfixSignals(_))
      .WillOnce(Invoke([&hotfixes](GetHotfixSignalsCallback signal_callback) {
        std::move(signal_callback).Run(hotfixes);
      }));

  base::test::TestFuture<base::Value> future;
  win_collector_.GetSignal(names::kInstalledHotfixes, base::Value(),
                           future.GetCallback());

  const base::Value& response = future.Get();
  ASSERT_TRUE(response.is_list());
  const base::Value::List& list_value = response.GetList();
  ASSERT_EQ(list_value.size(), hotfixes.size());
  EXPECT_EQ(list_value[0], hotfixes[0].ToValue());
}

}  // namespace device_signals
