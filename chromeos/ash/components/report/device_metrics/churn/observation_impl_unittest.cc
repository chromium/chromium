// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/churn/observation_impl.h"

#include <memory>
#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/report/device_metrics/use_case/stub_psm_client_manager.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/test_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

class ObservationImplTestBase : public testing::Test {
 public:
  ObservationImplTestBase() = default;
  ObservationImplTestBase(const ObservationImplTestBase&) = delete;
  ObservationImplTestBase& operator=(const ObservationImplTestBase&) = delete;
  ~ObservationImplTestBase() override = default;

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

  // Generate a well-formed fake PSM network request and response bodies for
  // testing purposes.
  const std::string GetFresnelOprfResponse() {
    FresnelPsmRlweOprfResponse psm_oprf_response;
    *psm_oprf_response.mutable_rlwe_oprf_response() =
        psm_rlwe::PrivateMembershipRlweOprfResponse();
    return psm_oprf_response.SerializeAsString();
  }

  const std::string GetFresnelQueryResponse() {
    FresnelPsmRlweQueryResponse psm_query_response;
    *psm_query_response.mutable_rlwe_query_response() =
        psm_rlwe::PrivateMembershipRlweQueryResponse();
    return psm_query_response.SerializeAsString();
  }

  void SimulateOprfRequest(
      StubPsmClientManagerDelegate* delegate,
      const psm_rlwe::PrivateMembershipRlweOprfRequest& request) {
    delegate->set_oprf_request(request);
  }

  void SimulateQueryRequest(
      StubPsmClientManagerDelegate* delegate,
      const psm_rlwe::PrivateMembershipRlweQueryRequest& request) {
    delegate->set_query_request(request);
  }

  void SimulateMembershipResponses(
      StubPsmClientManagerDelegate* delegate,
      const private_membership::rlwe::RlweMembershipResponses&
          membership_responses) {
    delegate->set_membership_responses(membership_responses);
  }

  void SimulateOprfResponse(const std::string& serialized_response_body,
                            net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        utils::GetOprfRequestURL().spec(), serialized_response_body,
        response_code);

    task_environment_.RunUntilIdle();
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

class ObservationImplDirectCheckInTest : public ObservationImplTestBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    ObservationImplTestBase::SetUp();

    // |psm_client_delegate| is owned by |psm_client_manager_|.
    // Stub successful request payloads when created by the PSM client.
    std::unique_ptr<StubPsmClientManagerDelegate> psm_client_delegate =
        std::make_unique<StubPsmClientManagerDelegate>();
    SimulateOprfRequest(psm_client_delegate.get(),
                        psm_rlwe::PrivateMembershipRlweOprfRequest());
    SimulateQueryRequest(psm_client_delegate.get(),
                         psm_rlwe::PrivateMembershipRlweQueryRequest());
    SimulateMembershipResponses(psm_client_delegate.get(),
                                GetMembershipResponses());
    psm_client_manager_ =
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate));

    use_case_params_ = std::make_unique<UseCaseParameters>(
        GetFakeTimeNow(), kFakeChromeParameters, GetUrlLoaderFactory(),
        utils::kFakeHighEntropySeed, GetLocalState(),
        psm_client_manager_.get());
    observation_impl_ =
        std::make_unique<ObservationImpl>(use_case_params_.get());
  }

  void TearDown() override {
    observation_impl_.reset();
    use_case_params_.reset();
    psm_client_manager_.reset();
  }

  ObservationImpl* GetObservationImpl() { return observation_impl_.get(); }

  // Returns a single positive membership response.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses() {
    psm_rlwe::RlweMembershipResponses membership_responses;

    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry* entry =
        membership_responses.add_membership_responses();
    private_membership::MembershipResponse* membership_response =
        entry->mutable_membership_response();
    membership_response->set_is_member(true);

    return membership_responses;
  }

  base::Time GetLastPingTimestamp() {
    return observation_impl_->GetLastPingTimestamp();
  }

  void SetLastPingTimestamp(base::Time ts) {
    observation_impl_->SetLastPingTimestamp(ts);
  }

  std::optional<FresnelImportDataRequest> GenerateImportRequestBody() {
    return observation_impl_->GenerateImportRequestBodyForTesting();
  }

 private:
  std::unique_ptr<PsmClientManager> psm_client_manager_;
  std::unique_ptr<UseCaseParameters> use_case_params_;
  std::unique_ptr<ObservationImpl> observation_impl_;
};

