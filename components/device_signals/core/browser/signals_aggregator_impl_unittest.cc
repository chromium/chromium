// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace {
constexpr char kGaiaId[] = "gaia-id";
}  // namespace
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void GrantUserPermission() {
    EXPECT_CALL(mock_permission_service_, CanUserCollectSignals(user_context_))
        .WillOnce(Return(UserPermission::kGranted));
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  std::unique_ptr<MockSignalsCollector> GetFakeCollector(
      SignalName signal_name) {
    auto mock_collector = std::make_unique<MockSignalsCollector>();
    ON_CALL(*mock_collector.get(), IsSignalSupported(_))
        .WillByDefault(Return(false));
    ON_CALL(*mock_collector.get(), IsSignalSupported(signal_name))
        .WillByDefault(Return(true));

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
  testing::StrictMock<MockUserPermissionService> mock_permission_service_;
  std::unique_ptr<SignalsAggregatorImpl> aggregator_;
  raw_ptr<MockSignalsCollector> av_signal_collector_;
  raw_ptr<MockSignalsCollector> hotfix_signal_collector_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  UserContext user_context_{kGaiaId};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  base::HistogramTester histogram_tester_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests that the aggregator will return an empty value when given an empty
// parameter dictionary.
TEST_F(SignalsAggregatorImplTest, GetSignalsForUser_NoSignal) {
  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignalsForUser(user_context_, SignalsAggregationRequest(),
                                 future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests that the aggregator will return an empty value when given a request
// with multiple signal names.
TEST_F(SignalsAggregatorImplTest, GetSignalsForUser_MultipleSignals) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kAntiVirus);
  request.signal_names.emplace(SignalName::kHotfixes);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignalsForUser(user_context_, request, future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by one of the collectors.
TEST_F(SignalsAggregatorImplTest, GetSignalsForUser_SingleSignal_Supported) {
  GrantUserPermission();

  auto expected_signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  EXPECT_CALL(*av_signal_collector_, IsSignalSupported(expected_signal_name))
      .Times(1);
  EXPECT_CALL(*av_signal_collector_,
              GetSignal(SignalName::kAntiVirus, request, _, _))
      .Times(1);

  EXPECT_CALL(*hotfix_signal_collector_, GetSignal(_, _, _, _)).Times(0);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignalsForUser(user_context_, std::move(request),
                                 future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  EXPECT_FALSE(response.top_level_error.has_value());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request", expected_signal_name, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.UserPermission", UserPermission::kGranted, 1);
}

// Tests how the aggregator behaves when given a parameter with a single signal
// that no collector supports.
TEST_F(SignalsAggregatorImplTest, GetSignalsForUser_SingleSignal_Unsupported) {
  GrantUserPermission();

  auto expected_signal_name = SignalName::kFileSystemInfo;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  EXPECT_CALL(*av_signal_collector_, IsSignalSupported(expected_signal_name))
      .WillOnce(Return(false));
  EXPECT_CALL(*hotfix_signal_collector_,
              IsSignalSupported(expected_signal_name))
      .WillOnce(Return(false));

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignalsForUser(user_context_, std::move(request),
                                 future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request", expected_signal_name, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.UserPermission", UserPermission::kGranted, 1);
}

// Tests how the aggregator behaves when encountering user permission errors.
TEST_F(SignalsAggregatorImplTest, GetSignalsForUser_InvalidUserPermissions) {
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

  uint16_t item_index = 0;
  for (const auto& test_case : permission_to_error_map) {
    EXPECT_CALL(mock_permission_service_, CanUserCollectSignals(user_context_))
        .WillOnce(Return(test_case.first));

    // This value is not important for these test cases.
    auto expected_signal_name = SignalName::kAntiVirus;
    SignalsAggregationRequest request;
    request.signal_names.emplace(expected_signal_name);

    base::test::TestFuture<SignalsAggregationResponse> future;
    aggregator_->GetSignalsForUser(user_context_, std::move(request),
                                   future.GetCallback());
    SignalsAggregationResponse response = future.Get();
    ASSERT_TRUE(response.top_level_error.has_value());
    EXPECT_EQ(response.top_level_error.value(), test_case.second);

    histogram_tester_.ExpectUniqueSample(
        "Enterprise.DeviceSignals.Collection.Request", expected_signal_name,
        ++item_index);
    histogram_tester_.ExpectBucketCount(
        "Enterprise.DeviceSignals.UserPermission", test_case.first, 1);
  }
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by one of the collectors. Specifically tests the API that
// does not accept a parameterized user context.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Supported) {
  EXPECT_CALL(mock_permission_service_, CanCollectSignals())
      .WillOnce(Return(UserPermission::kGranted));

  auto expected_signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  EXPECT_CALL(*av_signal_collector_, IsSignalSupported(expected_signal_name))
      .Times(1);
  EXPECT_CALL(*av_signal_collector_,
              GetSignal(SignalName::kAntiVirus, request, _, _))
      .Times(1);

  EXPECT_CALL(*hotfix_signal_collector_, GetSignal(_, _, _, _)).Times(0);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  EXPECT_FALSE(response.top_level_error.has_value());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request", expected_signal_name, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.UserPermission", UserPermission::kGranted, 1);
}

// Tests how the aggregator behaves when encountering user permission errors
// when called via the API that does not accept a parameterized user context.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_NoPermission) {
  EXPECT_CALL(mock_permission_service_, CanCollectSignals())
      .WillOnce(Return(UserPermission::kMissingConsent));

  // This value is not important for these test cases.
  auto expected_signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());
  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kConsentRequired);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request", expected_signal_name, 1);
  histogram_tester_.ExpectBucketCount("Enterprise.DeviceSignals.UserPermission",
                                      UserPermission::kMissingConsent, 1);
}

}  // namespace device_signals
