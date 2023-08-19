// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/churn/cohort_impl.h"

#include <memory>
#include "ash/constants/ash_features.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/private_computing/fake_private_computing_client.h"
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

class CohortImplTestBase : public testing::Test {
 public:
  CohortImplTestBase() = default;
  CohortImplTestBase(const CohortImplTestBase&) = delete;
  CohortImplTestBase& operator=(const CohortImplTestBase&) = delete;
  ~CohortImplTestBase() override = default;

  static psm_rlwe::PrivateMembershipRlweClientRegressionTestData*
  GetPsmTestData() {
    static base::NoDestructor<
        psm_rlwe::PrivateMembershipRlweClientRegressionTestData>
        data;
    return data.get();
  }

  static void CreatePsmTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
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

class CohortImplDirectCheckInTest : public CohortImplTestBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    CohortImplTestBase::SetUp();

    // PSM test data at index [5,9] contain negative check membership results.
    psm_test_case_ = utils::GetPsmTestCase(GetPsmTestData(), 5);
    ASSERT_FALSE(psm_test_case_.is_positive_membership_expected());

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        std::make_unique<FakePsmDelegate>(
            psm_test_case_.ec_cipher_key(), psm_test_case_.seed(),
            std::vector{psm_test_case_.plaintext_id()}));
    cohort_impl_ = std::make_unique<CohortImpl>(use_case_params_.get());
  }

  void TearDown() override {
    cohort_impl_.reset();
    use_case_params_.reset();
  }

  CohortImpl* GetCohortImpl() { return cohort_impl_.get(); }

  absl::optional<FresnelImportDataRequest>
  GenerateImportRequestBodyForTesting() {
    return cohort_impl_->GenerateImportRequestBody();
  }

  base::Time GetLastPingTimestamp() {
    return cohort_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    cohort_impl_->SetLastPingTimestamp(ts);
  }

  psm_rlwe_test GetPsmTestCase() { return psm_test_case_; }

 private:
  psm_rlwe_test psm_test_case_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<CohortImpl> cohort_impl_;
};

TEST_F(CohortImplDirectCheckInTest, QueryFeatureFlagDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      features::kDeviceActiveClientChurnCohortCheckMembership));
}

TEST_F(CohortImplDirectCheckInTest, ValidateBrandNewDeviceFlow) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetCohortImpl()->Run(base::DoNothing());

  // Validate FresnelImportRequest body is generated as expected.
  // active status 72351745 represents 0100010100 000000000000000001
  absl::optional<FresnelImportDataRequest> data =
      GenerateImportRequestBodyForTesting();
  EXPECT_EQ(data->import_data_size(), 1);
  FresnelImportData import_data = data->import_data().at(0);
  DeviceMetadata metadata = data->device_metadata();
  EXPECT_EQ(import_data.window_identifier(), "202301");
  EXPECT_FALSE(import_data.churn_cohort_metadata().is_first_active_in_cohort());
  EXPECT_EQ(import_data.churn_cohort_metadata().active_status_value(),
            72351745);

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), GetFakeTimeNow());
}

TEST_F(CohortImplDirectCheckInTest, GracefullyHandleImportResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  GetCohortImpl()->Run(base::DoNothing());

  // Set invalid import response body.
  SimulateImportResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since PSM flow failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
}

}  // namespace ash::report::device_metrics
