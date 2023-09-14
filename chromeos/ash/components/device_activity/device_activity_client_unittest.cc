// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/device_activity_client.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/dbus/private_computing/fake_private_computing_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/device_activity/churn_cohort_use_case_impl.h"
#include "chromeos/ash/components/device_activity/churn_observation_use_case_impl.h"
#include "chromeos/ash/components/device_activity/daily_use_case_impl.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fake_psm_delegate.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "chromeos/ash/components/device_activity/twenty_eight_day_active_use_case_impl.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Set the current time to the following string.
// Note that we use midnight PST (UTC-8) for the unit tests.
const char kFakeNowTimeString[] = "2023-01-01 08:00:00 GMT";

// This value represents the UTC based activate date of the device formatted
// YYYY-WW to reduce privacy granularity.
// See
// https://crsrc.org/o/src/third_party/chromiumos-overlay/chromeos-base/chromeos-activate-date/files/activate_date;l=67
const char kFakeFirstActivateDate[] = "2022-50";

// Milliseconds per minute.
constexpr int kMillisecondsPerMinute = 60000;

// URLs for the different network requests being performed.
const char kTestFresnelBaseUrl[] = "https://dummy.googleapis.com";
const char kPsmImportRequestEndpoint[] = "/v1/fresnel/psmRlweImport";
const char kPsmOprfRequestEndpoint[] = "/v1/fresnel/psmRlweOprf";
const char kPsmQueryRequestEndpoint[] = "/v1/fresnel/psmRlweQuery";

// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr char kFakeFresnelApiKey[] = "FAKE_FRESNEL_API_KEY";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

// Number of test cases exist in cros_test_data.binarypb file, which is part of
// private_membership third_party library.
const int kNumberOfPsmTestCases = 10;

// Number of test cases exist in private_computing_service_test_data.binarypb.
const int kNumberOfPrivateComputingServiceTestCases = 9;

// PrivateSetMembership regression tests maximum file size which is 4MB.
const size_t kMaxFileSizeInBytes = 4 * (1 << 20);

std::string GetFresnelTestEndpoint(const std::string& endpoint) {
  return kTestFresnelBaseUrl + endpoint;
}

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto)
    return false;

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }
  return out_proto->ParseFromString(file_content);
}

// Returns the total offset between Pacific Time (PT) and GMT.
// Parameter ts is expected to be GMT/UTC.
// TODO(hirthanan): Create utils library for commonly used methods.
base::Time ConvertToPT(base::Time ts) {
  // America/Los_Angleles is PT.
  std::unique_ptr<icu::TimeZone> time_zone(
      icu::TimeZone::createTimeZone("America/Los_Angeles"));
  if (*time_zone == icu::TimeZone::getUnknown()) {
    LOG(ERROR) << "Failed to get America/Los_Angeles timezone. "
               << "Returning UTC-8 timezone as default.";
    return ts - base::Hours(8);
  }

  // Calculate timedelta between PT and GMT. This method does not take day light
  // savings (DST) into account.
  const base::TimeDelta raw_time_diff =
      base::Minutes(time_zone->getRawOffset() / kMillisecondsPerMinute);

  UErrorCode status = U_ZERO_ERROR;
  auto gregorian_calendar =
      std::make_unique<icu::GregorianCalendar>(*time_zone, status);

  // Calculates the time difference adjust by the possible daylight savings
  // offset. If the status of any step fails, returns the default time
  // difference without considering daylight savings.
  if (!gregorian_calendar) {
    return ts + raw_time_diff;
  }

  // Convert ts object to UDate.
  UDate current_date =
      static_cast<UDate>(ts.ToDoubleT() * base::Time::kMillisecondsPerSecond);
  status = U_ZERO_ERROR;
  gregorian_calendar->setTime(current_date, status);
  if (U_FAILURE(status)) {
    return ts + raw_time_diff;
  }

  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status)) {
    return ts + raw_time_diff;
  }

  // Calculate timedelta between PT and GMT, taking DST into account for an
  // accurate PT.
  int gmt_offset = time_zone->getRawOffset();
  if (day_light) {
    gmt_offset += time_zone->getDSTSavings();
  }

  return ts + base::Minutes(gmt_offset / kMillisecondsPerMinute);
}

base::TimeDelta TimeUntilNextPTMidnight() {
  const auto pt_adjusted_ts = ConvertToPT(base::Time::Now());

  base::Time new_pt_midnight =
      pt_adjusted_ts.UTCMidnight() + base::Hours(base::Time::kHoursPerDay);

  return new_pt_midnight - pt_adjusted_ts;
}

base::TimeDelta TimeUntilNewPTMonth() {
  const auto current_ts = base::Time::Now();

  base::Time::Exploded exploded_current_ts;
  current_ts.UTCExplode(&exploded_current_ts);

  // Exploded structure uses 1-based month (values 1 = January, etc.)
  // Increment current ts to be the new month/year.
  // "+ 11) % 12) + 1" wraps the month around if it goes outside 1..12.
  exploded_current_ts.month = (((exploded_current_ts.month + 1) + 11) % 12) + 1;
  exploded_current_ts.year += (exploded_current_ts.month == 1);

  // New timestamp should reflect first day of new month.
  exploded_current_ts.day_of_month = 1;

  base::Time new_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded_current_ts, &new_ts));

  return new_ts - current_ts;
}

// Holds data used to create deterministic PSM network request/response protos.
struct PsmTestData {
  // Holds the response bodies used to test the case where the plaintext id is
  // a member of the PSM dataset.
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      member_test_case;

