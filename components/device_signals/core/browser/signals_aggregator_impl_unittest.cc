// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
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
#include "google_apis/gaia/gaia_id.h"
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
constexpr GaiaId::Literal kGaiaId("gaia-id");
}  // namespace
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class SignalsAggregatorImplTest : public testing::Test {
 protected:
  SignalsAggregatorImplTest() {
    auto settings_signal_collector =
        GetFakeCollector(SignalName::kSystemSettings);
    settings_signal_collector_ = settings_signal_collector.get();

    auto files_signal_collector = GetFakeCollector(SignalName::kFileSystemInfo);
    files_signal_collector_ = files_signal_collector.get();

    auto av_signal_collector = GetFakeCollector(SignalName::kAntiVirus);
    av_signal_collector_ = av_signal_collector.get();

    std::vector<std::unique_ptr<SignalsCollector>> collectors;
    collectors.push_back(std::move(settings_signal_collector));
    collectors.push_back(std::move(files_signal_collector));
    collectors.push_back(std::move(av_signal_collector));
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

    ON_CALL(*mock_collector.get(), GetSignal(signal_name, _, _, _, _))
        .WillByDefault(
            Invoke([&](SignalName signal_name, UserPermission permission,
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
  // Collectors that requires user consent.
  raw_ptr<MockSignalsCollector> settings_signal_collector_;
  raw_ptr<MockSignalsCollector> files_signal_collector_;
  // Collectors that does not require user consent.
  raw_ptr<MockSignalsCollector> av_signal_collector_;

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

  auto expected_signal_name = SignalName::kSystemSettings;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  EXPECT_CALL(*settings_signal_collector_,
              IsSignalSupported(expected_signal_name))
      .Times(1);
  EXPECT_CALL(*settings_signal_collector_,
              GetSignal(expected_signal_name, _, _, _, _))
      .Times(1);

  EXPECT_CALL(*files_signal_collector_, GetSignal(_, _, _, _, _)).Times(0);

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

  auto expected_signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  EXPECT_CALL(*settings_signal_collector_,
              IsSignalSupported(expected_signal_name))
      .WillOnce(Return(false));
  EXPECT_CALL(*files_signal_collector_, IsSignalSupported(expected_signal_name))
      .WillOnce(Return(false));

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignalsForUser(user_context_, std::move(request),
                                 future.GetCallback());

  EXPECT_FALSE(future.Get().top_level_error.has_value());

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

  EXPECT_CALL(mock_permission_service_, CanCollectReportSignals())
      .WillOnce(Return(UserPermission::kGranted));

  auto expected_signal_name = SignalName::kSystemSettings;
  SignalsAggregationRequest request, report_request;
  request.signal_names.emplace(expected_signal_name);
  report_request.signal_names.emplace(expected_signal_name);
  report_request.trigger = Trigger::kSignalsReport;

  EXPECT_CALL(*settings_signal_collector_,
              IsSignalSupported(expected_signal_name))
      .Times(2);
  EXPECT_CALL(
      *settings_signal_collector_,
      GetSignal(expected_signal_name, UserPermission::kGranted, request, _, _))
      .Times(1);
  EXPECT_CALL(*settings_signal_collector_,
              GetSignal(expected_signal_name, UserPermission::kGranted,
                        report_request, _, _))
      .Times(1);

  EXPECT_CALL(*files_signal_collector_, GetSignal(_, _, _, _, _)).Times(0);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  EXPECT_FALSE(response.top_level_error.has_value());

  base::test::TestFuture<SignalsAggregationResponse> report_future;
  aggregator_->GetSignals(std::move(report_request),
                          report_future.GetCallback());

  SignalsAggregationResponse report_response = report_future.Get();
  EXPECT_FALSE(report_response.top_level_error.has_value());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.SignalsCount", 1, 2);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.Request", expected_signal_name, 2);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.UserPermission", UserPermission::kGranted, 2);
}

// Tests how the aggregator behaves when encountering user permission errors
// other than missing consent when called via the API that does not accept a
// parameterized user context.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_NoPermission) {
  EXPECT_CALL(mock_permission_service_, CanCollectSignals())
      .WillOnce(Return(UserPermission::kConsumerUser));

  // This value is not important for these test cases.
  auto expected_signal_name = SignalName::kSystemSettings;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());
  SignalsAggregationResponse response = future.Get();
  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kInvalidUser);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.SignalsCount", 1, 1);
  histogram_tester_.ExpectBucketCount("Enterprise.DeviceSignals.UserPermission",
                                      UserPermission::kConsumerUser, 1);
}

