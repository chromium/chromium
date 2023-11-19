// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/report_controller.h"

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/report/device_metrics/use_case/stub_psm_client_manager.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/test_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace psm_rlwe = private_membership::rlwe;

using pc_preserved_file_test =
    private_computing::PrivateComputingClientRegressionTestData;
namespace ash::report::device_metrics {

class ReportControllerTestBase : public testing::Test {
 public:
  static private_computing::PrivateComputingClientRegressionTestData*
  GetPreservedFileTestData() {
    static base::NoDestructor<
        private_computing::PrivateComputingClientRegressionTestData>
        preserved_file_test_data;
    return preserved_file_test_data.get();
  }

  static void CreatePreservedFileTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
    const base::FilePath kPrivateComputingTestDataPath =
        src_root_dir.AppendASCII("chromeos")
            .AppendASCII("ash")
            .AppendASCII("components")
            .AppendASCII("report")
            .AppendASCII("device_metrics")
            .AppendASCII("testing")
            .AppendASCII("preserved_file_test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPrivateComputingTestDataPath));
    ASSERT_TRUE(utils::ParseProtoFromFile(kPrivateComputingTestDataPath,
                                          GetPreservedFileTestData()));

    // Note that the test cases can change since it's read from the binary pb.
    ASSERT_EQ(GetPreservedFileTestData()->test_cases_size(),
              utils::kPreservedFileTestCaseSize);
  }

  static void SetUpTestSuite() {
    // Initialize preserved file test data.
    CreatePreservedFileTestData();
  }

  void SetUp() override {
    // Set the mock time to |kFakeTimeNow|.
    base::Time ts;
    ASSERT_TRUE(
        base::Time::FromUTCString(utils::kFakeTimeNowUnadjustedString, &ts));
    task_environment_.AdvanceClock(ts - base::Time::Now());

    // Set up any necessary dependencies or objects before each test
    PrivateComputingClient::InitializeFake();
    SessionManagerClient::InitializeFake();
    SystemClockClient::InitializeFake();

    // Set a fake psm device active secret that is required to report use cases.
    GetFakeSessionManagerClient()->set_psm_device_active_secret(
        utils::kFakeHighEntropySeed);

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    ReportController::RegisterPrefs(local_state_.registry());

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    // Network is not connected on device yet.
    SetWifiNetworkState(shill::kStateNoConnectivity);
  }

  void TearDown() override {
    // Shutdown fake clients in reverse order.
    SystemClockClient::Shutdown();
    SessionManagerClient::Shutdown();
    PrivateComputingClient::Shutdown();
  }

 protected:
  PrefService* GetLocalState() { return &local_state_; }

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return test_shared_loader_factory_;
  }

  PrivateComputingClient::TestInterface* GetPrivateComputingTestInterface() {
    return PrivateComputingClient::Get()->GetTestInterface();
  }

  FakeSessionManagerClient* GetFakeSessionManagerClient() {
    return FakeSessionManagerClient::Get();
  }

  SystemClockClient::TestInterface* GetSystemClockTestInterface() {
    return SystemClockClient::Get()->GetTestInterface();
  }

  void ResetLocalStateForTesting() {
    const base::Time unix_epoch = base::Time::UnixEpoch();
    GetLocalState()->SetTime(
        prefs::kDeviceActiveLastKnown1DayActivePingTimestamp, unix_epoch);
    GetLocalState()->SetTime(
        prefs::kDeviceActiveLastKnown28DayActivePingTimestamp, unix_epoch);
    GetLocalState()->SetTime(
        prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp, unix_epoch);
    GetLocalState()->SetTime(
        prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp, unix_epoch);
    GetLocalState()->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                                0);
    GetLocalState()->SetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, false);
    GetLocalState()->SetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, false);
    GetLocalState()->SetBoolean(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, false);
  }

  void ForwardClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
    task_environment_.RunUntilIdle();
  }

  void SetWifiNetworkState(std::string network_state) {
    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \""
       << "wifi_guid"
       << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateIdle << "\""
       << "}";
    std::string wifi_network_service_path_ =
        network_handler_test_helper_.ConfigureService(ss.str());
    network_handler_test_helper_.SetServiceProperty(wifi_network_service_path_,
                                                    shill::kStateProperty,
                                                    base::Value(network_state));
    task_environment_.RunUntilIdle();
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
  NetworkHandlerTestHelper network_handler_test_helper_;
};