  // Holds the response bodies used to test the case where the plaintext id is
  // not a member of the PSM dataset.
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      nonmember_test_case;
};

std::vector<psm_rlwe::RlwePlaintextId> GetPlaintextIds(
    const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
        test_case) {
  // Return well formed plaintext ids used in faking PSM network requests.
  return {test_case.plaintext_id()};
}

// Holds data to test various PrivateComputingClient responses from DBus.
struct PrivateComputingClientTestData {
  std::vector<
      private_computing::PrivateComputingClientRegressionTestData::TestCase>
      test_cases;
};

class DailyUseCaseImplUnderTest : public DailyUseCaseImpl {
 public:
  DailyUseCaseImplUnderTest(
      PrefService* local_state,
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case)
      : DailyUseCaseImpl(
            kFakePsmDeviceActiveSecret,
            kFakeChromeParameters,
            local_state,
            std::make_unique<FakePsmDelegate>(test_case.ec_cipher_key(),
                                              test_case.seed(),
                                              GetPlaintextIds(test_case))) {}
  DailyUseCaseImplUnderTest(const DailyUseCaseImplUnderTest&) = delete;
  DailyUseCaseImplUnderTest& operator=(const DailyUseCaseImplUnderTest&) =
      delete;
  ~DailyUseCaseImplUnderTest() override = default;
};

class TwentyEightDayActiveUseCaseImplUnderTest
    : public TwentyEightDayActiveUseCaseImpl {
 public:
  TwentyEightDayActiveUseCaseImplUnderTest(
      PrefService* local_state,
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case)
      : TwentyEightDayActiveUseCaseImpl(
            kFakePsmDeviceActiveSecret,
            kFakeChromeParameters,
            local_state,
            std::make_unique<FakePsmDelegate>(test_case.ec_cipher_key(),
                                              test_case.seed(),
                                              GetPlaintextIds(test_case))) {}
  TwentyEightDayActiveUseCaseImplUnderTest(
      const TwentyEightDayActiveUseCaseImplUnderTest&) = delete;
  TwentyEightDayActiveUseCaseImplUnderTest& operator=(
      const TwentyEightDayActiveUseCaseImplUnderTest&) = delete;
  ~TwentyEightDayActiveUseCaseImplUnderTest() override = default;
};

class ChurnCohortUseCaseImplUnderTest : public ChurnCohortUseCaseImpl {
 public:
  ChurnCohortUseCaseImplUnderTest(
      ChurnActiveStatus* churn_active_status_ptr,
      PrefService* local_state,
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case)
      : ChurnCohortUseCaseImpl(
            churn_active_status_ptr,
            kFakePsmDeviceActiveSecret,
            kFakeChromeParameters,
            local_state,
            std::make_unique<FakePsmDelegate>(test_case.ec_cipher_key(),
                                              test_case.seed(),
                                              GetPlaintextIds(test_case))) {}
  ChurnCohortUseCaseImplUnderTest(const ChurnCohortUseCaseImplUnderTest&) =
      delete;
  ChurnCohortUseCaseImplUnderTest& operator=(
      const ChurnCohortUseCaseImplUnderTest&) = delete;
  ~ChurnCohortUseCaseImplUnderTest() override = default;
};

class ChurnObservationUseCaseImplUnderTest
    : public ChurnObservationUseCaseImpl {
 public:
  ChurnObservationUseCaseImplUnderTest(
      ChurnActiveStatus* churn_active_status_ptr,
      PrefService* local_state,
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case)
      : ChurnObservationUseCaseImpl(
            churn_active_status_ptr,
            kFakePsmDeviceActiveSecret,
            kFakeChromeParameters,
            local_state,
            std::make_unique<FakePsmDelegate>(test_case.ec_cipher_key(),
                                              test_case.seed(),
                                              GetPlaintextIds(test_case))) {}
  ChurnObservationUseCaseImplUnderTest(
      const ChurnObservationUseCaseImplUnderTest&) = delete;
  ChurnObservationUseCaseImplUnderTest& operator=(
      const ChurnObservationUseCaseImplUnderTest&) = delete;
  ~ChurnObservationUseCaseImplUnderTest() override = default;
};

}  // namespace

// TODO(crbug/1317652): Refactor checking if current use case local pref is
// unset. We may also want to abstract the psm network responses for the unit
// tests.
class DeviceActivityClientTest : public testing::Test {
 public:
  DeviceActivityClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DeviceActivityClientTest(const DeviceActivityClientTest&) = delete;
  DeviceActivityClientTest& operator=(const DeviceActivityClientTest&) = delete;
  ~DeviceActivityClientTest() override = default;

  static PsmTestData* GetPsmTestData() {
    static base::NoDestructor<PsmTestData> data;
    return data.get();
  }

  static void CreatePsmTestCase() {
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
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData test_data;
    ASSERT_TRUE(ParseProtoFromFile(kPsmTestDataPath, &test_data));

    // Note that the test cases can change since it's read from the binarypb.
    // This can cause unexpected failures for the unit tests below.
    // As a safety precaution, check whether the number of tests change.
    ASSERT_EQ(test_data.test_cases_size(), kNumberOfPsmTestCases);

    // Sets |psm_test_case_| to have one of the fake PSM request/response
    // protos.
    //
    // Test case 0 contains a response where check membership returns true.
    // Test case 5 contains a response where check membership returns false.
    GetPsmTestData()->member_test_case = test_data.test_cases(0);
    GetPsmTestData()->nonmember_test_case = test_data.test_cases(5);
  }

  static const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::
      TestCase&
      GetPsmNonMemberTestCase() {
    return GetPsmTestData()->nonmember_test_case;
  }

