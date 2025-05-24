// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/win/win_signals_collector.h"

#include <array>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/device_signals/core/browser/mock_system_signals_service_host.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
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

  SignalsAggregationRequest CreateRequest(SignalName signal_name) {
    SignalsAggregationRequest request;
    request.signal_names.emplace(signal_name);
    return request;
  }

  base::test::TaskEnvironment task_environment_;

  StrictMock<MockSystemSignalsServiceHost> service_host_;
  StrictMock<MockSystemSignalsService> service_;
  WinSignalsCollector win_collector_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(WinSignalsCollectorTest, SupportedSignalNames) {
  const std::array<SignalName, 2> supported_signals{
      {SignalName::kAntiVirus, SignalName::kHotfixes}};

  const auto names_set = win_collector_.GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(WinSignalsCollectorTest, GetSignal_Unsupported) {
  SignalName signal_name = SignalName::kFileSystemInfo;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  win_collector_.GetSignal(signal_name, UserPermission::kGranted,
                           CreateRequest(signal_name), response,
                           run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests that not being able to retrieve a pointer to the SystemSignalsService
// returns an error.
TEST_F(WinSignalsCollectorTest, GetSignal_AV_MissingSystemSignalsService) {
  EXPECT_CALL(service_host_, GetService()).WillOnce(Return(nullptr));

  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  win_collector_.GetSignal(signal_name, UserPermission::kGranted,
                           CreateRequest(signal_name), response,
                           run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.av_signal_response.has_value());
  ASSERT_TRUE(response.av_signal_response->collection_error.has_value());
  EXPECT_EQ(response.av_signal_response->collection_error.value(),
            SignalCollectionError::kMissingSystemService);
}

// Tests that not being able to retrieve a pointer to the SystemSignalsService
// returns an error.
TEST_F(WinSignalsCollectorTest, GetSignal_Hotfix_MissingSystemSignalsService) {
  EXPECT_CALL(service_host_, GetService()).WillOnce(Return(nullptr));

  SignalName signal_name = SignalName::kHotfixes;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  win_collector_.GetSignal(signal_name, UserPermission::kGranted,
                           CreateRequest(signal_name), response,
                           run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.hotfix_signal_response.has_value());
  ASSERT_TRUE(response.hotfix_signal_response->collection_error.has_value());
  EXPECT_EQ(response.hotfix_signal_response->collection_error.value(),
            SignalCollectionError::kMissingSystemService);
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

  SignalName signal_name = SignalName::kHotfixes;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  win_collector_.GetSignal(signal_name, UserPermission::kGranted,
                           CreateRequest(signal_name), response,
                           run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.hotfix_signal_response.has_value());
  EXPECT_FALSE(response.hotfix_signal_response->collection_error.has_value());
  EXPECT_EQ(response.hotfix_signal_response->hotfixes.size(), hotfixes.size());
  EXPECT_EQ(response.hotfix_signal_response->hotfixes[0], hotfixes[0]);
}

TEST_F(WinSignalsCollectorTest, GetSignal_AV_Empty) {
  EXPECT_CALL(service_host_, GetService()).Times(1);
  EXPECT_CALL(service_, GetAntiVirusSignals(_))
      .WillOnce(Invoke([](GetAntiVirusSignalsCallback signal_callback) {
        std::move(signal_callback).Run(std::vector<AvProduct>());
      }));

  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  win_collector_.GetSignal(signal_name, UserPermission::kGranted,
                           CreateRequest(signal_name), response,
                           run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.av_signal_response.has_value());
  EXPECT_FALSE(response.av_signal_response->collection_error.has_value());
  EXPECT_EQ(response.av_signal_response->antivirus_state,
            InstalledAntivirusState::kNone);
  EXPECT_TRUE(response.av_signal_response->av_products.empty());
}

struct AntivirusTestCase {
  std::vector<AvProductState> av_product_states{};
  InstalledAntivirusState antivirus_state{};
};

class AntivirusWinSignalsCollectorTest
    : public WinSignalsCollectorTest,
      public testing::WithParamInterface<AntivirusTestCase> {};

// Tests a successful AV signal retrieval.
TEST_P(AntivirusWinSignalsCollectorTest, GetSignal_AV) {
  const AntivirusTestCase& test_case = GetParam();

  std::vector<AvProduct> av_products;
  for (const auto& state : test_case.av_product_states) {
    av_products.push_back({"AV Product Name", state, "some product id"});
  }

  EXPECT_CALL(service_host_, GetService()).Times(1);
  EXPECT_CALL(service_, GetAntiVirusSignals(_))
      .WillOnce(
          Invoke([&av_products](GetAntiVirusSignalsCallback signal_callback) {
            std::move(signal_callback).Run(av_products);
          }));

  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  win_collector_.GetSignal(signal_name, UserPermission::kGranted,
                           CreateRequest(signal_name), response,
                           run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.av_signal_response.has_value());
  EXPECT_FALSE(response.av_signal_response->collection_error.has_value());
  EXPECT_EQ(response.av_signal_response->antivirus_state,
            test_case.antivirus_state);
  EXPECT_EQ(response.av_signal_response->av_products.size(),
            av_products.size());
  for (size_t i = 0U; i < av_products.size(); i++) {
    EXPECT_EQ(response.av_signal_response->av_products[i], av_products[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AntivirusWinSignalsCollectorTest,
    testing::Values(
        AntivirusTestCase{{AvProductState::kOn},
                          InstalledAntivirusState::kEnabled},
        AntivirusTestCase{{AvProductState::kOff},
                          InstalledAntivirusState::kDisabled},
        AntivirusTestCase{{AvProductState::kSnoozed},
                          InstalledAntivirusState::kDisabled},
        AntivirusTestCase{{AvProductState::kExpired},
                          InstalledAntivirusState::kDisabled},
        AntivirusTestCase{{AvProductState::kOff, AvProductState::kSnoozed},
                          InstalledAntivirusState::kDisabled},
        AntivirusTestCase{{AvProductState::kOff, AvProductState::kSnoozed,
                           AvProductState::kExpired, AvProductState::kOn},
                          InstalledAntivirusState::kEnabled}));

}  // namespace device_signals