class ReportControllerSimpleFlowTest : public ReportControllerTestBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
  };

  void SetUp() override {
    ReportControllerTestBase::SetUp();

    // Default network to being synchronized and available.
    GetSystemClockTestInterface()->SetServiceIsAvailable(true);
    GetSystemClockTestInterface()->SetNetworkSynchronized(true);

    // Default preserved file DBus operations to be empty.
    GetPrivateComputingTestInterface()->SetGetLastPingDatesStatusResponse(
        private_computing::GetStatusResponse());
    GetPrivateComputingTestInterface()->SetSaveLastPingDatesStatusResponse(
        private_computing::SaveStatusResponse());

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

    report_controller_ = std::make_unique<ReportController>(
        kFakeChromeParameters, GetLocalState(), GetUrlLoaderFactory(),
        base::Time(), base::BindRepeating([]() { return base::Minutes(1); }),
        base::BindRepeating(
            []() { return policy::DeviceMode::DEVICE_MODE_NOT_SET; }),
        base::BindRepeating([]() { return policy::MarketSegment::UNKNOWN; }),
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate)));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    report_controller_.reset();

    // Shutdown dependency clients after |report_controller_| is destroyed.
    ReportControllerTestBase::TearDown();
  }

  ReportController* GetReportController() { return report_controller_.get(); }

 protected:
  // Returns a single negative membership response.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses() {
    psm_rlwe::RlweMembershipResponses membership_responses;

    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry* entry =
        membership_responses.add_membership_responses();
    private_membership::MembershipResponse* membership_response =
        entry->mutable_membership_response();
    membership_response->set_is_member(false);

    return membership_responses;
  }

 private:
  std::unique_ptr<ReportController> report_controller_;
};

TEST_F(ReportControllerSimpleFlowTest, ValidateSingletonObject) {
  // The Get() method should return a valid singleton instance.
  ReportController* instance = ReportController::Get();
  EXPECT_NE(instance, nullptr);
  EXPECT_EQ(instance, GetReportController());
}

TEST_F(ReportControllerSimpleFlowTest, CompleteFlowOnFreshDevice) {
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            base::Time::UnixEpoch());
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            base::Time::UnixEpoch());
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            base::Time::UnixEpoch());
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp),
            base::Time::UnixEpoch());
  EXPECT_EQ(
      GetLocalState()->GetValue(prefs::kDeviceActiveLastKnownChurnActiveStatus),
      0);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0),
            false);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1),
            false);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2),
            false);

  // Start reporting sequence.
  SetWifiNetworkState(shill::kStateOnline);

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for 28DA use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Cohort use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Observation use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Ensure local state values are updated as expected.
  base::Time pst_adjusted_ts;
  ASSERT_TRUE(
      base::Time::FromUTCString(utils::kFakeTimeNowString, &pst_adjusted_ts));
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(
      GetLocalState()->GetValue(prefs::kDeviceActiveLastKnownChurnActiveStatus),
      72351745);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0),
            true);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1),
            true);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2),
            true);
}

TEST_F(ReportControllerSimpleFlowTest, DeviceFlowAcrossOneDay) {
  // Start reporting sequence.
  SetWifiNetworkState(shill::kStateOnline);

  EXPECT_TRUE(GetReportController()->IsDeviceReportingForTesting());

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for 28DA use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Cohort use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Observation use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Update mock time to be 1 day ahead.
  base::TimeDelta day_delta = base::Days(1);
  task_environment_.AdvanceClock(day_delta);

  // Expected local state timestamp after updating clock 1 day ahead.
  base::Time ts;
  ASSERT_TRUE(base::Time::FromUTCString(utils::kFakeTimeNowString, &ts));
  base::Time updated_ts = ts + day_delta;

  // Trigger reporting use case sequence.
  SetWifiNetworkState(shill::kStateNoConnectivity);
  SetWifiNetworkState(shill::kStateOnline);

  // 1DA will need to import in the new day.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // 28DA will need to import in the new day.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Ensure local state values are updated as expected.
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp),
            ts);
}