  static PrivateComputingClientTestData* GetPrivateComputingClientTestData() {
    static base::NoDestructor<PrivateComputingClientTestData> data;
    return data.get();
  }

  static void CreatePrivateComputingTestData() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
    const base::FilePath kPrivateComputingTestDataPath =
        src_root_dir.AppendASCII("chromeos")
            .AppendASCII("ash")
            .AppendASCII("components")
            .AppendASCII("device_activity")
            .AppendASCII("testing")
            .AppendASCII("private_computing_service_test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPrivateComputingTestDataPath));
    private_computing::PrivateComputingClientRegressionTestData test_data;
    ASSERT_TRUE(ParseProtoFromFile(kPrivateComputingTestDataPath, &test_data));

    // Note that the test cases can change since it's read from the binarypb.
    // This can cause unexpected failures for the unit tests below.
    // As a safety precaution, check whether the number of tests change.
    ASSERT_EQ(test_data.test_cases_size(),
              kNumberOfPrivateComputingServiceTestCases);

    // Assign the test data test cases to static variable containing all test
    // cases from the read binary protobuf.
    GetPrivateComputingClientTestData()->test_cases = std::vector(
        test_data.test_cases().begin(), test_data.test_cases().end());
  }

  static private_computing::PrivateComputingClientRegressionTestData::TestCase
  GetPrivateComputingRegressionTestCase(
      private_computing::PrivateComputingClientRegressionTestData::TestName
          test_name) {
    for (const auto& test : GetPrivateComputingClientTestData()->test_cases) {
      if (test.name() == test_name)
        return test;
    }

    LOG(ERROR) << "Error finding test_name "
               << private_computing::PrivateComputingClientRegressionTestData::
                      TestName_Name(test_name);
    return private_computing::PrivateComputingClientRegressionTestData::
        TestCase();
  }

  static void SetUpTestSuite() {
    // Initialize |psm_test_case_| which is used to generate deterministic psm
    // protos.
    CreatePsmTestCase();

    // Initialize static PrivateComputingClientTestData.
    CreatePrivateComputingTestData();
  }

 protected:
  // Initialize well formed OPRF response body used to deterministically fake
  // PSM network responses.
  const std::string GetFresnelOprfResponse(
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case) {
    FresnelPsmRlweOprfResponse psm_oprf_response;
    *psm_oprf_response.mutable_rlwe_oprf_response() = test_case.oprf_response();
    return psm_oprf_response.SerializeAsString();
  }

  // Initialize well formed Query response body used to deterministically fake
  // PSM network responses.
  const std::string GetFresnelQueryResponse(
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case) {
    FresnelPsmRlweQueryResponse psm_query_response;
    *psm_query_response.mutable_rlwe_query_response() =
        test_case.query_response();
    return psm_query_response.SerializeAsString();
  }

  PrivateComputingClient::TestInterface* client_test_interface() {
    return PrivateComputingClient::Get()->GetTestInterface();
  }

  // testing::Test:
  void SetUp() override {
    // Initialize the fake PrivateComputingClient used to send DBus calls.
    PrivateComputingClient::InitializeFake();

    // Default network to being synchronized and available.
    SystemClockClient::InitializeFake();
    GetSystemClockTestInterface()->SetServiceIsAvailable(true);
    GetSystemClockTestInterface()->SetNetworkSynchronized(true);

    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    CreateWifiNetworkConfig();

    // Initialize |local_state_| prefs used by device_activity_client class.
    DeviceActivityController::RegisterPrefs(local_state_.registry());
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // ActivateDate VPD field is read from machine statistics in downstream
    // dependency.
    statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                             kFakeFirstActivateDate);

