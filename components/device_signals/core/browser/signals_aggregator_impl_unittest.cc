// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/browser/mock_signals_collector.h"
#include "components/device_signals/core/browser/mock_user_permission_service.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Pointee;
using testing::Ref;
using testing::Return;

namespace device_signals {

namespace {

constexpr char kGaiaId[] = "gaia-id";

}  // namespace

class SignalsAggregatorImplTest : public testing::Test {
 protected:
  SignalsAggregatorImplTest() {
    auto av_signal_collector = GetFakeCollector(SignalName::kAntiVirus);
    av_signal_collector_ = av_signal_collector.get();

    auto hotfix_signal_collector = GetFakeCollector(SignalName::kHotfixes);
    hotfix_signal_collector_ = hotfix_signal_collector.get();

    std::vector<std::unique_ptr<SignalsCollector>> collectors;
    collectors.push_back(std::move(av_signal_collector));
    collectors.push_back(std::move(hotfix_signal_collector));
    aggregator_ = std::make_unique<SignalsAggregatorImpl>(
        &mock_permission_service_, std::move(collectors));
  }

  void GrantUserPermission() {
    EXPECT_CALL(mock_permission_service_, CanCollectSignals(user_context_, _))
        .WillOnce([](const UserContext&,
                     UserPermissionService::CanCollectCallback callback) {
          std::move(callback).Run(UserPermission::kGranted);
        });
  }

  SignalsAggregationRequest CreateRequest() {
    SignalsAggregationRequest request;
    request.user_context = user_context_;
    return request;
  }

  std::unique_ptr<MockSignalsCollector> GetFakeCollector(
      SignalName signal_name) {
    auto mock_collector = std::make_unique<MockSignalsCollector>();
    ON_CALL(*mock_collector.get(), GetSupportedSignalNames())
        .WillByDefault(Return(std::unordered_set<SignalName>({signal_name})));

    ON_CALL(*mock_collector.get(), GetSignal(signal_name, _, _, _))
        .WillByDefault(Invoke([&](SignalName signal_name,
                                  const SignalsAggregationRequest& request,
                                  SignalsAggregationResponse& response,
                                  base::OnceClosure done_closure) {
          std::move(done_closure).Run();
        }));

    return mock_collector;
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockSignalsCollector> av_signal_collector_;
  raw_ptr<MockSignalsCollector> hotfix_signal_collector_;
  testing::StrictMock<MockUserPermissionService> mock_permission_service_;
  UserContext user_context_{kGaiaId};
  std::unique_ptr<SignalsAggregatorImpl> aggregator_;
};

// Tests that the aggregator will return an empty value when given an empty
// parameter dictionary.
TEST_F(SignalsAggregatorImplTest, GetSignals_NoSignal) {
  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(CreateRequest()), future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by one of the collectors.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Supported) {
  GrantUserPermission();

  auto request = CreateRequest();
  request.signal_names.emplace(SignalName::kAntiVirus);

  EXPECT_CALL(*av_signal_collector_, GetSupportedSignalNames()).Times(1);
  EXPECT_CALL(*av_signal_collector_,
              GetSignal(SignalName::kAntiVirus, request, _, _))
      .Times(1);

  EXPECT_CALL(*hotfix_signal_collector_, GetSignal(_, _, _, _)).Times(0);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  EXPECT_FALSE(response.top_level_error.has_value());
}

// Tests how the aggregator behaves when given a parameter with a single signal
// that no collector supports.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Unsupported) {
  GrantUserPermission();

  auto request = CreateRequest();
  request.signal_names.emplace(SignalName::kFileSystemInfo);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests how the aggregator behaves when encountering user permission errors.
TEST_F(SignalsAggregatorImplTest, GetSignals_InvalidUserPermissions) {
  std::map<UserPermission, SignalCollectionError> permission_to_error_map;
  permission_to_error_map[UserPermission::kUnaffiliated] =
      SignalCollectionError::kUnaffiliatedUser;
  permission_to_error_map[UserPermission::kMissingConsent] =
      SignalCollectionError::kConsentRequired;
  permission_to_error_map[UserPermission::kConsumerUser] =
      SignalCollectionError::kInvalidUser;
  permission_to_error_map[UserPermission::kUnknownUser] =
      SignalCollectionError::kInvalidUser;
  permission_to_error_map[UserPermission::kMissingUser] =
      SignalCollectionError::kInvalidUser;

  for (const auto& test_case : permission_to_error_map) {
    EXPECT_CALL(mock_permission_service_, CanCollectSignals(user_context_, _))
        .WillOnce(
            [&test_case](const UserContext&,
                         UserPermissionService::CanCollectCallback callback) {
              std::move(callback).Run(test_case.first);
            });

    // This value is not important for these test cases.
    auto request = CreateRequest();
    request.signal_names.emplace(SignalName::kAntiVirus);

    base::test::TestFuture<SignalsAggregationResponse> future;
    aggregator_->GetSignals(std::move(request), future.GetCallback());
    SignalsAggregationResponse response = future.Get();
    ASSERT_TRUE(response.top_level_error.has_value());
    EXPECT_EQ(response.top_level_error.value(), test_case.second);
  }
}

}  // namespace device_signals
