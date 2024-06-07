// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_policy_handler.h"

#include <memory>
#include <optional>
#include <queue>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_test_helper.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

using InhibitReason = CellularInhibitor::InhibitReason;

namespace {

// EUICC constants
const char kTestEuiccPath0[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEuiccPath1[] = "/org/chromium/Hermes/Euicc/1";
const char kTestEid0[] = "00000000000000000000000000000000";
const char kTestEid1[] = "11111111111111111111111111111111";

// Profile constants
const char kTestProfilePath0[] =
    "/org/chromium/Hermes/Euicc/0/Profile/0000000000000000000";
const char kTestProfileIccid0[] = "0000000000000000000";
const char kTestProfileName0[] = "Name";
const char kTestProfileNickname0[] = "Nickname";
const char kTestProfileServiceProvider0[] = "Service Provider";

// Shill constants
const char kTestCellularDevicePath[] = "/device/cellular";
const char kTestCellularDevicePathName[] = "stub_cellular_device";
const char kTestProfileServicePath0[] = "/service/profile0";

const char kCellularPolicyPattern[] =
    R"({
      "GUID": "Cellular-%lu",
      "Type": "Cellular",
      "Name": "Cellular",
      "Cellular": %s
    })";
const char kCellularPolicyCellularTypePattern[] =
    R"({
      "%s": "%s"
    })";
const char kCellularPolicyCellularTypeWithIccidPattern[] =
    R"({
      "%s": "%s",
      "ICCID": "%s"
    })";


std::string GenerateCellularPolicy(
    const policy_util::SmdxActivationCode& activation_code,
    std::optional<std::string> iccid = std::nullopt) {
  const char* const activation_code_type =
      activation_code.type() == policy_util::SmdxActivationCode::Type::SMDP
          ? onc::cellular::kSMDPAddress
          : onc::cellular::kSMDSAddress;
  const std::string cellular_type =
      iccid.has_value()
          ? base::StringPrintf(kCellularPolicyCellularTypeWithIccidPattern,
                               activation_code_type,
                               activation_code.value().c_str(), iccid->c_str())
          : base::StringPrintf(kCellularPolicyCellularTypePattern,
                               activation_code_type,
                               activation_code.value().c_str());
  return base::StringPrintf(kCellularPolicyPattern, base::RandUint64(),
                            cellular_type.c_str());
}

}  // namespace

class CellularInhibitorObserver : public CellularInhibitor::Observer {
 public:
  CellularInhibitorObserver() {
    session_observation_.Observe(NetworkHandler::Get()->cellular_inhibitor());
  }

  void OnInhibitStateChanged() override {
    std::optional<InhibitReason> inhibit_reason =
        NetworkHandler::Get()->cellular_inhibitor()->GetInhibitReason();
    if (inhibit_reason.has_value()) {
      inhibit_reasons_.push(*inhibit_reason);
    }
  }

  std::optional<InhibitReason> PopInhibitReason() {
    std::optional<InhibitReason> inhibit_reason;
    if (!inhibit_reasons_.empty()) {
      inhibit_reason = inhibit_reasons_.front();
      inhibit_reasons_.pop();
    }
    return inhibit_reason;
  }

 private:
  std::queue<InhibitReason> inhibit_reasons_;
  base::ScopedObservation<CellularInhibitor, CellularInhibitor::Observer>
      session_observation_{this};
};

class CellularPolicyHandlerTest : public testing::Test {
 protected:
  // This struct is used to simplify checking the metrics associated with the
  // tests below.
  struct ExpectedHistogramState {
    size_t success_initial_count = 0u;
    size_t success_retry_count = 0u;
    size_t inhibit_failed_initial_count = 0u;
    size_t inhibit_failed_retry_count = 0u;
    size_t hermes_install_failed_initial_count = 0u;
    size_t hermes_install_failed_retry_count = 0u;
    size_t smds_scan_profile_total_count = 0u;
    size_t smds_scan_profile_sum = 0u;
    size_t no_available_profiles_via_smdp_count = 0u;
    size_t no_available_profiles_via_smds_count = 0u;
    size_t install_method_via_smdp_count = 0u;
    size_t install_method_via_smds_count = 0u;
    size_t scan_duration_other_success_count = 0u;
    size_t scan_duration_other_failure_count = 0u;
    size_t scan_duration_android_success_count = 0u;
    size_t scan_duration_android_failure_count = 0u;
    size_t scan_duration_gsma_success_count = 0u;
    size_t scan_duration_gsma_failure_count = 0u;
    cellular_metrics::ESimSmdsScanHistogramState smds_scan_state;
  };