TEST_F(ObservationImplDirectCheckInTest, QueryFeatureFlagDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      features::kDeviceActiveClientChurnObservationCheckMembership));
}

TEST_F(ObservationImplDirectCheckInTest, ValidateBrandNewDeviceFlow) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 111111111111111111
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72613887;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  // Execute observation reporting logic.
  GetObservationImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), cur_ts);
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
}

TEST_F(ObservationImplDirectCheckInTest,
       ValidateBrandNewDeviceFlowWithFailedCohort) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  // Simulate with default cohort local state values. This indicates cohort ping
  // was not sent successfully prior to reporting observation.
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            base::Time::UnixEpoch());
  EXPECT_EQ(GetLocalState()->GetInteger(
                prefs::kDeviceActiveLastKnownChurnActiveStatus),
            0);

  // Execute observation reporting logic.
  GetObservationImpl()->Run(base::DoNothing());

  // Expect observation import request failed.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
  EXPECT_FALSE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
  EXPECT_FALSE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
  EXPECT_FALSE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
}

TEST_F(ObservationImplDirectCheckInTest,
       GracefullyHandleImportResponseFailure) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 111111111111111111
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72613887;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  GetObservationImpl()->Run(base::DoNothing());

  // Set invalid import response body.
  SimulateImportResponse(std::string(), net::HTTP_REQUEST_TIMEOUT);

  // Not updated since the PSM import failed via. timeout.
  EXPECT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());
  EXPECT_FALSE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
  EXPECT_FALSE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
  EXPECT_FALSE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
}

TEST_F(ObservationImplDirectCheckInTest,
       ValidateRandomActiveStatusHistoryWithSuccessfulCohortPing) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 001010010010010101
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72393877;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  // Execute observation reporting logic.
  GetObservationImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), cur_ts);
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
}

TEST_F(ObservationImplDirectCheckInTest, ValidateNewDeviceChurnMetadata) {
  ASSERT_EQ(GetLastPingTimestamp(), base::Time::UnixEpoch());

  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 001010010010010101
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72393877;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  // Execute observation reporting logic.
  GetObservationImpl()->Run(base::DoNothing());

  // Return well formed response bodies for the pending network requests.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_EQ(GetLastPingTimestamp(), cur_ts);
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
  EXPECT_TRUE(GetLocalState()->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
}

TEST_F(ObservationImplDirectCheckInTest, GenerateImportRequestBody_Successful) {
  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 001010010010010101
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72393877;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  std::optional<FresnelImportDataRequest> import_request =
      GenerateImportRequestBody();
  ASSERT_TRUE(import_request.has_value());
  EXPECT_EQ(import_request.value().import_data_size(), 3);
}

TEST_F(ObservationImplDirectCheckInTest, GenerateImportRequestBody) {
  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 001010010010010101
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72393877;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  // Field under test.
  std::optional<FresnelImportDataRequest> import_request;

  // Failure to generate import request body.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);
  import_request = GenerateImportRequestBody();
  EXPECT_EQ(import_request->import_data_size(), 0);

  // Generates period 0 observation.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, false);
  import_request = GenerateImportRequestBody();
  EXPECT_EQ(import_request->import_data_size(), 1);

  // Generates period 0 and 1 observations.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, false);
  import_request = GenerateImportRequestBody();
  EXPECT_EQ(import_request->import_data_size(), 2);

  // Generates period 0, 1, and 2 observations.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, false);
  import_request = GenerateImportRequestBody();
  EXPECT_EQ(import_request->import_data_size(), 3);
}

TEST_F(ObservationImplDirectCheckInTest, GenerateObservationImportData) {
  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 001010010010010101
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72393877;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  // Generates period 0, 1, and 2 observations.
  std::optional<FresnelImportDataRequest> import_request =
      GenerateImportRequestBody();
  ASSERT_EQ(import_request->import_data_size(), 3);

  struct {
    int period;
    bool expected_monthly_active_status;
    bool expected_yearly_active_status;
    const std::string expected_first_active_week;
    const std::string expected_last_powerwash_week;
  } kTestCases[] = {
      {0, false, true, "UNKNOWN", "UNKNOWN"},
      {1, true, false, "UNKNOWN", "UNKNOWN"},
      {2, false, true, "UNKNOWN", "UNKNOWN"},
  };

  // Validate observation import data.
  FresnelImportData obs_data;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Period: " << test_case.period);
    obs_data = import_request->import_data(test_case.period);
    ASSERT_TRUE(obs_data.has_churn_observation_metadata());

    EXPECT_EQ(obs_data.churn_observation_metadata().monthly_active_status(),
              test_case.expected_monthly_active_status);
    EXPECT_EQ(obs_data.churn_observation_metadata().yearly_active_status(),
              test_case.expected_yearly_active_status);
    EXPECT_EQ(obs_data.churn_observation_metadata().first_active_week(),
              test_case.expected_first_active_week);
    EXPECT_EQ(obs_data.churn_observation_metadata().last_powerwash_week(),
              test_case.expected_last_powerwash_week);
  }
}