    SetUpDeviceActivityClient(
        {
            features::kDeviceActiveClientDailyCheckMembership,
            features::kDeviceActiveClient28DayActiveCheckIn,
            features::kDeviceActiveClient28DayActiveCheckMembership,
            features::kDeviceActiveClientChurnCohortCheckIn,
            features::kDeviceActiveClientChurnCohortCheckMembership,
            features::kDeviceActiveClientChurnObservationCheckIn,
            features::kDeviceActiveClientChurnObservationCheckMembership,
        },
        GetPsmNonMemberTestCase(),
        GetPrivateComputingRegressionTestCase(
            private_computing::PrivateComputingClientRegressionTestData::
                GET_FAIL_SAVE_FAIL));
  }

  void TearDown() override {
    DCHECK(device_activity_client_);
    DCHECK(churn_active_status_);

    device_activity_client_.reset();

    // Initialized in the SetUp method and safely destructed here.
    churn_active_status_.reset();

    // The system clock must be shutdown after the |device_activity_client_| is
    // destroyed.
    SystemClockClient::Shutdown();

    // The private computing client must be shutdown after the
    // |device_activity_client_| is destroyed.
    PrivateComputingClient::Shutdown();
  }

  SystemClockClient::TestInterface* GetSystemClockTestInterface() {
    return SystemClockClient::Get()->GetTestInterface();
  }

  void SetUpDeviceActivityClient(
      std::vector<base::test::FeatureRef> enabled_features,
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          psm_test_case,
      const private_computing::PrivateComputingClientRegressionTestData::
          TestCase& pc_test_case) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/enabled_features,
        /*disabled_features*/ {});

    // Remote env. runs unit tests assuming base::Time::Now() is unix epoch.
    // Forward current time |kFakeNowTimeString|.
    base::Time new_ts;
    EXPECT_TRUE(base::Time::FromUTCString(kFakeNowTimeString, &new_ts));
    task_environment_.FastForwardBy(new_ts - base::Time::Now());
    task_environment_.RunUntilIdle();

    // Use fake private computing daemon responses from test case.
    client_test_interface()->SetGetLastPingDatesStatusResponse(
        pc_test_case.get_response());
    client_test_interface()->SetSaveLastPingDatesStatusResponse(
        pc_test_case.save_response());

    // Initialize the churn active status to a default value of 0.
    churn_active_status_ = std::make_unique<ChurnActiveStatus>(0);

    // Create vector of device active use cases, which device activity client
    // should maintain ownership of.
    std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases;

    // Append use case if any related feature flag is enabled.
    if (base::FeatureList::IsEnabled(
            features::kDeviceActiveClientDailyCheckMembership)) {
      use_cases.push_back(std::make_unique<DailyUseCaseImplUnderTest>(
          &local_state_, psm_test_case));
    }
    if (base::FeatureList::IsEnabled(
            features::kDeviceActiveClient28DayActiveCheckIn) ||
        base::FeatureList::IsEnabled(
            features::kDeviceActiveClient28DayActiveCheckMembership)) {
      use_cases.push_back(
          std::make_unique<TwentyEightDayActiveUseCaseImplUnderTest>(
              &local_state_, psm_test_case));
    }
    if (base::FeatureList::IsEnabled(
            features::kDeviceActiveClientChurnCohortCheckIn) ||
        base::FeatureList::IsEnabled(
            features::kDeviceActiveClientChurnCohortCheckMembership)) {
      use_cases.push_back(std::make_unique<ChurnCohortUseCaseImplUnderTest>(
          churn_active_status_.get(), &local_state_, psm_test_case));
    }
    if (base::FeatureList::IsEnabled(
            features::kDeviceActiveClientChurnObservationCheckIn) ||
        base::FeatureList::IsEnabled(
            features::kDeviceActiveClientChurnObservationCheckMembership)) {
      use_cases.push_back(
          std::make_unique<ChurnObservationUseCaseImplUnderTest>(
              churn_active_status_.get(), &local_state_, psm_test_case));
    }

    device_activity_client_ = std::make_unique<DeviceActivityClient>(
        churn_active_status_.get(), &local_state_,
        network_state_test_helper_->network_state_handler(),
        test_shared_loader_factory_,
        std::make_unique<base::MockRepeatingTimer>(), kTestFresnelBaseUrl,
        kFakeFresnelApiKey, base::Time(), std::move(use_cases));
  }

  PrefService* GetLocalState() { return &local_state_; }

  // Simulate powerwashing device by removing the local state prefs.
  void SimulateLocalStateOnPowerwash() {
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnownDailyPingTimestamp);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnown28DayActivePingTimestamp);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp);
    local_state_.RemoveUserPref(prefs::kDeviceActiveLastKnownChurnActiveStatus);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2);

    // On device powerwash, the churn active status value will get erased.
    churn_active_status_->SetValue(0);
  }

  void SimulateOprfResponse(const std::string& serialized_response_body,
                            net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        serialized_response_body, response_code);
  }

  void SimulateQueryResponse(const std::string& serialized_response_body,
                             net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        serialized_response_body, response_code);
  }

  void SimulateImportResponse(const std::string& serialized_response_body,
                              net::HttpStatusCode response_code) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint),
        serialized_response_body, response_code);
  }

  void CreateWifiNetworkConfig() {
    ASSERT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \""
       << "wifi_guid"
       << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateIdle << "\""
       << "}";

    wifi_network_service_path_ =
        network_state_test_helper_->ConfigureService(ss.str());
  }

  // |network_state| is a shill network state, e.g. "shill::kStateIdle".
  void SetWifiNetworkState(std::string network_state) {
    network_state_test_helper_->SetServiceProperty(wifi_network_service_path_,
                                                   shill::kStateProperty,
                                                   base::Value(network_state));
    task_environment_.RunUntilIdle();
  }

  // Used in tests, after |device_activity_client_| is generated.
  // Triggers the repeating timer in the client code.
  void FireTimer() {
    base::MockRepeatingTimer* mock_timer =
        static_cast<base::MockRepeatingTimer*>(
            device_activity_client_->GetReportTimer());
    if (mock_timer->IsRunning())
      mock_timer->Fire();

    // Ensure all pending tasks after the timer fires are executed
    // synchronously.
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;
  std::unique_ptr<DeviceActivityClient> device_activity_client_;
  std::string wifi_network_service_path_;
  base::HistogramTester histogram_tester_;
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(DeviceActivityClientTest, ValidateActiveUseCases) {
  EXPECT_EQ(static_cast<int>(device_activity_client_->GetUseCases().size()), 4);
}

TEST_F(DeviceActivityClientTest,
       StayIdleIfSystemClockServiceUnavailableOnNetworkConnection) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  GetSystemClockTestInterface()->SetServiceIsAvailable(false);
  GetSystemClockTestInterface()->NotifyObserversSystemClockUpdated();

  // Network has come online.
  SetWifiNetworkState(shill::kStateOnline);

  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnNetworkOnline,
      1);

  // |OnSystemClockSyncResult| is not called because the service for syncing the
  // clock is unavailble.
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSystemClockSyncResult,
      0);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientReportUseCases,
      0);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest,
       StayIdleIfSystemClockIsNotNetworkSynchronized) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  GetSystemClockTestInterface()->SetNetworkSynchronized(false);
  GetSystemClockTestInterface()->NotifyObserversSystemClockUpdated();

  // Network has come online.
  SetWifiNetworkState(shill::kStateOnline);

  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnNetworkOnline,
      1);

  // |OnSystemClockSyncResult| callback is not executed if the network is not
  // synchronized.
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSystemClockSyncResult,
      0);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientReportUseCases,
      0);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest,
       CheckMembershipOnTimerRetryIfSystemClockIsNotInitiallySynced) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  GetSystemClockTestInterface()->SetNetworkSynchronized(false);
  GetSystemClockTestInterface()->NotifyObserversSystemClockUpdated();

  // Network has come online.
  SetWifiNetworkState(shill::kStateOnline);

  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnNetworkOnline,
      1);

  // |OnSystemClockSyncResult| callback is not executed if the network is not
  // synchronized.
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSystemClockSyncResult,
      0);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientReportUseCases,
      0);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  // Timer executes client and blocks to wait for the system clock
  // synchronization result.
  FireTimer();

  // Synchronously complete pending tasks before validating histogram counts
  // below.
  GetSystemClockTestInterface()->SetNetworkSynchronized(true);
  GetSystemClockTestInterface()->NotifyObserversSystemClockUpdated();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSystemClockSyncResult,
      1);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientReportUseCases,
      1);

  // Begins check membership flow.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingMembershipOprf);
}