  CellularPolicyHandlerTest() {}
  ~CellularPolicyHandlerTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->ResetDevicesAndServices();
    network_handler_test_helper_->RegisterPrefs(profile_prefs_.registry(),
                                                device_prefs_.registry());
    network_handler_test_helper_->InitializePrefs(&profile_prefs_,
                                                  &device_prefs_);
    cellular_policy_handler_ = NetworkHandler::Get()->cellular_policy_handler();
    base::RunLoop().RunUntilIdle();

    cellular_policy_handler_ = NetworkHandler::Get()->cellular_policy_handler();
  }

  void TearDown() override { network_handler_test_helper_.reset(); }

  // There are multiple dependencies/requirements for installing an eSIM
  // profile. This function wraps all of the setup into a single method for
  // convenience.
  void SetupGolden() {
    AddCellularDevice();
    AddEuiccs();
    AddWiFi();
  }

  void AddCellularDevice() {
    network_handler_test_helper_->device_test()->AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular,
        kTestCellularDevicePathName);
    base::RunLoop().RunUntilIdle();
  }

  void AddEuiccs() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();

    // We call FastForwardRefreshDelay() after each time we add an EUICC since
    // adding an EUICC will trigger an attempt to refresh/request the list of
    // installed profiles.
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath0), kTestEid0, /*is_active=*/true,
        /*physical_slot=*/0);
    FastForwardRefreshDelay();
    base::RunLoop().RunUntilIdle();

    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath1), kTestEid1, /*is_active=*/false,
        /*physical_slot=*/1);
    FastForwardRefreshDelay();
    base::RunLoop().RunUntilIdle();
  }

  void AddWiFi() {
    network_handler_test_helper_->ConfigureWiFi(shill::kStateOnline);
    base::RunLoop().RunUntilIdle();
  }

  void InstallProfile(const base::Value::Dict& onc_config) {
    cellular_policy_handler()->InstallESim(onc_config);
    base::RunLoop().RunUntilIdle();

    FastForwardRefreshDelay();
  }

  HermesProfileClient::Properties* FindProfileProperties(
      const std::string& activation_code_value) {
    std::optional<dbus::ObjectPath> euicc_path =
        cellular_utils::GetCurrentEuiccPath();
    if (!euicc_path.has_value()) {
      return nullptr;
    }

    HermesEuiccClient::Properties* euicc_properties =
        HermesEuiccClient::Get()->GetProperties(*euicc_path);
    if (!euicc_properties) {
      return nullptr;
    }

    for (auto profile : euicc_properties->profiles().value()) {
      HermesProfileClient::Properties* profile_properties =
          HermesProfileClient::Get()->GetProperties(profile);
      if (profile_properties && profile_properties->activation_code().value() ==
                                    activation_code_value) {
        return profile_properties;
      }
    }
    return nullptr;
  }

  bool IsProfileInstalled(const base::Value::Dict& onc_config,
                          const std::string& activation_code_value,
                          bool check_for_service) {
    HermesProfileClient::Properties* profile_properties =
        FindProfileProperties(activation_code_value);
    if (!profile_properties) {
      LOG(INFO) << "Failed to find Hermes profile properties";
      return false;
    }

    if (profile_properties->state().value() ==
        hermes::profile::State::kPending) {
      LOG(INFO) << "Hermes profile is in the pending state";
      return false;
    }

    const std::string* guid =
        onc_config.FindString(::onc::network_config::kGUID);
    const std::string& iccid = profile_properties->iccid().value();
    if (iccid.empty() || !guid || guid->empty()) {
      LOG(INFO) << "Missing ICCID or GUID";
      return false;
    }

    return !check_for_service || HasShillConfiguration(*guid, iccid);
  }

  bool HasShillConfiguration(const std::string& guid,
                             const std::string& iccid) {
    const std::string shill_service_path =
        ShillServiceClient::Get()->GetTestInterface()->FindServiceMatchingGUID(
            guid);
    const base::Value::Dict* properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            shill_service_path);

    if (!properties) {
      LOG(INFO) << "Failed to find Shill service properties";
      return false;
    }

    const std::string* shill_guid =
        properties->FindString(shill::kGuidProperty);
    if (!shill_guid || guid != *shill_guid) {
      LOG(INFO) << "Missing or mismatched GUID";
      return false;
    }

    const std::string* shill_iccid =
        properties->FindString(shill::kIccidProperty);
    if (!shill_iccid || iccid != *shill_iccid) {
      LOG(INFO) << "Missing or mismatched ICCID";
      return false;
    }

    // UI data should not be empty for configured cellular services.
    return properties->FindString(shill::kUIDataProperty);
  }

  bool HasESimMetadata(const std::string& activation_code_value) {
    HermesProfileClient::Properties* profile_properties =
        FindProfileProperties(activation_code_value);
    if (!profile_properties) {
      LOG(INFO) << "Failed to find Hermes profile properties";
      return false;
    }
    return NetworkHandler::Get()
               ->managed_cellular_pref_handler()
               ->GetESimMetadata(profile_properties->iccid().value()) !=
           nullptr;
  }

  void CheckCurrentEuiccSlot(int32_t physical_slot) {
    std::optional<dbus::ObjectPath> euicc_path =
        cellular_utils::GetCurrentEuiccPath();
    ASSERT_TRUE(euicc_path.has_value());

    HermesEuiccClient::Properties* euicc_properties =
        HermesEuiccClient::Get()->GetProperties(*euicc_path);
    ASSERT_TRUE(euicc_properties);
    EXPECT_EQ(physical_slot, euicc_properties->physical_slot().value());
  }

  void CheckHistogramState(const ExpectedHistogramState& state) {
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanViaPolicyProfileCount,
        /*expected_count=*/state.smds_scan_profile_total_count);
    EXPECT_EQ(
        static_cast<int64_t>(state.smds_scan_profile_sum),
        histogram_tester_.GetTotalSum(
            CellularNetworkMetricsLogger::kSmdsScanViaPolicyProfileCount));
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kESimPolicyInstallNoAvailableProfiles,
        CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmdp,
        /*expected_count=*/state.no_available_profiles_via_smdp_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kESimPolicyInstallNoAvailableProfiles,
        CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmds,
        /*expected_count=*/state.no_available_profiles_via_smds_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kESimPolicyInstallMethod,
        CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmdp,
        /*expected_count=*/state.install_method_via_smdp_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kESimPolicyInstallMethod,
        CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmds,
        /*expected_count=*/state.install_method_via_smds_count);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanAndroidDurationSuccess,
        /*expected_count=*/state.scan_duration_android_success_count);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanAndroidDurationFailure,
        /*expected_count=*/state.scan_duration_android_failure_count);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanOtherDurationSuccess,
        /*expected_count=*/state.scan_duration_other_success_count);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanOtherDurationFailure,
        /*expected_count=*/state.scan_duration_other_failure_count);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanGsmaDurationSuccess,
        /*expected_count=*/state.scan_duration_gsma_success_count);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanGsmaDurationFailure,
        /*expected_count=*/state.scan_duration_gsma_failure_count);
    state.smds_scan_state.Check(&histogram_tester_);
  }

  // This functionality was explicitly separated from InstallProfile() since
  // multiple tests involve attempting, and failing, to install an eSIM profile.
  // By separating the installation and auto-connect logic we can simply call
  // this method specifically when we expect the installation to succeed.
  void CompleteShillServiceAutoConnect(const base::Value::Dict& onc_config) {
    const std::string* shill_guid = onc_config.FindString(shill::kGuidProperty);
    ASSERT_TRUE(shill_guid);

    const std::string shill_service_path =
        ShillServiceClient::Get()->GetTestInterface()->FindServiceMatchingGUID(
            *shill_guid);
    EXPECT_FALSE(shill_service_path.empty());

    ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
        shill_service_path, shill::kStateProperty,
        base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  void FastForwardBy(base::TimeDelta delay) {
    task_environment_.FastForwardBy(delay);
  }

  void FastForwardRefreshDelay() {
    // TODO(crbug.com/1216693): Update when a more robust way of waiting for
    // eSIM profile objects to be loaded is available.
    FastForwardBy(base::Seconds(1));
  }

  // Between the completion of an SM-DS scan and the discovered profile paths
  // actually being provided to CellularPolicyHandler there can be a delay. This
  // delay allows Hermes to update the properties of the discovered profiles
  // before we attempt to read them, e.g. the activation code.
  void FastForwardProfileWaiterDelay() { FastForwardBy(base::Seconds(30)); }

  CellularPolicyHandler* cellular_policy_handler() {
    return cellular_policy_handler_;
  }

  NetworkHandlerTestHelper* network_handler_test_helper() {
    return network_handler_test_helper_.get();
  }

 private:
  void CheckHistogram(const char* histogram,
                      size_t success_count,
                      size_t inhibit_failed_count,
                      size_t hermes_install_failed_count) {
    using InstallESimProfileResult =
        CellularESimInstaller::InstallESimProfileResult;
    histogram_tester_.ExpectBucketCount(histogram,
                                        InstallESimProfileResult::kSuccess,
                                        /*expected_count=*/success_count);
    histogram_tester_.ExpectBucketCount(
        histogram, InstallESimProfileResult::kInhibitFailed,
        /*expected_count=*/inhibit_failed_count);
    histogram_tester_.ExpectBucketCount(
        histogram, InstallESimProfileResult::kHermesInstallFailed,
        /*expected_count=*/hermes_install_failed_count);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;
  raw_ptr<CellularPolicyHandler, DanglingUntriaged> cellular_policy_handler_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple device_prefs_;
};

TEST_F(CellularPolicyHandlerTest, InstallSuccess_SMDP) {
  SetupGolden();

  // We sanity check that the current EUICC has the expected slot in these core
  // installation success tests.
  CheckCurrentEuiccSlot(0);

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());
  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  CellularInhibitorObserver cellular_inhibitor_observer;
  InstallProfile(*onc_config);

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_EQ(InhibitReason::kRefreshingProfileList,
            cellular_inhibitor_observer.PopInhibitReason());
  EXPECT_EQ(InhibitReason::kRequestingAvailableProfiles,
            cellular_inhibitor_observer.PopInhibitReason());
  EXPECT_EQ(InhibitReason::kInstallingProfile,
            cellular_inhibitor_observer.PopInhibitReason());

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_SMDS) {
  SetupGolden();

  // We sanity check that the current EUICC has the expected slot in these core
  // installation success tests.
  CheckCurrentEuiccSlot(0);

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  // A different value is used to more accurately simulate what will happen when
  // installing a profile via SM-DS; the activation code used for the SM-DS scan
  // are different than the activation codes of the available profiles.
  const std::string different_activation_code_value =
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode();

  HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
      dbus::ObjectPath(kTestProfilePath0), dbus::ObjectPath(kTestEuiccPath0),
      kTestProfileIccid0, kTestProfileName0, kTestProfileNickname0,
      kTestProfileServiceProvider0, different_activation_code_value,
      kTestProfileServicePath0, hermes::profile::State::kPending,
      hermes::profile::ProfileClass::kOperational,
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextRefreshSmdxProfilesResult({dbus::ObjectPath(kTestProfilePath0)});

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDS,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  CellularInhibitorObserver cellular_inhibitor_observer;
  InstallProfile(*onc_config);

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_EQ(InhibitReason::kRefreshingProfileList,
            cellular_inhibitor_observer.PopInhibitReason());
  EXPECT_EQ(InhibitReason::kRequestingAvailableProfiles,
            cellular_inhibitor_observer.PopInhibitReason());
  EXPECT_EQ(InhibitReason::kInstallingProfile,
            cellular_inhibitor_observer.PopInhibitReason());

  EXPECT_TRUE(IsProfileInstalled(*onc_config, different_activation_code_value,
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(different_activation_code_value));
  expected_state.success_initial_count++;
  expected_state.smds_scan_profile_total_count++;
  expected_state.smds_scan_profile_sum++;
  expected_state.install_method_via_smds_count++;
  expected_state.scan_duration_other_success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_filtered
      .success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_included
      .success_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_DespiteHermesErrors) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDS,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  // Queue a success result for the call to refresh the profile list.
  HermesEuiccClient::Get()->GetTestInterface()->QueueHermesErrorStatus(
      HermesResponseStatus::kSuccess);

  // Queue a failure result for the SM-DS scan itself.
  HermesEuiccClient::Get()->GetTestInterface()->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorUnknown);
  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.no_available_profiles_via_smds_count++;
  expected_state.smds_scan_profile_total_count++;
  expected_state.scan_duration_other_failure_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_filtered
      .hermes_failed_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_included
      .hermes_failed_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstalledButFailedToEnable) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  // Set the result of the next attempt to enable a carrier profile to match
  // what would be returned when a profile was successfully installed, but
  // failed to become enabled.
  HermesProfileClient::Get()
      ->GetTestInterface()
      ->SetNextEnableCarrierProfileResult(
          HermesResponseStatus::kErrorWrongState);

  CellularInhibitorObserver cellular_inhibitor_observer;
  InstallProfile(*onc_config);

  EXPECT_EQ(InhibitReason::kRefreshingProfileList,
            cellular_inhibitor_observer.PopInhibitReason());
  EXPECT_EQ(InhibitReason::kRequestingAvailableProfiles,
            cellular_inhibitor_observer.PopInhibitReason());
  EXPECT_EQ(InhibitReason::kInstallingProfile,
            cellular_inhibitor_observer.PopInhibitReason());

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_SMDSMultipleProfiles) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  // A different value is used to more accurately simulate what will happen when
  // installing a profile via SM-DS; the activation code used for the SM-DS scan
  // are different than the activation codes of the available profiles.
  const std::string different_activation_code_value =
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode();

  // Add a profile that is already installed and active.
  const dbus::ObjectPath profile_path0 =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath0), hermes::profile::State::kActive,
          HermesEuiccClient::Get()
              ->GetTestInterface()
              ->GenerateFakeActivationCode(),
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithService);

  // Add a profile that is already installed and inactive.
  const dbus::ObjectPath profile_path1 =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath0), hermes::profile::State::kInactive,
          HermesEuiccClient::Get()
              ->GetTestInterface()
              ->GenerateFakeActivationCode(),
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithService);

  // Add a profile that is pending but not expected to be installed since it
  // does not have an activation code.
  const dbus::ObjectPath profile_path2 =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath0), hermes::profile::State::kActive,
          /*activation_code=*/"",
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithService);

  // Add the pending profile that we expect will be installed. This should be
  // the third profile returned, and should be chosen because it is the first
  // profile that is pending and has a valid activation code.
  HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
      dbus::ObjectPath(kTestProfilePath0), dbus::ObjectPath(kTestEuiccPath0),
      kTestProfileIccid0, kTestProfileName0, kTestProfileNickname0,
      kTestProfileServiceProvider0, different_activation_code_value,
      kTestProfileServicePath0, hermes::profile::State::kPending,
      hermes::profile::ProfileClass::kOperational,
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);

  // Add a profile that is pending but not expected to be installed since only
  // the first profile returned that is pending and has a valid activation code
  // should be installed.
  const dbus::ObjectPath profile_path3 =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath0), hermes::profile::State::kActive,
          HermesEuiccClient::Get()
              ->GetTestInterface()
              ->GenerateFakeActivationCode(),
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextRefreshSmdxProfilesResult({
          profile_path0,
          profile_path1,
          profile_path2,
          dbus::ObjectPath(kTestProfilePath0),
          profile_path3,
      });

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDS,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  FastForwardProfileWaiterDelay();

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, different_activation_code_value,
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(different_activation_code_value));
  expected_state.success_initial_count++;
  expected_state.smds_scan_profile_total_count++;
  expected_state.smds_scan_profile_sum = 5;
  expected_state.install_method_via_smds_count++;
  expected_state.scan_duration_other_success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_filtered
      .success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_included
      .success_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_RequireCellularDevice) {
  AddEuiccs();
  AddWiFi();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  AddCellularDevice();

  FastForwardRefreshDelay();

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_RequireEuicc) {
  AddCellularDevice();
  AddWiFi();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  AddEuiccs();

  FastForwardRefreshDelay();

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_RequireNonCellularConnection) {
  AddCellularDevice();
  AddEuiccs();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  // When there is no non-cellular connectivity the installation attempt is
  // considered failed, and will be retried after a delay.
  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  AddWiFi();

  // The delay for the first failure is 10 minutes. Fast forward to just before
  // the next installation attempt should be.
  FastForwardBy(base::Minutes(9));

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  FastForwardBy(base::Minutes(1));

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_retry_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_ExistingIccid) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());
  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code, kTestProfileIccid0));
  ASSERT_TRUE(onc_config.has_value());

  // Add a profile the same ICCID as |onc_config|.
  HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
      dbus::ObjectPath(kTestProfilePath0), dbus::ObjectPath(kTestEuiccPath0),
      kTestProfileIccid0, kTestProfileName0, kTestProfileNickname0,
      kTestProfileServiceProvider0, activation_code.value(),
      kTestProfileServicePath0, hermes::profile::State::kActive,
      hermes::profile::ProfileClass::kOperational,
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/false));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));

  const base::Value::Dict* properties =
      network_handler_test_helper()->service_test()->GetServiceProperties(
          kTestProfileServicePath0);
  ASSERT_TRUE(properties);

  const std::string* iccid = properties->FindString(shill::kIccidProperty);
  EXPECT_TRUE(iccid && *iccid == kTestProfileIccid0);

  InstallProfile(*onc_config);

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(HasESimMetadata(activation_code.value()));

  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_WaitForProfileProperties) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const dbus::ObjectPath profile_path(kTestProfilePath0);

  // Add a profile that is missing the required properties.
  HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
      profile_path, dbus::ObjectPath(kTestEuiccPath0), kTestProfileIccid0,
      /*name=*/"", kTestProfileNickname0, kTestProfileServiceProvider0,
      /*activation_code=*/"", kTestProfileServicePath0,
      /*state=*/hermes::profile::State::kInactive,
      hermes::profile::ProfileClass::kOperational,
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextRefreshSmdxProfilesResult({profile_path});

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());
  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  ASSERT_TRUE(profile_properties);

  profile_properties->name().ReplaceValue(/*value=*/kTestProfileName0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  profile_properties->activation_code().ReplaceValue(
      /*value=*/activation_code.value());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);

  profile_properties->state().ReplaceValue(
      /*value=*/hermes::profile::State::kPending);
  base::RunLoop().RunUntilIdle();

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallFailure_NoActivationCodeProvided) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          base::StringPrintf(kCellularPolicyPattern, base::RandUint64(), "{}"));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallFailure_ProfileMissingActivationCode) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  // Configure a profile with no activation code.
  HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
      dbus::ObjectPath(kTestProfilePath0), dbus::ObjectPath(kTestEuiccPath0),
      kTestProfileIccid0, kTestProfileName0, kTestProfileNickname0,
      kTestProfileServiceProvider0, /*activation_code=*/"",
      kTestProfileServicePath0, hermes::profile::State::kPending,
      hermes::profile::ProfileClass::kOperational,
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextRefreshSmdxProfilesResult({dbus::ObjectPath(kTestProfilePath0)});

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDS,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.smds_scan_profile_total_count++;
  expected_state.smds_scan_profile_sum++;
  expected_state.scan_duration_other_success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_filtered
      .success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_included
      .success_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallFailure_InternalErrorRetry) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorUnknown);

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.hermes_install_failed_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);

  // Failures due to Hermes are considered transient and the installation will
  // be retried after a delay. Fast forward to just before we expect the retry.
  FastForwardBy(base::Minutes(9));

  CheckHistogramState(expected_state);

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorUnknown);

  FastForwardBy(base::Minutes(1));

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.hermes_install_failed_retry_count++;
  CheckHistogramState(expected_state);

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorUnknown);

  // We don't know how much time has passed since the first retry, so instead of
  // checking before and after when we expect the retry to happen we simply skip
  // forward to when we know the next retry should happen.
  FastForwardBy(base::Minutes(20));

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.hermes_install_failed_retry_count++;
  CheckHistogramState(expected_state);

  // Please see the comment above for more context.
  FastForwardBy(base::Minutes(40));

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_retry_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallFailure_OtherErrorRetry) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorSendHttpsFailure);

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.hermes_install_failed_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);

  // Failures that are not due to Hermes or user behavior are not considered
  // transient and the installation will only be retried after an entire day.
  FastForwardBy(base::Hours(23));

  CheckHistogramState(expected_state);

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorSendHttpsFailure);

  FastForwardBy(base::Hours(1));

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.hermes_install_failed_retry_count++;
  CheckHistogramState(expected_state);

  for (int i = 0; i < 2; ++i) {
    HermesEuiccClient::Get()
        ->GetTestInterface()
        ->SetNextInstallProfileFromActivationCodeResult(
            HermesResponseStatus::kErrorSendHttpsFailure);

    // We don't know how much time has passed since the first retry, so instead
    // of checking before and after when we expect the retry to happen we simply
    // skip forward to when we know the next retry should happen.
    FastForwardBy(base::Days(1));

    EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                    /*check_for_service=*/true));
    EXPECT_FALSE(HasESimMetadata(activation_code.value()));
    expected_state.hermes_install_failed_retry_count++;
    CheckHistogramState(expected_state);
  }

  // Failures that are not due to Hermes or user behavior have limit to how many
  // times we will retry the installation. Fast forward an entire week to ensure
  // that we don't continue attempting to install the profile.
  FastForwardBy(base::Days(7));

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallFailure_UserError) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorInvalidActivationCode);

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  expected_state.hermes_install_failed_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);

  // Failures that are due to user behavior are not considered transient and the
  // installation will not be retried.
  FastForwardBy(base::Days(7));

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  EXPECT_FALSE(HasESimMetadata(activation_code.value()));
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, InstallSuccess_SecondEuicc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kCellularUseSecondEuicc);
  SetupGolden();

  CheckCurrentEuiccSlot(1);

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  InstallProfile(*onc_config);

  CompleteShillServiceAutoConnect(*onc_config);

  EXPECT_TRUE(IsProfileInstalled(*onc_config, activation_code.value(),
                                 /*check_for_service=*/true));
  EXPECT_TRUE(HasESimMetadata(activation_code.value()));
  expected_state.success_initial_count++;
  expected_state.install_method_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, NoAvailableProfiles_SMDP) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());
  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextRefreshSmdxProfilesResult({});

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  expected_state.no_available_profiles_via_smdp_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerTest, NoAvailableProfiles_SMDS) {
  SetupGolden();

  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDS,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());
  std::optional<base::Value::Dict> onc_config =
      chromeos::onc::ReadDictionaryFromJson(
          GenerateCellularPolicy(activation_code));
  ASSERT_TRUE(onc_config.has_value());

  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextRefreshSmdxProfilesResult({});

  InstallProfile(*onc_config);

  EXPECT_FALSE(IsProfileInstalled(*onc_config, activation_code.value(),
                                  /*check_for_service=*/true));
  expected_state.no_available_profiles_via_smds_count++;
  expected_state.scan_duration_other_success_count++;
  expected_state.smds_scan_profile_total_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_filtered
      .success_count++;
  expected_state.smds_scan_state.smds_scan_other_user_errors_included
      .success_count++;
  CheckHistogramState(expected_state);
}

}  // namespace ash