TEST_F(ReportControllerSimpleFlowTest, DeviceFlowAcrossOneWeek) {
  // Start reporting sequence.
  SetWifiNetworkState(shill::kStateOnline);

  EXPECT_TRUE(GetReportController()->IsDeviceReportingForTesting());

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for 28DA use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Cohort use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Observation use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Update mock time to be 7 days ahead.
  base::TimeDelta week_delta = base::Days(7);
  task_environment_.AdvanceClock(week_delta);

  // Expected local state timestamp after updating clock 7 days ahead.
  base::Time ts;
  ASSERT_TRUE(base::Time::FromUTCString(utils::kFakeTimeNowString, &ts));
  base::Time updated_ts = ts + week_delta;

  // Trigger reporting use case sequence.
  SetWifiNetworkState(shill::kStateNoConnectivity);
  SetWifiNetworkState(shill::kStateOnline);

  // 1DA will need to import in the new day.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // 28DA will need to import in the new day.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Ensure local state values are updated as expected.
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp),
            ts);
}

TEST_F(ReportControllerSimpleFlowTest, DeviceFlowAcrossOneMonth) {
  // Start reporting sequence.
  SetWifiNetworkState(shill::kStateOnline);

  EXPECT_TRUE(GetReportController()->IsDeviceReportingForTesting());

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for 28DA use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Cohort use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Observation use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Update mock time to be 1 month ahead.
  base::TimeDelta month_delta = base::Days(31);
  task_environment_.AdvanceClock(month_delta);

  // Expected local state timestamp after updating clock.
  base::Time ts;
  ASSERT_TRUE(base::Time::FromUTCString(utils::kFakeTimeNowString, &ts));
  base::Time updated_ts = ts + month_delta;

  // Trigger reporting use case sequence.
  SetWifiNetworkState(shill::kStateNoConnectivity);
  SetWifiNetworkState(shill::kStateOnline);

  // 1DA will need to import in the new day.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // 28DA will need to import in the new month.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Cohort will need to import in the new month.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Observation will need to import in the new month.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Ensure local state values are updated as expected.
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            updated_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp),
            updated_ts);
  EXPECT_EQ(
      GetLocalState()->GetValue(prefs::kDeviceActiveLastKnownChurnActiveStatus),
      72613891);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0),
            true);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1),
            true);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2),
            true);
}

class ReportControllerPreservedFileReadWriteSuccessTest
    : public ReportControllerTestBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */
  };

  void SetUp() override {
    ReportControllerTestBase::SetUp();

    // Default network to being synchronized and available.
    GetSystemClockTestInterface()->SetServiceIsAvailable(true);
    GetSystemClockTestInterface()->SetNetworkSynchronized(true);

    // Default preserved file DBus operations to retrieve successfully.
    pc_preserved_file_test::TestCase test = utils::GetPreservedFileTestCase(
        GetPreservedFileTestData(),
        pc_preserved_file_test::TestName::
            PrivateComputingClientRegressionTestData_TestName_GET_SUCCESS_SAVE_SUCCESS);
    GetPrivateComputingTestInterface()->SetGetLastPingDatesStatusResponse(
        test.get_response());
    GetPrivateComputingTestInterface()->SetSaveLastPingDatesStatusResponse(
        test.save_response());

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

    report_controller_ = std::make_unique<ReportController>(
        kFakeChromeParameters, GetLocalState(), GetUrlLoaderFactory(),
        base::Time(), base::BindRepeating([]() { return base::Minutes(1); }),
        base::BindRepeating(
            []() { return policy::DeviceMode::DEVICE_MODE_NOT_SET; }),
        base::BindRepeating([]() { return policy::MarketSegment::UNKNOWN; }),
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate)));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    report_controller_.reset();

    // Shutdown dependency clients after |report_controller_| is destroyed.
    ReportControllerTestBase::TearDown();
  }

 protected:
  ReportController* GetReportController() { return report_controller_.get(); }

  // Returns a single negative membership response.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses() {
    psm_rlwe::RlweMembershipResponses membership_responses;

    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry* entry =
        membership_responses.add_membership_responses();
    private_membership::MembershipResponse* membership_response =
        entry->mutable_membership_response();
    membership_response->set_is_member(false);

    return membership_responses;
  }

 private:
  std::unique_ptr<ReportController> report_controller_;
};