TEST_F(DeviceActivityClientTest,
       CheckMembershipIfSystemClockServiceAvailableOnNetworkConnection) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  // Network has come online.
  SetWifiNetworkState(shill::kStateOnline);

  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnNetworkOnline,
      1);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnSystemClockSyncResult,
      1);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientReportUseCases,
      1);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingMembershipOprf);
}

TEST_F(DeviceActivityClientTest, DefaultStatesAreInitializedProperly) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_FALSE(use_case->IsLastKnownPingTimestampSet());
  }

  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
}

TEST_F(DeviceActivityClientTest, NetworkRequestsUseFakeApiKey) {
  // When network comes online, the device performs a check membership
  // network request.
  SetWifiNetworkState(shill::kStateOnline);

  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  task_environment_.RunUntilIdle();

  std::string api_key_header_value;
  request->request.headers.GetHeader("X-Goog-Api-Key", &api_key_header_value);

  EXPECT_EQ(api_key_header_value, kFakeFresnelApiKey);
}

// Fire timer to run |TransitionOutOfIdle|. Network is currently disconnected
// so the client is expected to go back to |kIdle| state.
TEST_F(DeviceActivityClientTest,
       FireTimerWithoutNetworkKeepsClientinIdleState) {
  SetWifiNetworkState(shill::kStateIdle);
  FireTimer();

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, NetworkReconnectsAfterSuccessfulCheckIn) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Reconnecting network connection triggers |TransitionOutOfIdle|.
  SetWifiNetworkState(shill::kStateIdle);
  SetWifiNetworkState(shill::kStateOnline);

  // Check that no additional network requests are pending since all use cases
  // have already been imported.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(DeviceActivityClientTest,
       CheckMembershipOnLocalStateUnsetAndPingRequired) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // On first ever ping, we begin the check membership protocol
    // since the local state pref for that use case is by default unix
    // epoch.
    EXPECT_FALSE(use_case->IsLastKnownPingTimestampSet());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInOnLocalStateSetAndPingRequired) {
  // Set the use cases last ping timestamps to the previous date to
  // |kFakeNowTimeString|.
  base::Time expected = base::Time::Now() - base::Days(1);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(expected);
  }

  // Device active reporting starts check in on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    // base::Time::Now() is updated in |DeviceActivityClientTest| constructor.
    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), expected);
  }

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, TransitionClientToIdleOnInvalidOprfResponse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);
  auto use_cases = device_activity_client_->GetUseCases();

  for (auto* use_case : use_cases) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    // Return an invalid Fresnel OPRF response.
    SimulateOprfResponse(/*fresnel_oprf_response*/ std::string(), net::HTTP_OK);

    task_environment_.RunUntilIdle();
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.CheckMembershipCases",
      DeviceActivityClient::CheckMembershipResponseCases::
          kNotHasRlweOprfResponse,
      use_cases.size());
}

