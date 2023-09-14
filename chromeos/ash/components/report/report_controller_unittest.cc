// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/report_controller.h"

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/private_computing/fake_private_computing_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/report/device_metrics/use_case/fake_psm_delegate.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/utils/network_utils.h"
#include "chromeos/ash/components/report/utils/test_utils.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"

namespace psm_rlwe = private_membership::rlwe;

using psm_rlwe_test =
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase;

using pc_preserved_file_test =
    private_computing::PrivateComputingClientRegressionTestData;
namespace ash::report::device_metrics {

class ReportControllerTestBase : public testing::Test {
 public:
  static psm_rlwe::PrivateMembershipRlweClientRegressionTestData*
  GetPsmTestData() {
    static base::NoDestructor<
        psm_rlwe::PrivateMembershipRlweClientRegressionTestData>
        data;
    return data.get();
  }

  static private_computing::PrivateComputingClientRegressionTestData*
  GetPreservedFileTestData() {
    static base::NoDestructor<
        private_computing::PrivateComputingClientRegressionTestData>
        preserved_file_test_data;
    return preserved_file_test_data.get();
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

  static void CreatePreservedFileTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
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
    // Initialize PSM test data used to fake check membership flow.
    CreatePsmTestData();

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
  NetworkHandlerTestHelper network_handler_test_helper_;
};

class ReportControllerSimpleFlowTest : public ReportControllerTestBase {
 public:
  static constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    ReportControllerTestBase::SetUp();

    // PSM test data at index [5,9] contain negative check membership results.
    psm_test_case_ = utils::GetPsmTestCase(GetPsmTestData(), 5);
    ASSERT_FALSE(psm_test_case_.is_positive_membership_expected());

    // Default network to being synchronized and available.
    GetSystemClockTestInterface()->SetServiceIsAvailable(true);
    GetSystemClockTestInterface()->SetNetworkSynchronized(true);

    // Default preserved file DBus operations to be empty.
    GetPrivateComputingTestInterface()->SetGetLastPingDatesStatusResponse(
        private_computing::GetStatusResponse());
    GetPrivateComputingTestInterface()->SetSaveLastPingDatesStatusResponse(
        private_computing::SaveStatusResponse());

    report_controller_ = std::make_unique<ReportController>(
        kFakeChromeParameters, GetLocalState(), GetUrlLoaderFactory(),
        base::Time(), base::BindRepeating([]() { return base::Minutes(1); }),
        std::make_unique<FakePsmDelegate>(
            psm_test_case_.ec_cipher_key(), psm_test_case_.seed(),
            std::vector{psm_test_case_.plaintext_id()}));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    report_controller_.reset();

    // Shutdown dependency clients after |report_controller_| is destroyed.
    ReportControllerTestBase::TearDown();
  }

 protected:
  ReportController* GetReportController() { return report_controller_.get(); }

  psm_rlwe_test GetPsmTestCase() { return psm_test_case_; }

 private:
  psm_rlwe_test psm_test_case_;
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

  // Return well formed response bodies for the pending network requests.
  psm_rlwe_test psm_test_case = GetPsmTestCase();

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(psm_test_case), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(psm_test_case), net::HTTP_OK);
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

  // Return well formed response bodies for the pending network requests.
  psm_rlwe_test psm_test_case = GetPsmTestCase();

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(psm_test_case), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(psm_test_case), net::HTTP_OK);
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

  // Return well formed response bodies for the pending network requests.
  psm_rlwe_test psm_test_case = GetPsmTestCase();

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(psm_test_case), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(psm_test_case), net::HTTP_OK);
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

  // Return well formed response bodies for the pending network requests.
  psm_rlwe_test psm_test_case = GetPsmTestCase();

  // First mock network requests for 1DA use case.
  SimulateOprfResponse(GetFresnelOprfResponse(psm_test_case), net::HTTP_OK);
  SimulateQueryResponse(GetFresnelQueryResponse(psm_test_case), net::HTTP_OK);
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
      version_info::Channel::STABLE /* chromeos_channel */,
      MarketSegment::MARKET_SEGMENT_CONSUMER /* market_segment */,
  };

  void SetUp() override {
    ReportControllerTestBase::SetUp();

    // PSM test data at index [5,9] contain negative check membership results.
    psm_test_case_ = utils::GetPsmTestCase(GetPsmTestData(), 5);
    ASSERT_FALSE(psm_test_case_.is_positive_membership_expected());

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

    report_controller_ = std::make_unique<ReportController>(
        kFakeChromeParameters, GetLocalState(), GetUrlLoaderFactory(),
        base::Time(), base::BindRepeating([]() { return base::Minutes(1); }),
        std::make_unique<FakePsmDelegate>(
            psm_test_case_.ec_cipher_key(), psm_test_case_.seed(),
            std::vector{psm_test_case_.plaintext_id()}));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    report_controller_.reset();

    // Shutdown dependency clients after |report_controller_| is destroyed.
    ReportControllerTestBase::TearDown();
  }

 protected:
  ReportController* GetReportController() { return report_controller_.get(); }

  psm_rlwe_test GetPsmTestCase() { return psm_test_case_; }

 private:
  psm_rlwe_test psm_test_case_;
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

}  // namespace ash::report::device_metrics