TEST_F(ReportControllerPreservedFileReadWriteSuccessTest, PreservedFileRead) {
  // Local state prefs are updated by reading the preserved file.
  base::Time pst_adjusted_ts;
  ASSERT_TRUE(
      base::Time::FromUTCString(utils::kFakeTimeNowString, &pst_adjusted_ts));
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp),
            pst_adjusted_ts);
  EXPECT_EQ(
      GetLocalState()->GetValue(prefs::kDeviceActiveLastKnownChurnActiveStatus),
      72351745);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0),
            true);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1),
            true);
  EXPECT_EQ(GetLocalState()->GetBoolean(
                prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2),
            true);
}

class ReportControllerDeviceRecoveryTest : public ReportControllerTestBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
  };

  void SetUp() override {
    ReportControllerTestBase::SetUp();

    // Default network to being synchronized and available.
    GetSystemClockTestInterface()->SetServiceIsAvailable(true);
    GetSystemClockTestInterface()->SetNetworkSynchronized(true);

    // Default preserved file DBus operations to retrieve successfully.
    pc_preserved_file_test::TestCase test = utils::GetPreservedFileTestCase(
        GetPreservedFileTestData(),
        pc_preserved_file_test::TestName::
            PrivateComputingClientRegressionTestData_TestName_GET_SUCCESS_UNIX_EPOCH_PING_DATE_SAVE_SUCCESS);
    GetPrivateComputingTestInterface()->SetGetLastPingDatesStatusResponse(
        test.get_response());
    GetPrivateComputingTestInterface()->SetSaveLastPingDatesStatusResponse(
        test.save_response());

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

    report_controller_ = std::make_unique<ReportController>(
        kFakeChromeParameters, GetLocalState(), GetUrlLoaderFactory(),
        base::Time(), base::BindRepeating([]() { return base::Minutes(1); }),
        base::BindRepeating(
            []() { return policy::DeviceMode::DEVICE_MODE_NOT_SET; }),
        base::BindRepeating([]() { return policy::MarketSegment::UNKNOWN; }),
        std::make_unique<PsmClientManager>(std::move(psm_client_delegate)));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    report_controller_.reset();

    // Shutdown dependency clients after |report_controller_| is destroyed.
    ReportControllerTestBase::TearDown();
  }

 protected:
  ReportController* GetReportController() { return report_controller_.get(); }

  // Returns a single negative membership response.
  psm_rlwe::RlweMembershipResponses GetMembershipResponses() {
    psm_rlwe::RlweMembershipResponses membership_responses;

    psm_rlwe::RlweMembershipResponses::MembershipResponseEntry* entry =
        membership_responses.add_membership_responses();
    private_membership::MembershipResponse* membership_response =
        entry->mutable_membership_response();
    membership_response->set_is_member(false);

    return membership_responses;
  }

 private:
  std::unique_ptr<ReportController> report_controller_;
};

TEST_F(ReportControllerDeviceRecoveryTest,
       ValidateCheckMembershipFlowOnRecovery) {
  // Start reporting sequence.
  SetWifiNetworkState(shill::kStateOnline);

  EXPECT_TRUE(GetReportController()->IsDeviceReportingForTesting());

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for 28DA use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Cohort use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Observation use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());

  // Reset local state so that when reporting flow begins again, the device will
  // attempt check membership.
  ResetLocalStateForTesting();

  // Updating time 1 hour ahead will trigger timer to execute reporting flow.
  ForwardClock(base::Minutes(60));

  EXPECT_TRUE(GetReportController()->IsDeviceReportingForTesting());

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(), net::HTTP_OK);
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for 28DA use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Cohort use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  // Next mock network requests for Observation use case.
  SimulateImportResponse(std::string(), net::HTTP_OK);

  EXPECT_FALSE(GetReportController()->IsDeviceReportingForTesting());
}

}  // namespace ash::report::device_metrics
