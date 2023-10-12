// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/actives/twenty_eight_day_impl.h"

#include <memory>
#include "ash/constants/ash_features.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/report/device_metrics/use_case/fake_psm_delegate.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/test_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"

namespace psm_rlwe = private_membership::rlwe;

using psm_rlwe_test =
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase;

namespace ash::report::device_metrics {

class TwentyEightDayImplBase : public testing::Test {
 public:
  TwentyEightDayImplBase() = default;
  TwentyEightDayImplBase(const TwentyEightDayImplBase&) = delete;
  TwentyEightDayImplBase& operator=(const TwentyEightDayImplBase&) = delete;
  ~TwentyEightDayImplBase() override = default;

  static psm_rlwe::PrivateMembershipRlweClientRegressionTestData*
  GetPsmTestData() {
    static base::NoDestructor<
        psm_rlwe::PrivateMembershipRlweClientRegressionTestData>
        data;
    return data.get();
  }

  static void CreatePsmTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
    const base::FilePath kPsmTestDataPath =
        src_root_dir.AppendASCII("third_party")
            .AppendASCII("private_membership")
            .AppendASCII("src")
            .AppendASCII("internal")
            .AppendASCII("testing")
            .AppendASCII("regression_test_data")
            .AppendASCII("test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPsmTestDataPath));
    ASSERT_TRUE(utils::ParseProtoFromFile(kPsmTestDataPath, GetPsmTestData()));

    ASSERT_EQ(GetPsmTestData()->test_cases_size(), utils::kPsmTestCaseSize);
  }

  static void SetUpTestSuite() {
    // Initialize PSM test data used to fake check membership flow.
    CreatePsmTestData();
  }

  void SetUp() override {
    // Set the mock time to |kFakeTimeNow|.
    base::Time ts;
    ASSERT_TRUE(base::Time::FromUTCString(utils::kFakeTimeNowString, &ts));
    task_environment_.AdvanceClock(ts - base::Time::Now());

    // Register all related local state prefs.
    ReportController::RegisterPrefs(local_state_.registry());

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);
  }

 protected:
  base::Time GetFakeTimeNow() { return base::Time::Now(); }

  PrefService* GetLocalState() { return &local_state_; }

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return test_shared_loader_factory_;
  }

  // Generate a well-formed fake PSM network response body for testing purposes.
  const std::string GetFresnelOprfResponse(const psm_rlwe_test& test_case) {
    FresnelPsmRlweOprfResponse psm_oprf_response;
    *psm_oprf_response.mutable_rlwe_oprf_response() = test_case.oprf_response();
    return psm_oprf_response.SerializeAsString();
  }

  void SimulateOprfResponse(const std::string& serialized_response_body,
                            net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetOprfRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
  }

  const std::string GetFresnelQueryResponse(const psm_rlwe_test& test_case) {
    FresnelPsmRlweQueryResponse psm_query_response;
    *psm_query_response.mutable_rlwe_query_response() =
        test_case.query_response();
    return psm_query_response.SerializeAsString();
  }

  // Generate a well-formed fake PSM network response body for testing purposes.
  void SimulateQueryResponse(const std::string& serialized_response_body,
                             net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetQueryRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
  }

  void SimulateImportResponse(const std::string& serialized_response_body,
                              net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetImportRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

class TwentyEightDayImplDirectCheckIn : public TwentyEightDayImplBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    TwentyEightDayImplBase::SetUp();

    // PSM test data at index [5,9] contain negative check membership results.
    psm_test_case_ = utils::GetPsmTestCase(GetPsmTestData(), 5);
    ASSERT_FALSE(psm_test_case_.is_positive_membership_expected());

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        std::make_unique<FakePsmDelegate>(
            psm_test_case_.ec_cipher_key(), psm_test_case_.seed(),
            std::vector{psm_test_case_.plaintext_id()}));
    twenty_eight_day_impl_ =
        std::make_unique<TwentyEightDayImpl>(use_case_params_.get());
  }

  void TearDown() override {
    twenty_eight_day_impl_.reset();
    use_case_params_.reset();
  }

  TwentyEightDayImpl* GetTwentyEightDayImpl() {
    return twenty_eight_day_impl_.get();
  }

  base::Time GetLastPingTimestamp() {
    return twenty_eight_day_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    twenty_eight_day_impl_->SetLastPingTimestamp(ts);
  }

  psm_rlwe_test GetPsmTestCase() { return psm_test_case_; }

  absl::optional<FresnelImportDataRequest>
  GenerateImportRequestBodyForTesting() {
    return twenty_eight_day_impl_->GenerateImportRequestBody();
  }

 private:
  psm_rlwe_test psm_test_case_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<TwentyEightDayImpl> twenty_eight_day_impl_;
};

TEST_F(TwentyEightDayImplDirectCheckIn, QueryFeatureFlagDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      features::kDeviceActiveClient28DayActiveCheckMembership));
}

TEST_F(TwentyEightDayImplDirectCheckIn, ValidateBrandNewDeviceFlow) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow());
}

TEST_F(TwentyEightDayImplDirectCheckIn, GracefullyHandleImportResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Set invalid import response body.
  SimulateImportResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
}

TEST_F(TwentyEightDayImplDirectCheckIn, ReportAgainAfterOneDay) {
  // Set last ping ts to be 1 day before the current fake time.
  base::Time ts = GetFakeTimeNow();
  SetLastPingTimestamp(ts - base::Days(1));

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Validate import request data that will be sent.
  auto request_body = GenerateImportRequestBodyForTesting();
  EXPECT_EQ(request_body->import_data_size(), 1);
  EXPECT_EQ(request_body->import_data().at(0).window_identifier(), "20230128");

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), ts);
}

TEST_F(TwentyEightDayImplDirectCheckIn, ReportAgainAfterOneWeek) {
  // Set last ping ts to be 7 days before the current fake time.
  base::Time ts = GetFakeTimeNow();
  SetLastPingTimestamp(ts - base::Days(7));

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Validate import request data that will be sent.
  auto request_body = GenerateImportRequestBodyForTesting();
  EXPECT_EQ(request_body->import_data_size(), 7);
  EXPECT_EQ(request_body->import_data().at(0).window_identifier(), "20230122");
  EXPECT_EQ(request_body->import_data().at(6).window_identifier(), "20230128");

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), ts);
}

TEST_F(TwentyEightDayImplDirectCheckIn, ReportAgainAfterOneMonth) {
  // Set last ping ts to be 1 month before the current fake time.
  base::Time ts = GetFakeTimeNow();
  SetLastPingTimestamp(ts - base::Days(31));

  GetTwentyEightDayImpl()->Run(base::DoNothing());

  // Validate import request data that will be sent.
  auto request_body = GenerateImportRequestBodyForTesting();
  EXPECT_EQ(request_body->import_data_size(), 28);
  EXPECT_EQ(request_body->import_data().at(0).window_identifier(), "20230101");
  EXPECT_EQ(request_body->import_data().at(27).window_identifier(), "20230128");

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), ts);
}

}  // namespace ash::report::device_metrics