TEST_F(DeviceActivityClientTest, TransitionClientToIdleOnInvalidQueryResponse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    // Return a valid OPRF response.
    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);

    // Return an invalid Query response.
    SimulateQueryResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, DailyCheckInFailsButRemainingUseCasesSucceed) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // On first ever ping, we begin the check membership protocol
    // since the local state pref for that use case is by default unix
    // epoch.
    EXPECT_FALSE(use_case->IsLastKnownPingTimestampSet());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY) {
      // Daily use case will terminate while failing to parse
      // this invalid OPRF response.
      SimulateOprfResponse(std::string(), net::HTTP_OK);

      task_environment_.RunUntilIdle();

      // Failed to update the local state since the OPRF response was invalid.
      EXPECT_FALSE(use_case->IsLastKnownPingTimestampSet());
    } else {
      // All remaining use cases will return valid psm network request
      // responses.
      SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                           net::HTTP_OK);
      SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                            net::HTTP_OK);
      SimulateImportResponse(std::string(), net::HTTP_OK);

      task_environment_.RunUntilIdle();

      // Successfully imported and updated the last ping timestamp to the
      // current mocked time for this test.
      EXPECT_EQ(use_case->GetLastKnownPingTimestamp(),
                ConvertToPT(base::Time::Now()));
    }
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, SuccessfulMembershipCheckAndImport) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // On first ever ping, we begin the check membership protocol
    // since the local state pref for that use case is by default unix
    // epoch.
    EXPECT_FALSE(use_case->IsLastKnownPingTimestampSet());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    // Remaining use cases will return valid psm network request responses.
    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);

    task_environment_.RunUntilIdle();

    // Successfully imported and updated the last ping timestamp to the
    // current mocked time for this test.
    EXPECT_EQ(use_case->GetLastKnownPingTimestamp(),
              ConvertToPT(base::Time::Now()));
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CurrentTimeIsBeforeLocalStateTimeStamp) {
  // Update last ping timestamps to a time in the future.
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("2100-01-01 00:00:00", &expected));
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(expected);
  }

  // Device active reporting is triggered by network connection.
  SetWifiNetworkState(shill::kStateOnline);

  // Device pings are not required since the last ping timestamps are in the
  // future. Client will stay in |kIdle| state.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, StayIdleIfTimerFiresWithoutNetworkConnected) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  SetWifiNetworkState(shill::kStateIdle);
  FireTimer();

  // Verify that no network requests were sent.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInIfCheckMembershipReturnsFalse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);
    base::Time prev_time = use_case->GetLastKnownPingTimestamp();

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    // After a PSM identifier is checked in, local state prefs is updated.
    EXPECT_LT(prev_time, use_case->GetLastKnownPingTimestamp());
  }

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, NetworkDisconnectsWhileWaitingForResponse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // We expect the size of the use cases to be greater than 0.
  EXPECT_GT(device_activity_client_->GetUseCases().size(), 0u);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingMembershipOprf);

  // Currently there is at least 1 pending request that has not received it's
  // response.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Disconnect network.
  SetWifiNetworkState(shill::kStateIdle);

  // All pending requests should be cancelled, and our device activity client
  // should get set back to |kIdle|.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest,
       ReportGracefullyAfterNetworkDisconnectsDuringPreviousRun) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  DeviceActiveUseCase* first_use_case =
      device_activity_client_->GetUseCases().front();
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingMembershipOprf);

  EXPECT_NE(first_use_case->GetWindowIdentifier(), absl::nullopt);
  EXPECT_NE(first_use_case->GetPsmIdentifier(), absl::nullopt);
  EXPECT_NE(first_use_case->GetPsmRlweClient(), nullptr);

  // While waiting for OPRF request, simulate network disconnection.
  SetWifiNetworkState(shill::kStateIdle);

  // Network offline should cancel all pending use cases, and clear the saved
  // state of the attempted pings.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Currently the use cases stores window id, psm id, and psm rlwe client
    // pointer in state.
    EXPECT_EQ(use_case->GetWindowIdentifier(), absl::nullopt);
    EXPECT_EQ(use_case->GetPsmIdentifier(), absl::nullopt);
    EXPECT_EQ(use_case->GetPsmRlweClient(), nullptr);
  }

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  // Attempt to report actives gracefully.
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  // Verify that |OnCheckInDone| is called for each use case.
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActivity.MethodCalled",
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityClientOnCheckInDone,
      device_activity_client_->GetUseCases().size());

  // Verify the last known ping timestamp is set for each use case.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());
  }

  // Returned back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, NetworkDisconnectionClearsUseCaseState) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // After the network comes online, the client triggers device active reporting
  // for the front use case first. It will block on waiting for a response from
  // the OPRF network request. At this point the window id, psm id, and psm rlwe
  // client should be set by the client for just the front use case.
  DeviceActiveUseCase* first_use_case =
      device_activity_client_->GetUseCases().front();
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingMembershipOprf);

  EXPECT_NE(first_use_case->GetWindowIdentifier(), absl::nullopt);
  EXPECT_NE(first_use_case->GetPsmIdentifier(), absl::nullopt);
  EXPECT_NE(first_use_case->GetPsmRlweClient(), nullptr);

  // While waiting for OPRF response, simulate network disconnection.
  SetWifiNetworkState(shill::kStateIdle);

  // Network offline should cancel all pending use cases, and clear the saved
  // state of the attempted pings.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Currently the use cases stores window id, psm id, and psm rlwe client
    // pointer in state.
    EXPECT_EQ(use_case->GetWindowIdentifier(), absl::nullopt);
    EXPECT_EQ(use_case->GetPsmIdentifier(), absl::nullopt);
    EXPECT_EQ(use_case->GetPsmRlweClient(), nullptr);
  }

  // Return back to |kIdle| state after the network goes offline.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInAfterNextPTMidnight) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  task_environment_.FastForwardBy(TimeUntilNextPTMidnight());
  task_environment_.RunUntilIdle();

  FireTimer();

  // Check that at least 1 network request is pending since the PSM id
  // has NOT been imported for the new PT period.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Verify state is |kCheckingIn| since local state was updated
  // with the last check in timestamp during the previous day check ins.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);

  // Return well formed Import response body for the daily and nday use cases.
  // The time was forwarded by 1 day, which only require daily and nday use
  // cases to report active again.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    psm_rlwe::RlweUseCase psm_use_case = use_case->GetPsmUseCase();
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(psm_use_case));
    if (psm_use_case == psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY ||
        psm_use_case == psm_rlwe::RlweUseCase::CROS_FRESNEL_28DAY_ACTIVE) {
      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }
  }

  // Return back to |kIdle| state after another successful check-in of
  // the daily and nday use cases.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, DoNotCheckInTwiceBeforeNextPTDay) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after the first successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  base::TimeDelta before_pt_meridian =
      TimeUntilNextPTMidnight() - base::Minutes(1);
  task_environment_.FastForwardBy(before_pt_meridian);
  task_environment_.RunUntilIdle();

  // Trigger attempt to report device active.
  FireTimer();

  // Client should not send network requests since device is still in same
  // PT day.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Remains in |kIdle| state since the device is still in same PT day.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInAfterNextPTMonth) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  task_environment_.FastForwardBy(TimeUntilNewPTMonth());
  task_environment_.RunUntilIdle();

  FireTimer();

  // Check that at least 1 network request is pending since the PSM id
  // has NOT been imported for the new PT period.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Verify state is |kCheckingIn| since local state was updated
  // with the last check in timestamp during the previous day check ins.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);

  // Return well formed Import response body.
  // The time was forwarded to a new month, which means the daily and 28DA
  // use cases will report active again.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    psm_rlwe::RlweUseCase psm_use_case = use_case->GetPsmUseCase();
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(psm_use_case));
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after successful check-in of daily use case.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

