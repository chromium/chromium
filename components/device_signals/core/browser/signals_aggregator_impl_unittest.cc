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

constexpr char kFakeSignalName[] = "signal_name";
constexpr char kOtherFakeSignalName[] = "other_signal_name";

constexpr char kGaiaId[] = "gaia-id";

base::Value GetFakeSignalParameter() {
  return base::Value("some parameter needed by the signal collector");
}

base::Value GetFakeSignalValue() {
  base::Value::Dict fake_signal_value;
  fake_signal_value.Set("some_key", "some_value");
  return base::Value(std::move(fake_signal_value));
}

base::Value GetOtherFakeSignalParameter() {
  base::Value::Dict fake_signal_value;
  fake_signal_value.Set("some_file_path", "some/path");
  return base::Value(std::move(fake_signal_value));
}

base::Value GetOtherFakeSignalValue() {
  return base::Value("just a string");
}

std::unique_ptr<MockSignalsCollector> GetCollectorForFakeSignal() {
  auto mock_collector = std::make_unique<MockSignalsCollector>();
  ON_CALL(*mock_collector.get(), GetSupportedSignalNames())
      .WillByDefault(
          Return(std::unordered_set<std::string>({kFakeSignalName})));

  ON_CALL(*mock_collector.get(), GetSignal(kFakeSignalName, _, _))
      .WillByDefault(
          Invoke([](const std::string& signal_name, const base::Value& params,
                    SignalsCollector::GetSignalCallback callback) {
            EXPECT_EQ(params, GetFakeSignalParameter());
            std::move(callback).Run(GetFakeSignalValue());
          }));

  return mock_collector;
}

std::unique_ptr<MockSignalsCollector> GetCollectorForOtherFakeSignal() {
  auto mock_collector = std::make_unique<MockSignalsCollector>();
  ON_CALL(*mock_collector.get(), GetSupportedSignalNames())
      .WillByDefault(
          Return(std::unordered_set<std::string>({kOtherFakeSignalName})));

  ON_CALL(*mock_collector.get(), GetSignal(kOtherFakeSignalName, _, _))
      .WillByDefault(
          Invoke([](const std::string& signal_name, const base::Value& params,
                    SignalsCollector::GetSignalCallback callback) {
            EXPECT_EQ(params, GetOtherFakeSignalParameter());
            std::move(callback).Run(GetOtherFakeSignalValue());
          }));

  return mock_collector;
}

}  // namespace

class SignalsAggregatorImplTest : public testing::Test {
 protected:
  SignalsAggregatorImplTest() {
    auto fake_signal_collector = GetCollectorForFakeSignal();
    fake_signal_collector_ = fake_signal_collector.get();

    auto other_fake_signal_collector = GetCollectorForOtherFakeSignal();
    other_fake_signal_collector_ = other_fake_signal_collector.get();

    std::vector<std::unique_ptr<SignalsCollector>> collectors;
    collectors.push_back(std::move(fake_signal_collector));
    collectors.push_back(std::move(other_fake_signal_collector));
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

  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockSignalsCollector> fake_signal_collector_;
  raw_ptr<MockSignalsCollector> other_fake_signal_collector_;
  testing::StrictMock<MockUserPermissionService> mock_permission_service_;
  UserContext user_context_{kGaiaId};
  std::unique_ptr<SignalsAggregatorImpl> aggregator_;
};

// Tests that the aggregator will return an empty value when given an empty
// parameter dictionary.
TEST_F(SignalsAggregatorImplTest, GetSignals_NoSignal) {
  base::test::TestFuture<base::Value> future;
  base::Value::Dict empty_value;
  aggregator_->GetSignals(user_context_, std::move(empty_value),
                          future.GetCallback());
  EXPECT_EQ(future.Get(), base::Value(errors::kUnsupported));
}

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by one of the collectors.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Supported) {
  GrantUserPermission();

  base::Value::Dict parameters;
  parameters.Set(kFakeSignalName, GetFakeSignalParameter());

  EXPECT_CALL(*fake_signal_collector_, GetSupportedSignalNames()).Times(1);
  EXPECT_CALL(*fake_signal_collector_, GetSignal(kFakeSignalName, _, _))
      .Times(1);

  EXPECT_CALL(*other_fake_signal_collector_, GetSignal(_, _, _)).Times(0);

  base::test::TestFuture<base::Value> future;
  aggregator_->GetSignals(user_context_, std::move(parameters),
                          future.GetCallback());

  base::Value::Dict expected_value;
  expected_value.Set(kFakeSignalName, GetFakeSignalValue());

  EXPECT_EQ(future.Get(), base::Value(std::move(expected_value)));
}

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by another collector.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_SupportedOther) {
  GrantUserPermission();

  base::Value::Dict parameters;
  parameters.Set(kOtherFakeSignalName, GetOtherFakeSignalParameter());

  EXPECT_CALL(*other_fake_signal_collector_, GetSupportedSignalNames())
      .Times(1);
  EXPECT_CALL(*other_fake_signal_collector_,
              GetSignal(kOtherFakeSignalName, _, _))
      .Times(1);

  EXPECT_CALL(*fake_signal_collector_, GetSignal(_, _, _)).Times(0);

  base::test::TestFuture<base::Value> future;
  aggregator_->GetSignals(user_context_, std::move(parameters),
                          future.GetCallback());

  base::Value::Dict expected_value;
  expected_value.Set(kOtherFakeSignalName, GetOtherFakeSignalValue());

  EXPECT_EQ(future.Get(), base::Value(std::move(expected_value)));
}

// Tests how the aggregator behaves when given a parameter with a single signal
// that no collector supports.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Unsupported) {
  GrantUserPermission();

  base::Value::Dict parameters;
  parameters.Set("something unsupported", base::Value());

  base::test::TestFuture<base::Value> future;
  aggregator_->GetSignals(user_context_, std::move(parameters),
                          future.GetCallback());
  EXPECT_EQ(future.Get(), base::Value(errors::kUnsupported));
}

// Tests how the aggregator behaves when encountering user permission errors.
TEST_F(SignalsAggregatorImplTest, GetSignals_InvalidUserPermissions) {
  std::map<UserPermission, std::string> permission_to_error_map;
  permission_to_error_map[UserPermission::kUnaffiliated] =
      errors::kUnaffiliatedUser;
  permission_to_error_map[UserPermission::kMissingConsent] =
      errors::kConsentRequired;
  permission_to_error_map[UserPermission::kConsumerUser] = errors::kUnsupported;
  permission_to_error_map[UserPermission::kUnknownUser] = errors::kUnsupported;

  for (const auto& test_case : permission_to_error_map) {
    EXPECT_CALL(mock_permission_service_, CanCollectSignals(user_context_, _))
        .WillOnce(
            [&test_case](const UserContext&,
                         UserPermissionService::CanCollectCallback callback) {
              std::move(callback).Run(test_case.first);
            });

    // This value is not important for these test cases.
    base::Value::Dict parameters;
    parameters.Set("something unsupported", base::Value());

    base::test::TestFuture<base::Value> future;
    aggregator_->GetSignals(user_context_, std::move(parameters),
                            future.GetCallback());

    EXPECT_EQ(future.Get(), base::Value(test_case.second));
  }
}

}  // namespace device_signals