// Tests how the aggregator behaves when encountering `kMissingConsent` user
// permission when called via the API that does not accept a parameterized user
// context.
// When consent is missing, collectors should still be called and make the
// decision on which signals to collect (if any).
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_MissingConsent) {
  EXPECT_CALL(mock_permission_service_, CanCollectSignals())
      .WillOnce(Return(UserPermission::kMissingConsent));

  auto expected_signal_name_with_consent = SignalName::kSystemSettings;
  auto expected_signal_name_regardless_consent = SignalName::kAntiVirus;
  SignalsAggregationRequest request;
  request.signal_names.emplace(expected_signal_name_with_consent);
  request.signal_names.emplace(expected_signal_name_regardless_consent);

  // Trying to collect settings signals
  EXPECT_CALL(*settings_signal_collector_,
              IsSignalSupported(expected_signal_name_with_consent))
      .Times(1);

  EXPECT_CALL(*settings_signal_collector_,
              GetSignal(expected_signal_name_with_consent,
                        UserPermission::kMissingConsent, request, _, _))
      .Times(1);

  // Trying to collect av signals
  EXPECT_CALL(*settings_signal_collector_,
              IsSignalSupported(expected_signal_name_regardless_consent))
      .Times(1);
  EXPECT_CALL(*av_signal_collector_,
              IsSignalSupported(expected_signal_name_regardless_consent))
      .Times(1);
  EXPECT_CALL(*av_signal_collector_,
              GetSignal(expected_signal_name_regardless_consent,
                        UserPermission::kMissingConsent, request, _, _))
      .Times(1);

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(std::move(request), future.GetCallback());

  SignalsAggregationResponse response = future.Get();
  ASSERT_FALSE(response.top_level_error.has_value());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Collection.SignalsCount", 2, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.DeviceSignals.Collection.Request",
      expected_signal_name_with_consent, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.DeviceSignals.Collection.Request",
      expected_signal_name_regardless_consent, 1);
  histogram_tester_.ExpectBucketCount("Enterprise.DeviceSignals.UserPermission",
                                      UserPermission::kMissingConsent, 1);
}

// Tests that the aggregator will return an empty value when given an empty
// parameter dictionary.
TEST_F(SignalsAggregatorImplTest, GetSignals_NoSignal) {
  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(SignalsAggregationRequest(), future.GetCallback());

  const auto& response = future.Get();
  EXPECT_FALSE(response.top_level_error);
  EXPECT_FALSE(response.agent_signals_response);
  EXPECT_FALSE(response.settings_response);
  EXPECT_FALSE(response.file_system_info_response);
#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(response.av_signal_response);
  EXPECT_FALSE(response.hotfix_signal_response);
#endif  // BUILDFLAG(IS_WIN)
}

// Tests that the aggregator will return an empty value when given a request
// with multiple signal names.
TEST_F(SignalsAggregatorImplTest, GetSignals_MultipleSignals_Supported) {
  EXPECT_CALL(mock_permission_service_, CanCollectSignals())
      .WillOnce(Return(UserPermission::kGranted));

  SignalsAggregationRequest request;
  request.signal_names.emplace(SignalName::kSystemSettings);
  request.signal_names.emplace(SignalName::kFileSystemInfo);
#if BUILDFLAG(IS_WIN)
  request.signal_names.emplace(SignalName::kAntiVirus);
#endif  // BUILDFLAG(IS_WIN)

  FileSystemItem file_item;
  ASSERT_TRUE(base::GetCurrentDirectory(&file_item.file_path));
  FileSystemInfoResponse files_response;
  files_response.file_system_items.push_back(std::move(file_item));

  SettingsItem settings_item;
  settings_item.path = "some_path";
  settings_item.key = "some_key";
  SettingsResponse settings_response;
  settings_response.settings_items.push_back(std::move(settings_item));

  EXPECT_CALL(
      *settings_signal_collector_,
      GetSignal(SignalName::kSystemSettings, UserPermission::kGranted, _, _, _))
      .WillOnce(Invoke([&](SignalName signal_name, UserPermission permission,
                           const SignalsAggregationRequest& request,
                           SignalsAggregationResponse& response,
                           base::OnceClosure done_closure) {
        response.settings_response = settings_response;
        std::move(done_closure).Run();
      }));
  EXPECT_CALL(
      *files_signal_collector_,
      GetSignal(SignalName::kFileSystemInfo, UserPermission::kGranted, _, _, _))
      .WillOnce(Invoke([&](SignalName signal_name, UserPermission permission,
                           const SignalsAggregationRequest& request,
                           SignalsAggregationResponse& response,
                           base::OnceClosure done_closure) {
        response.file_system_info_response = files_response;
        std::move(done_closure).Run();
      }));

#if BUILDFLAG(IS_WIN)
  AvProduct av_product;
  av_product.display_name = "some_name";
  av_product.product_id = "some_id";
  av_product.state = device_signals::AvProductState::kOn;
  AntiVirusSignalResponse av_response;
  av_response.av_products.push_back(av_product);

  EXPECT_CALL(
      *av_signal_collector_,
      GetSignal(SignalName::kAntiVirus, UserPermission::kGranted, _, _, _))
      .WillOnce(Invoke([&](SignalName signal_name, UserPermission permission,
                           const SignalsAggregationRequest& request,
                           SignalsAggregationResponse& response,
                           base::OnceClosure done_closure) {
        response.av_signal_response = av_response;
        std::move(done_closure).Run();
      }));
#endif  // BUILDFLAG(IS_WIN)

  base::test::TestFuture<SignalsAggregationResponse> future;
  aggregator_->GetSignals(request, future.GetCallback());

  const auto& response = future.Get();
  EXPECT_FALSE(response.top_level_error);
  EXPECT_FALSE(response.agent_signals_response);
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(response.av_signal_response);
  EXPECT_EQ(response.av_signal_response->av_products.size(), 1U);
  EXPECT_FALSE(response.hotfix_signal_response);
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(response.settings_response);
  EXPECT_EQ(response.settings_response->settings_items.size(), 1U);
  ASSERT_TRUE(response.file_system_info_response);
  EXPECT_EQ(response.file_system_info_response->file_system_items.size(), 1U);
}

}  // namespace device_signals