// Powerwashing a device resets the local state. This will result in the
// client re-importing a PSM ID, on the same day.
TEST_F(DeviceActivityClientTest, CheckInAgainOnLocalStateReset) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    base::Time prev_time = use_case->GetLastKnownPingTimestamp();

    // Mock Successful |kCheckingMembershipOprf|.
    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);

    // Mock Successful |kCheckingMembershipQuery|.
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);

    // Mock Successful |kCheckingIn|.
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    base::Time new_time = use_case->GetLastKnownPingTimestamp();

    // After a PSM identifier is checked in, local state prefs is updated.
    EXPECT_LT(prev_time, new_time);
  }

  // Simulate powerwashing device by removing related local state prefs.
  SimulateLocalStateOnPowerwash();

  // Retrigger |TransitionOutOfIdle| codepath by either firing timer or
  // reconnecting network.
  FireTimer();

  // Verify each use case performs check in successfully after local state prefs
  // is reset.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Verify that the |kCheckingIn| state is reached.
    // Indicator is used to verify that we are checking in the PSM ID again
    // after powerwash/recovery scenario.
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    base::Time prev_time = use_case->GetLastKnownPingTimestamp();

    // Mock Successful |kCheckingMembershipOprf|.
    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);

    // Mock Successful |kCheckingMembershipQuery|.
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);

    // Mock Successful |kCheckingIn|.
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    base::Time new_time = use_case->GetLastKnownPingTimestamp();

    // After a PSM identifier is checked in, local state prefs is updated.
    EXPECT_LT(prev_time, new_time);
  }

  // Transitions back to |kIdle| state.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, InitialUmaHistogramStateCount) {
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.StateCount",
      DeviceActivityClient::State::kCheckingMembershipOprf, 0);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.StateCount",
      DeviceActivityClient::State::kCheckingMembershipQuery, 0);
  histogram_tester_.ExpectBucketCount("Ash.DeviceActiveClient.StateCount",
                                      DeviceActivityClient::State::kCheckingIn,
                                      0);
}

TEST_F(DeviceActivityClientTest, UmaHistogramStateCountAfterFirstCheckIn) {
  // Device active reporting starts membership check on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  std::vector<DeviceActiveUseCase*> use_cases =
      device_activity_client_->GetUseCases();

  for (auto* use_case : use_cases) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Mock Successful |kCheckingMembershipOprf|.
    SimulateOprfResponse(GetFresnelOprfResponse(GetPsmNonMemberTestCase()),
                         net::HTTP_OK);

    // Mock Successful |kCheckingMembershipQuery|.
    SimulateQueryResponse(GetFresnelQueryResponse(GetPsmNonMemberTestCase()),
                          net::HTTP_OK);

    // Mock Successful |kCheckingIn|.
    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  histogram_tester_.ExpectBucketCount("Ash.DeviceActiveClient.StateCount",
                                      DeviceActivityClient::State::kCheckingIn,
                                      use_cases.size());
}

TEST_F(DeviceActivityClientTest,
       UpdateChurnActiveStatusAfterChurnCohortCheckIn) {
  // The decimal representation of the bit string `100010001000000000000001101`
  // The first 10 bits represent the number of months since 2000 is 273, which
  // represents the 2022-10.
  // The right 18 bits represent the churn cohort active status for past 18
  // months. The right most bit represents the status of previous active mont,
  // in this case, it represent 2022-10. And the second right most bit
  // represents 2022-09, etc.
  int kFakeBeforeChurnActiveStatus = 71565325;
  int kFakeAfterChurnActvieStatus = 72351849;

  // Set the past ping month to 2022-10.
  base::Time new_daily_ts = base::Time::Now() - base::Days(70);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-10, months is 273
  // Current Churn Cohort month is: 2023-01, months is 276
  // 273->276:   0100010001->0100010100
  // 2022-10 active value: 71565325 -> 0100010001 000000000000001101
  // 2023-01 active value: 72351849 -> 0100010100 000000000001101001

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActvieStatus);
}

TEST_F(DeviceActivityClientTest, ChurnActiveStatusTest) {
  int kFakeBeforeChurnActiveStatus = 71565325;
  int kFakeAfterChurnActiveStatus = 72351849;

  // Set the past ping month to 2022-10.
  base::Time new_daily_ts = base::Time::Now() - base::Days(70);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-10, months is 273
  // Current Churn Cohort month is: 2023-01, months is 276
  // 273->276:   0100010001->0100010100
  // 2022-10 active value: 71565325 -> 0100010001 000000000000001101
  // 2023-01 active value: 72351849 -> 0100010100 000000000001101001

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    SimulateImportResponse(std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActiveStatus);

  private_computing::SaveStatusRequest save_request =
      device_activity_client_->GetSaveStatusRequest();
  for (auto status : save_request.active_status()) {
    if (status.use_case() == private_computing::PrivateComputingUseCase::
                                 CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
      EXPECT_EQ(status.churn_active_status(), kFakeAfterChurnActiveStatus);
    }
  }
}