TEST_F(ObservationImplDirectCheckInTest, ObservationImportDataNewDeviceChurn) {
  // Validate metadata when new device churn feature is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kDeviceActiveClientChurnObservationNewDeviceMetadata},
      /*disabled_features*/ {});

  // Observation import will only go through if cohort imported successfully.
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 001010010010010101
  base::Time cur_ts = GetFakeTimeNow();
  int cur_value = 72393877;
  GetLocalState()->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                           cur_ts);
  GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                              cur_value);

  struct {
    const std::string activate_date_vpd;
    std::string first_active_week_obs_0;
    std::string first_active_week_obs_1;
    std::string first_active_week_obs_2;
  } kTestCases[] = {
      {"2023-01", "2023-01", "2023-01", "2023-01"},
      {"2022-36", "2022-36", "2022-36", "2022-36"},

      /* ActivateDate is > 4 months old */
      {"2022-35", "", "", ""},
      /* ActivateDate is undefined */
      {std::string(), "UNKNOWN", "UNKNOWN", "UNKNOWN"},
      /* ActivateDate is incorrectly formatted */
      {"123-456", "UNKNOWN", "UNKNOWN", "UNKNOWN"},
  };

  // Initialize fake statistics provider to test various activate date inputs.
  system::FakeStatisticsProvider statistics_provider;
  system::StatisticsProvider::SetTestProvider(&statistics_provider);

  // Validate if new device churn metadata is attached in observation ping.
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test input Activate Date VPD as: "
                                    << test_case.activate_date_vpd);

    // Set ActivateDate field in FakeStatisticsProvider before reading
    // ActivateDate in the GenerateImportRequestBody method.
    statistics_provider.SetMachineStatistic(system::kActivateDateKey,
                                            test_case.activate_date_vpd);

    // Method under test
    std::optional<FresnelImportDataRequest> import_request =
        GenerateImportRequestBody();
    ASSERT_EQ(import_request->import_data_size(), 3);

    auto obs_data_0 = import_request->import_data(0);
    auto obs_data_1 = import_request->import_data(1);
    auto obs_data_2 = import_request->import_data(2);

    ASSERT_TRUE(obs_data_0.has_churn_observation_metadata());
    ASSERT_TRUE(obs_data_1.has_churn_observation_metadata());
    ASSERT_TRUE(obs_data_2.has_churn_observation_metadata());

    EXPECT_EQ(obs_data_0.churn_observation_metadata().first_active_week(),
              test_case.first_active_week_obs_0);
    EXPECT_EQ(obs_data_1.churn_observation_metadata().first_active_week(),
              test_case.first_active_week_obs_1);
    EXPECT_EQ(obs_data_2.churn_observation_metadata().first_active_week(),
              test_case.first_active_week_obs_2);
  }

  // Set statistics test provider to nullptr after tests.
  system::StatisticsProvider::SetTestProvider(nullptr);
}

}  // namespace ash::report::device_metrics