TEST_F(DeviceActivityClientTest, ReportAgainAfterThreeMonths) {
  int kFakeBeforeChurnActiveStatus = 71565325;
  int kFakeAfterChurnActiveStatus = 72351849;

  // Set the past ping month to 2022-10.
  base::Time new_daily_ts = base::Time::Now() - base::Days(70);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Set the relative observation local state booleans to all be true.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-10, months is 273
  // Current Churn Cohort month is: 2023-01, months is 276
  // 273->276:   0100010001->0100010100
  // 2022-10 active value: 71565325 -> 0100010001 000000000000001101
  // 2023-01 active value: 72351849 -> 0100010100 000000000001101001

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
      // Before cohort import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After cohort import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
      // Before observation import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After observation import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else {
      // Successfully import for all remaining use cases.
      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActiveStatus);
}

TEST_F(DeviceActivityClientTest, ReportAgainAfterTwoMonths) {
  int kFakeBeforeChurnActiveStatus = 71827469;
  int kFakeAfterChurnActiveStatus = 72351797;

  // Set the past ping month to 2022-11.
  base::Time new_daily_ts = base::Time::Now() - base::Days(40);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Set the relative observation local state booleans to all be true.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-11, months is 274
  // Current Churn Cohort month is: 2023-01, months is 276
  // 274->276:   0100010010->0100010100
  // 2022-11 active value: 71827469 -> 0100010010 000000000000001101
  // 2023-01 active value: 72351797 -> 0100010100 000000000000110101

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
      // Before cohort import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After cohort import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
      // Before observation import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After observation import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else {
      // Successfully import for all remaining use cases.
      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActiveStatus);
}

TEST_F(DeviceActivityClientTest, ReportAgainAfterOneMonth) {
  int kFakeBeforeChurnActiveStatus = 72089613;
  int kFakeAfterChurnActiveStatus = 72351771;

  // Set the past ping month to 2022-12.
  base::Time new_daily_ts = base::Time::Now() - base::Days(10);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Set the relative observation local state booleans to all be true.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-12, months is 275
  // Current Churn Cohort month is: 2023-01, months is 276
  // 275->276:   0100010011->0100010100
  // 2022-12 active value: 72089613 -> 0100010011 000000000000001101
  // 2023-01 active value: 72351771 -> 0100010100 000000000000011011

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
      // Before cohort import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After cohort import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
      // Before observation import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After observation import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else {
      // Successfully import for all remaining use cases.
      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActiveStatus);
}

TEST_F(DeviceActivityClientTest,
       ObservationPeriodGeneratedAfterNewCohortMonth) {
  int kFakeBeforeChurnActiveStatus = 72089613;
  int kFakeAfterChurnActiveStatus = 72351771;

  // Set the past ping month to 2022-12.
  base::Time new_daily_ts = base::Time::Now() - base::Days(10);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Set the relative observation local state booleans to all be true.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-12, months is 275
  // Current Churn Cohort month is: 2023-01, months is 276
  // 275->276:   0100010011->0100010100
  // 2022-12 active value: 72089613 -> 0100010011 000000000000001101
  // 2023-01 active value: 72351771 -> 0100010100 000000000000011011

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
      // Before cohort import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After cohort import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
      // Before observation import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_EQ(use_case->GetObservationPeriod(0), "202301-202303");
      EXPECT_EQ(use_case->GetObservationPeriod(1), std::string());
      EXPECT_EQ(use_case->GetObservationPeriod(2), std::string());

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After observation import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else {
      // Successfully import for all remaining use cases.
      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActiveStatus);
}

TEST_F(DeviceActivityClientTest, ValidateObservationPeriodForUnsetLocalState) {
  int kFakeBeforeChurnActiveStatus = 72089613;
  int kFakeAfterChurnActiveStatus = 72351771;

  // Set the past ping month to 2022-12.
  base::Time new_daily_ts = base::Time::Now() - base::Days(10);
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(new_daily_ts);
  }

  // Set the relative observation local state booleans.
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, false);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  GetLocalState()->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, false);

  // Initialize the churn_active_value to kFakeBeforeChurnActiveStatus.
  churn_active_status_->SetValue(kFakeBeforeChurnActiveStatus);

  // Last Churn Cohort month is: 2022-12, months is 275
  // Current Churn Cohort month is: 2023-01, months is 276
  // 275->276:   0100010011->0100010100
  // 2022-12 active value: 72089613 -> 0100010011 000000000000001101
  // 2023-01 active value: 72351771 -> 0100010100 000000000000011011

  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT) {
      // Before cohort import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After cohort import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION) {
      // Before observation import request.
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_FALSE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
      EXPECT_EQ(use_case->GetObservationPeriod(0), "202301-202303");
      EXPECT_EQ(use_case->GetObservationPeriod(1), "202212-202302");
      EXPECT_EQ(use_case->GetObservationPeriod(2), std::string());

      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();

      // After observation import request.
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
      EXPECT_TRUE(GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));
    } else {
      // Successfully import for all remaining use cases.
      SimulateImportResponse(std::string(), net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }

    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), new_daily_ts);
  }

  // Check the new churn active status after ping.
  int updated_active_status_value = churn_active_status_->GetValueAsInt();
  EXPECT_EQ(updated_active_status_value, kFakeAfterChurnActiveStatus);
}

}  // namespace ash::device_activity
