// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"

#include <memory>

#include "base/debug/debugging_buildflags.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Note: All histogram names should be listed here.

// LogAllConnectionResult() Cellular histograms.
const char kCellularConnectResultAllHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.All";
const char kCellularESimConnectResultAllHistogram[] =
    "Network.Ash.Cellular.ESim.ConnectionResult.All";
const char kCellularESimConnectResultNonUserInitiatedHistogram[] =
    "Network.Ash.Cellular.ESim.ConnectionResult.NonUserInitiated";
const char kCellularPSimConnectResultAllHistogram[] =
    "Network.Ash.Cellular.PSim.ConnectionResult.All";
const char kCellularPSimConnectResultNonUserInitiatedHistogram[] =
    "Network.Ash.Cellular.PSim.ConnectionResult.NonUserInitiated";
const char kCellularESimPolicyConnectResultAllHistogram[] =
    "Network.Ash.Cellular.ESim.Policy.ConnectionResult.All";

// LogAllConnectionResult() filtered Cellular histograms.
const char kCellularConnectResultFilteredHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.Filtered";
const char kCellularESimConnectResultFilteredHistogram[] =
    "Network.Ash.Cellular.ESim.ConnectionResult.Filtered";
const char kCellularPSimConnectResultFilteredHistogram[] =
    "Network.Ash.Cellular.PSim.ConnectionResult.Filtered";
const char kCellularESimPolicyConnectResultFilteredHistogram[] =
    "Network.Ash.Cellular.ESim.Policy.ConnectionResult.Filtered";

// LogAllConnectionResult() VPN histograms.
const char kVpnConnectResultAllHistogram[] =
    "Network.Ash.VPN.ConnectionResult.All";
const char kVpnBuiltInConnectResultAllHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.ConnectionResult.All";
const char kVpnThirdPartyConnectResultAllHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.ConnectionResult.All";
const char kVpnUnknownConnectResultAllHistogram[] =
    "Network.Ash.VPN.TypeUnknown.ConnectionResult.All";

// LogAllConnectionResult() filtered VPN histograms.
const char kVpnConnectResultFilteredHistogram[] =
    "Network.Ash.VPN.ConnectionResult.Filtered";
const char kVpnBuiltInConnectResultFilteredHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.ConnectionResult.Filtered";
const char kVpnThirdPartyConnectResultFilteredHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.ConnectionResult.Filtered";
const char kVpnUnknownConnectResultFilteredHistogram[] =
    "Network.Ash.VPN.TypeUnknown.ConnectionResult.Filtered";

// LogAllConnectionResult() WiFi histograms.
const char kWifiConnectResultAllHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.All";
const char kWifiConnectResultNonUserInitiatedHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.NonUserInitiated";
const char kWifiOpenConnectResultAllHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.ConnectionResult.All";
const char kWifiPasswordProtectedConnectResultAllHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected.ConnectionResult.All";

// LogAllConnectionResult() filtered WiFi histograms.
const char kWifiConnectResultFilteredHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.Filtered";
const char kWifiOpenConnectResultFilteredHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.ConnectionResult.Filtered";
const char kWifiPasswordProtectedConnectResultFilteredHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected.ConnectionResult.Filtered";

// LogAllConnectionResult() Ethernet histograms.
const char kEthernetConnectResultAllHistogram[] =
    "Network.Ash.Ethernet.ConnectionResult.All";
const char kEthernetEapConnectResultAllHistogram[] =
    "Network.Ash.Ethernet.Eap.ConnectionResult.All";
const char kEthernetNoEapConnectResultAllHistogram[] =
    "Network.Ash.Ethernet.NoEap.ConnectionResult.All";

// LogAllConnectionResult() filtered Ethernet histograms.
const char kEthernetConnectResultFilteredHistogram[] =
    "Network.Ash.Ethernet.ConnectionResult.Filtered";
const char kEthernetEapConnectResultFilteredHistogram[] =
    "Network.Ash.Ethernet.Eap.ConnectionResult.Filtered";
const char kEthernetNoEapConnectResultFilteredHistogram[] =
    "Network.Ash.Ethernet.NoEap.ConnectionResult.Filtered";

// LogUserInitiatedConnectionResult() Cellular histograms.
const char kCellularConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Cellular.ConnectionResult.UserInitiated";
const char kCellularESimConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Cellular.ESim.ConnectionResult.UserInitiated";
const char kCellularPSimConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Cellular.PSim.ConnectionResult.UserInitiated";
const char kCellularESimPolicyConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Cellular.ESim.Policy.ConnectionResult.UserInitiated";

// LogUserInitiatedConnectionResult() VPN histograms.
const char kVpnConnectResultUserInitiatedHistogram[] =
    "Network.Ash.VPN.ConnectionResult.UserInitiated";
const char kVpnBuiltInConnectResultUserInitiatedHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.ConnectionResult.UserInitiated";
const char kVpnThirdPartyConnectResultUserInitiatedHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.ConnectionResult.UserInitiated";
const char kVpnUnknownConnectResultUserInitiatedHistogram[] =
    "Network.Ash.VPN.TypeUnknown.ConnectionResult.UserInitiated";

// LogUserInitiatedConnectionResult() WiFi histograms.
const char kWifiConnectResultUserInitiatedHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.UserInitiated";
const char kWifiOpenConnectResultUserInitiatedHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.ConnectionResult.UserInitiated";
const char kWifiPasswordProtectedConnectResultUserInitiatedHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected.ConnectionResult.UserInitiated";

// LogUserInitiatedConnectionResult() Ethernet histograms.
const char kEthernetConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Ethernet.ConnectionResult.UserInitiated";
const char kEthernetEapConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Ethernet.Eap.ConnectionResult.UserInitiated";
const char kEthernetNoEapConnectResultUserInitiatedHistogram[] =
    "Network.Ash.Ethernet.NoEap.ConnectionResult.UserInitiated";

// LogConnectionStateResult() Cellular histograms.
const char kCellularConnectionStateHistogram[] =
    "Network.Ash.Cellular.DisconnectionsWithoutUserAction";
const char kCellularESimConnectionStateHistogram[] =
    "Network.Ash.Cellular.ESim.DisconnectionsWithoutUserAction";
const char kCellularPSimConnectionStateHistogram[] =
    "Network.Ash.Cellular.PSim.DisconnectionsWithoutUserAction";
const char kCellularESimPolicyConnectionStateHistogram[] =
    "Network.Ash.Cellular.ESim.Policy.DisconnectionsWithoutUserAction";
const char kCellularConnectionStateShillErrorHistogram[] =
    "Network.Ash.Cellular.DisconnectionsWithoutUserAction.ShillError";
const char kCellularESimConnectionStateShillErrorHistogram[] =
    "Network.Ash.Cellular.ESim.DisconnectionsWithoutUserAction.ShillError";
const char kCellularPSimConnectionStateShillErrorHistogram[] =
    "Network.Ash.Cellular.PSim.DisconnectionsWithoutUserAction.ShillError";
const char kCellularESimPolicyConnectionStateShillErrorHistogram[] =
    "Network.Ash.Cellular.ESim.Policy.DisconnectionsWithoutUserAction."
    "ShillError";

// LogConnectionStateResult() VPN histograms.
const char kVpnConnectionStateHistogram[] =
    "Network.Ash.VPN.DisconnectionsWithoutUserAction";
const char kVpnBuiltInConnectionStateHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.DisconnectionsWithoutUserAction";
const char kVpnThirdPartyConnectionStateHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.DisconnectionsWithoutUserAction";
const char kVpnUnknownConnectionStateHistogram[] =
    "Network.Ash.VPN.TypeUnknown.DisconnectionsWithoutUserAction";
const char kVpnConnectionStateShillErrorHistogram[] =
    "Network.Ash.VPN.DisconnectionsWithoutUserAction.ShillError";
const char kVpnBuiltInConnectionStateShillErrorHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.DisconnectionsWithoutUserAction.ShillError";
const char kVpnThirdPartyConnectionStateShillErrorHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.DisconnectionsWithoutUserAction.ShillError";
const char kVpnUnknownConnectionStateShillErrorHistogram[] =
    "Network.Ash.VPN.TypeUnknown.DisconnectionsWithoutUserAction.ShillError";

// LogConnectionStateResult() WiFi histograms.
const char kWifiConnectionStateHistogram[] =
    "Network.Ash.WiFi.DisconnectionsWithoutUserAction";
const char kWifiOpenConnectionStateHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.DisconnectionsWithoutUserAction";
const char kWifiPasswordProtectedConnectionStateHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected."
    "DisconnectionsWithoutUserAction";
const char kWifiConnectionStateShillErrorHistogram[] =
    "Network.Ash.WiFi.DisconnectionsWithoutUserAction.ShillError";
const char kWifiOpenConnectionStateShillErrorHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.DisconnectionsWithoutUserAction.ShillError";
const char kWifiPasswordProtectedConnectionStateShillErrorHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected."
    "DisconnectionsWithoutUserAction.ShillError";

// LogConnectionStateResult() Ethernet histograms.
const char kEthernetConnectionStateHistogram[] =
    "Network.Ash.Ethernet.DisconnectionsWithoutUserAction";
const char kEthernetEapConnectionStateHistogram[] =
    "Network.Ash.Ethernet.Eap.DisconnectionsWithoutUserAction";
const char kEthernetNoEapConnectionStateHistogram[] =
    "Network.Ash.Ethernet.NoEap.DisconnectionsWithoutUserAction";
const char kEthernetConnectionStateShillErrorHistogram[] =
    "Network.Ash.Ethernet.DisconnectionsWithoutUserAction.ShillError";
const char kEthernetEapConnectionStateShillErrorHistogram[] =
    "Network.Ash.Ethernet.Eap.DisconnectionsWithoutUserAction.ShillError";
const char kEthernetNoEapConnectionStateShillErrorHistogram[] =
    "Network.Ash.Ethernet.NoEap.DisconnectionsWithoutUserAction.ShillError";

// LogEnableTechnologyResult() histograms.
const char kEnableWifiResultHistogram[] =
    "Network.Ash.WiFi.EnabledState.Enable.Result";
const char kEnableWifiResultCodeHistogram[] =
    "Network.Ash.WiFi.EnabledState.Enable.ResultCode";
const char kEnableEthernetResultHistogram[] =
    "Network.Ash.Ethernet.EnabledState.Enable.Result";
const char kEnableEthernetResultCodeHistogram[] =
    "Network.Ash.Ethernet.EnabledState.Enable.ResultCode";
const char kEnableCellularResultHistogram[] =
    "Network.Ash.Cellular.EnabledState.Enable.Result";
const char kEnableCellularResultCodeHistogram[] =
    "Network.Ash.Cellular.EnabledState.Enable.ResultCode";
const char kEnableVpnResultHistogram[] =
    "Network.Ash.VPN.EnabledState.Enable.Result";
const char kEnableVpnResultCodeHistogram[] =
    "Network.Ash.VPN.EnabledState.Enable.ResultCode";

// LogDisableTechnologyResult() histograms.
const char kDisableWifiResultHistogram[] =
    "Network.Ash.WiFi.EnabledState.Disable.Result";
const char kDisableWifiResultCodeHistogram[] =
    "Network.Ash.WiFi.EnabledState.Disable.ResultCode";
const char kDisableEthernetResultHistogram[] =
    "Network.Ash.Ethernet.EnabledState.Disable.Result";
const char kDisableEthernetResultCodeHistogram[] =
    "Network.Ash.Ethernet.EnabledState.Disable.ResultCode";
const char kDisableCellularResultHistogram[] =
    "Network.Ash.Cellular.EnabledState.Disable.Result";
const char kDisableCellularResultCodeHistogram[] =
    "Network.Ash.Cellular.EnabledState.Disable.ResultCode";
const char kDisableVpnResultHistogram[] =
    "Network.Ash.VPN.EnabledState.Disable.Result";
const char kDisableVpnResultCodeHistogram[] =
    "Network.Ash.VPN.EnabledState.Disable.ResultCode";

const char kTestGuid[] = "test_guid";
const char kTestServicePath[] = "/service/network";
const char kTestServicePath1[] = "/service/network1";
const char kTestDevicePath[] = "/device/network";
const char kTestName[] = "network_name";
const char kTestVpnHost[] = "test host";
const char kTestUnknownVpn[] = "test_unknown_vpn";

void LogVpnResult(const std::string& provider,
                  base::RepeatingClosure func,
                  bool* failed_to_log_result) {
  ASSERT_NE(failed_to_log_result, nullptr);

// Emitting a metric for an unknown VPN provider will always cause a
// DUMP_WILL_BE_NOTREACHED() to be hit. This is fatal outside official builds,
// so make sure that we die in those configurations.
#if !defined(OFFICIAL_BUILD)
  if (provider == kTestUnknownVpn) {
    ASSERT_DEATH({ func.Run(); }, "");
    *failed_to_log_result = true;
    return;
  }
#endif  // !defined(OFFICIAL_BUILD)
  func.Run();
}

}  // namespace

class NetworkMetricsHelperTest : public testing::Test {
 public:
  NetworkMetricsHelperTest() {}

  NetworkMetricsHelperTest(const NetworkMetricsHelperTest&) = delete;
  NetworkMetricsHelperTest& operator=(const NetworkMetricsHelperTest&) = delete;

  ~NetworkMetricsHelperTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->ClearServices();
    network_handler_test_helper_->RegisterPrefs(profile_prefs_.registry(),
                                                local_state_.registry());
    network_handler_test_helper_->InitializePrefs(&profile_prefs_,
                                                  &local_state_);

    shill_service_client_ = network_handler_test_helper_->service_test();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    shill_service_client_->ClearServices();
    network_handler_test_helper_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  raw_ptr<ShillServiceClient::TestInterface, DanglingUntriaged>
      shill_service_client_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(NetworkMetricsHelperTest, EnableTechnologyWithErrors) {
  NetworkMetricsHelper::LogEnableTechnologyResult(
      shill::kTypeWifi,
      /*success=*/false, "org.chromium.flimflam.Error.AlreadyConnected");
  histogram_tester_->ExpectTotalCount(kEnableWifiResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEnableWifiResultCodeHistogram,
      ShillConnectResult::kErrorResultAlreadyConnected, 1);

  NetworkMetricsHelper::LogEnableTechnologyResult(
      shill::kTypeEthernet,
      /*success=*/false, "org.chromium.flimflam.Error.OperationTimeout");
  histogram_tester_->ExpectTotalCount(kEnableEthernetResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEnableEthernetResultCodeHistogram,
      ShillConnectResult::kErrorResultOperationTimeout, 1);

  NetworkMetricsHelper::LogEnableTechnologyResult(
      shill::kTypeCellular,
      /*success=*/false, "org.chromium.flimflam.Error.NoCarrier");
  histogram_tester_->ExpectTotalCount(kEnableCellularResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEnableCellularResultCodeHistogram,
      ShillConnectResult::kErrorResultNoCarrier, 1);

  NetworkMetricsHelper::LogEnableTechnologyResult(
      shill::kTypeVPN,
      /*success=*/false, "org.chromium.flimflam.Error.WrongState");
  histogram_tester_->ExpectTotalCount(kEnableVpnResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEnableVpnResultCodeHistogram, ShillConnectResult::kErrorResultWrongState,
      1);
}

TEST_F(NetworkMetricsHelperTest, DisableTechnologyWithErrors) {
  NetworkMetricsHelper::LogDisableTechnologyResult(
      shill::kTypeWifi,
      /*success=*/false, "org.chromium.flimflam.Error.AlreadyConnected");
  histogram_tester_->ExpectTotalCount(kDisableWifiResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kDisableWifiResultCodeHistogram,
      ShillConnectResult::kErrorResultAlreadyConnected, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(
      shill::kTypeEthernet,
      /*success=*/false, "org.chromium.flimflam.Error.OperationTimeout");
  histogram_tester_->ExpectTotalCount(kDisableEthernetResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kDisableEthernetResultCodeHistogram,
      ShillConnectResult::kErrorResultOperationTimeout, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(
      shill::kTypeCellular,
      /*success=*/false, "org.chromium.flimflam.Error.NoCarrier");
  histogram_tester_->ExpectTotalCount(kDisableCellularResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kDisableCellularResultCodeHistogram,
      ShillConnectResult::kErrorResultNoCarrier, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(
      shill::kTypeVPN,
      /*success=*/false, "org.chromium.flimflam.Error.WrongState");
  histogram_tester_->ExpectTotalCount(kDisableVpnResultCodeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kDisableVpnResultCodeHistogram,
      ShillConnectResult::kErrorResultWrongState, 1);
}

TEST_F(NetworkMetricsHelperTest, EnableDisableTechnology) {
  NetworkMetricsHelper::LogEnableTechnologyResult(shill::kTypeWifi,
                                                  /*success=*/true);
  histogram_tester_->ExpectTotalCount(kEnableWifiResultHistogram, 1);

  NetworkMetricsHelper::LogEnableTechnologyResult(shill::kTypeEthernet,
                                                  /*success=*/true);
  histogram_tester_->ExpectTotalCount(kEnableEthernetResultHistogram, 1);

  NetworkMetricsHelper::LogEnableTechnologyResult(shill::kTypeCellular,
                                                  /*success=*/true);
  histogram_tester_->ExpectTotalCount(kEnableCellularResultHistogram, 1);

  NetworkMetricsHelper::LogEnableTechnologyResult(shill::kTypeVPN,
                                                  /*success=*/true);
  histogram_tester_->ExpectTotalCount(kEnableVpnResultHistogram, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(shill::kTypeWifi,
                                                   /*success=*/true);
  histogram_tester_->ExpectTotalCount(kDisableWifiResultHistogram, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(shill::kTypeEthernet,
                                                   /*success=*/true);
  histogram_tester_->ExpectTotalCount(kDisableEthernetResultHistogram, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(shill::kTypeCellular,
                                                   /*success=*/true);
  histogram_tester_->ExpectTotalCount(kDisableCellularResultHistogram, 1);

  NetworkMetricsHelper::LogDisableTechnologyResult(shill::kTypeVPN,
                                                   /*success=*/true);
  histogram_tester_->ExpectTotalCount(kDisableVpnResultHistogram, 1);
}

TEST_F(NetworkMetricsHelperTest, CellularESim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kEidProperty, base::Value("eid"));
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kIccidProperty, base::Value("iccid"));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/false,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultNonUserInitiatedHistogram, 0);

  histogram_tester_->ExpectTotalCount(kCellularConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/true, /*is_repeated_error=*/true,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 2);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      2);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultNonUserInitiatedHistogram, 1);

  histogram_tester_->ExpectTotalCount(kCellularConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultFilteredHistogram, 0);

  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kErrorProperty,
      base::Value(shill::kErrorInvalidAPN));
  base::RunLoop().RunUntilIdle();
  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorConnectFailed);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectResultUserInitiatedHistogram,
      UserInitiatedConnectResult::kErrorInvalidAPN, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularESimConnectResultUserInitiatedHistogram,
      UserInitiatedConnectResult::kErrorInvalidAPN, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularESimConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kCellularPSimConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
}

TEST_F(NetworkMetricsHelperTest, CellularESimPolicy) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kEidProperty, base::Value("eid"));
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kIccidProperty, base::Value("iccid"));
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  shill_service_client_->SetServiceProperty(kTestServicePath,
                                            shill::kUIDataProperty,
                                            base::Value(ui_data->GetAsJson()));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/false,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);

  histogram_tester_->ExpectTotalCount(kCellularConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularESimConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kCellularPSimConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
}

TEST_F(NetworkMetricsHelperTest, CellularPSim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kIccidProperty, base::Value("iccid"));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/false,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultNonUserInitiatedHistogram, 0);

  histogram_tester_->ExpectTotalCount(kCellularConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/true, /*is_repeated_error=*/true,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 2);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      2);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultNonUserInitiatedHistogram, 1);

  histogram_tester_->ExpectTotalCount(kCellularConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kCellularESimConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kCellularESimPolicyConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kCellularPSimConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
}

TEST_F(NetworkMetricsHelperTest, VPN) {
  const std::vector<std::string> kProviders{{
      shill::kProviderIKEv2,
      shill::kProviderL2tpIpsec,
      shill::kProviderArcVpn,
      shill::kProviderOpenVpn,
      shill::kProviderThirdPartyVpn,
      shill::kProviderWireGuard,
      kTestUnknownVpn,
  }};

  // This test emits to the "ConnectionState" histograms twice per loop
  // iteration to cover the "good" and the "error" cases. Rather than
  // introducing a count variable for these cases, we simply multiply our
  // expectation by |2|.
  const size_t kConnectionStateCountScale = 2u;

  size_t expected_all_count = 0u;
  size_t expected_user_initiated_count = 0u;
  size_t expected_built_in_count = 0u;
  size_t expected_built_in_count_user_initiated = 0u;
  size_t expected_third_party_count = 0u;
  size_t expected_third_party_count_user_initiated = 0u;
  size_t expected_unknown_count = 0u;
  size_t expected_unknown_count_user_initiated = 0u;

  size_t expected_filtered_count = 0u;
  size_t expected_built_in_fitered_count = 0u;
  size_t expected_third_party_filtered_count = 0u;
  size_t expected_unknown_filtered_count = 0u;

  const size_t kTotalCountIncrement = 3u;
  const size_t kFilteredCountIncrement = 2u;
  const size_t kUserInitiatedCountIncrement = 1u;

  base::RepeatingClosure log_all_connection_result_not_repeated_with_error =
      base::BindRepeating(
          &NetworkMetricsHelper::LogAllConnectionResult, kTestGuid,
          /*is_auto_connect=*/false, /*is_repeated_error=*/false,
          shill::kErrorNotRegistered);
  base::RepeatingClosure log_all_connection_result_repeated_with_error =
      base::BindRepeating(&NetworkMetricsHelper::LogAllConnectionResult,
                          kTestGuid,
                          /*is_auto_connect=*/false, /*is_repeated_error=*/true,
                          shill::kErrorNotRegistered);
  base::RepeatingClosure log_all_connection_result_repeated_without_error =
      base::BindRepeating(&NetworkMetricsHelper::LogAllConnectionResult,
                          kTestGuid,
                          /*is_auto_connect=*/false, /*is_repeated_error=*/true,
                          /*shill_error=*/std::nullopt);

  base::RepeatingClosure log_user_initiated_connection_result =
      base::BindRepeating(
          &NetworkMetricsHelper::LogUserInitiatedConnectionResult, kTestGuid,
          shill::kErrorNotRegistered);
  base::RepeatingClosure log_connection_state_result = base::BindRepeating(
      &NetworkMetricsHelper::LogConnectionStateResult, kTestGuid,
      NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  base::RepeatingClosure log_connection_state_shill_error_result =
      base::BindRepeating(
          &NetworkMetricsHelper::LogConnectionStateResult, kTestGuid,
          NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
          /*shill_error=*/ShillConnectResult::kUnknown);

  for (const auto& provider : kProviders) {
    bool failed_to_log_result = false;

    shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                      shill::kTypeVPN, shill::kStateIdle,
                                      /*visible=*/true);
    shill_service_client_->SetServiceProperty(
        kTestServicePath, shill::kProviderTypeProperty, base::Value(provider));
    shill_service_client_->SetServiceProperty(kTestServicePath,
                                              shill::kProviderHostProperty,
                                              base::Value(kTestVpnHost));
    base::RunLoop().RunUntilIdle();

    // We call LogAllConnectionResult() 3 times here to check various
    // combinations if connection results and repeated failures. The counts will
    // be increased, by 3 or 2 depending on if the failure is repeated.
    LogVpnResult(provider, log_all_connection_result_not_repeated_with_error,
                 &failed_to_log_result);
    LogVpnResult(provider, log_all_connection_result_repeated_with_error,
                 &failed_to_log_result);
    LogVpnResult(provider, log_all_connection_result_repeated_without_error,
                 &failed_to_log_result);

    LogVpnResult(provider, log_user_initiated_connection_result,
                 &failed_to_log_result);
    LogVpnResult(provider, log_connection_state_result, &failed_to_log_result);
    LogVpnResult(provider, log_connection_state_shill_error_result,
                 &failed_to_log_result);

    if (!failed_to_log_result) {
      if (provider == shill::kProviderThirdPartyVpn ||
          provider == shill::kProviderArcVpn) {
        expected_third_party_count += kTotalCountIncrement;
        expected_third_party_count_user_initiated +=
            kUserInitiatedCountIncrement;
        expected_third_party_filtered_count += kFilteredCountIncrement;
      } else if (provider == shill::kProviderIKEv2 ||
                 provider == shill::kProviderL2tpIpsec ||
                 provider == shill::kProviderOpenVpn ||
                 provider == shill::kProviderWireGuard) {
        expected_built_in_count += kTotalCountIncrement;
        expected_built_in_count_user_initiated += kUserInitiatedCountIncrement;
        expected_built_in_fitered_count += kFilteredCountIncrement;
      } else {
        expected_unknown_count += kTotalCountIncrement;
        expected_unknown_count_user_initiated += kUserInitiatedCountIncrement;
        expected_unknown_filtered_count += kFilteredCountIncrement;
      }
      expected_all_count += kTotalCountIncrement;
      expected_filtered_count += kFilteredCountIncrement;
      expected_user_initiated_count += kUserInitiatedCountIncrement;
    }

    histogram_tester_->ExpectTotalCount(kVpnConnectResultAllHistogram,
                                        expected_all_count);
    histogram_tester_->ExpectTotalCount(kVpnBuiltInConnectResultAllHistogram,
                                        expected_built_in_count);
    histogram_tester_->ExpectTotalCount(kVpnThirdPartyConnectResultAllHistogram,
                                        expected_third_party_count);
    histogram_tester_->ExpectTotalCount(kVpnUnknownConnectResultAllHistogram,
                                        expected_unknown_count);
    histogram_tester_->ExpectTotalCount(kVpnConnectResultUserInitiatedHistogram,
                                        expected_user_initiated_count);

    histogram_tester_->ExpectTotalCount(kVpnConnectResultFilteredHistogram,
                                        expected_filtered_count);
    histogram_tester_->ExpectTotalCount(
        kVpnBuiltInConnectResultFilteredHistogram,
        expected_built_in_fitered_count);
    histogram_tester_->ExpectTotalCount(
        kVpnThirdPartyConnectResultFilteredHistogram,
        expected_third_party_filtered_count);
    histogram_tester_->ExpectTotalCount(
        kVpnUnknownConnectResultFilteredHistogram,
        expected_unknown_filtered_count);

    histogram_tester_->ExpectTotalCount(
        kVpnBuiltInConnectResultUserInitiatedHistogram,
        expected_built_in_count_user_initiated);
    histogram_tester_->ExpectTotalCount(
        kVpnThirdPartyConnectResultUserInitiatedHistogram,
        expected_third_party_count_user_initiated);
    histogram_tester_->ExpectTotalCount(
        kVpnUnknownConnectResultUserInitiatedHistogram,
        expected_unknown_count_user_initiated);

    histogram_tester_->ExpectTotalCount(
        kVpnConnectionStateHistogram,
        expected_user_initiated_count * kConnectionStateCountScale);
    histogram_tester_->ExpectTotalCount(kVpnConnectionStateShillErrorHistogram,
                                        expected_user_initiated_count);
    histogram_tester_->ExpectBucketCount(kVpnConnectionStateShillErrorHistogram,
                                         ShillConnectResult::kUnknown,
                                         expected_user_initiated_count);

    histogram_tester_->ExpectTotalCount(
        kVpnBuiltInConnectionStateHistogram,
        expected_built_in_count_user_initiated * kConnectionStateCountScale);
    histogram_tester_->ExpectTotalCount(
        kVpnBuiltInConnectionStateShillErrorHistogram,
        expected_built_in_count_user_initiated);
    histogram_tester_->ExpectBucketCount(
        kVpnBuiltInConnectionStateShillErrorHistogram,
        ShillConnectResult::kUnknown, expected_built_in_count_user_initiated);

    histogram_tester_->ExpectTotalCount(
        kVpnThirdPartyConnectionStateHistogram,
        expected_third_party_count_user_initiated * kConnectionStateCountScale);
    histogram_tester_->ExpectTotalCount(
        kVpnThirdPartyConnectionStateShillErrorHistogram,
        expected_third_party_count_user_initiated);
    histogram_tester_->ExpectBucketCount(
        kVpnThirdPartyConnectionStateShillErrorHistogram,
        ShillConnectResult::kUnknown,
        expected_third_party_count_user_initiated);

    histogram_tester_->ExpectTotalCount(
        kVpnUnknownConnectionStateHistogram,
        expected_unknown_count_user_initiated * kConnectionStateCountScale);
    histogram_tester_->ExpectTotalCount(
        kVpnUnknownConnectionStateShillErrorHistogram,
        expected_unknown_count_user_initiated);
    histogram_tester_->ExpectBucketCount(
        kVpnUnknownConnectionStateShillErrorHistogram,
        ShillConnectResult::kUnknown, expected_unknown_count_user_initiated);

    shill_service_client_->RemoveService(kTestServicePath);
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(NetworkMetricsHelperTest, WifiOpen) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeWifi, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityClassNone));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/false,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiConnectResultNonUserInitiatedHistogram, 0);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 0);

  histogram_tester_->ExpectTotalCount(kWifiConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/true, /*is_repeated_error=*/true,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      kWifiConnectResultNonUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 0);

  histogram_tester_->ExpectTotalCount(kWifiConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultUserInitiatedHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateShillErrorHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateShillErrorHistogram,
                                      0);
  histogram_tester_->ExpectBucketCount(kWifiConnectionStateShillErrorHistogram,
                                       ShillConnectResult::kUnknown, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kWifiOpenConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);

  shill_service_client_->SetServiceProperty(kTestServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  base::RunLoop().RunUntilIdle();
  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateShillErrorHistogram,
                                      1);
  histogram_tester_->ExpectBucketCount(kWifiConnectionStateShillErrorHistogram,
                                       ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kWifiOpenConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      1);
}

TEST_F(NetworkMetricsHelperTest, WifiPasswordProtected) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeWifi, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityClassPsk));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/false,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiConnectResultNonUserInitiatedHistogram, 0);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 1);

  histogram_tester_->ExpectTotalCount(kWifiConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultFilteredHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultFilteredHistogram, 1);

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/true, /*is_repeated_error=*/true,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      kWifiConnectResultNonUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 2);

  histogram_tester_->ExpectTotalCount(kWifiConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultFilteredHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultFilteredHistogram, 1);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultUserInitiatedHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectResultUserInitiatedHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultUserInitiatedHistogram, 1);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateShillErrorHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateShillErrorHistogram,
                                      0);
  histogram_tester_->ExpectBucketCount(kWifiConnectionStateShillErrorHistogram,
                                       ShillConnectResult::kUnknown, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kWifiOpenConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);

  shill_service_client_->SetServiceProperty(kTestServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  base::RunLoop().RunUntilIdle();
  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateShillErrorHistogram,
                                      1);
  histogram_tester_->ExpectBucketCount(kWifiConnectionStateShillErrorHistogram,
                                       ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kWifiPasswordProtectedConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
}

TEST_F(NetworkMetricsHelperTest, EthernetNoEap) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeEthernet, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityClassNone));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/false,
      shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kEthernetConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectResultAllHistogram,
                                      1);

  histogram_tester_->ExpectTotalCount(kEthernetConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultFilteredHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultFilteredHistogram, 1);

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false, /*is_repeated_error=*/true,
      shill::kErrorNotRegistered);

  histogram_tester_->ExpectTotalCount(kEthernetConnectResultAllHistogram, 2);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectResultAllHistogram,
                                      2);

  histogram_tester_->ExpectTotalCount(kEthernetConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultFilteredHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultFilteredHistogram, 1);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultUserInitiatedHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultUserInitiatedHistogram, 1);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kEthernetConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectionStateHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEthernetConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kEthernetEapConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEthernetNoEapConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
}

TEST_F(NetworkMetricsHelperTest, EthernetEap) {
  ShillDeviceClient::TestInterface* device_test =
      network_handler_test_helper_->device_test();
  device_test->ClearDevices();
  device_test->AddDevice(kTestDevicePath, shill::kTypeEthernet, kTestName);
  shill_service_client_->AddService(kTestServicePath1, kTestGuid, kTestName,
                                    shill::kTypeEthernetEap, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeEthernet, shill::kStateIdle,
                                    /*visible=*/true);

  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kStateProperty, base::Value(shill::kStateReady));

  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityClass8021x));

  device_test->SetDeviceProperty(kTestDevicePath,
                                 shill::kEapAuthenticationCompletedProperty,
                                 base::Value(true), /*notify_changed=*/false);
  base::RunLoop().RunUntilIdle();

  // Setting up the Ethernet Eap connection in tests may cause metrics to be
  // logged automatically by ConnectionInfoMetricsLogger.
  histogram_tester_.reset(new base::HistogramTester());
  histogram_tester_->ExpectTotalCount(kEthernetConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectResultAllHistogram,
                                      0);

  histogram_tester_->ExpectTotalCount(kEthernetConnectResultFilteredHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultFilteredHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogAllConnectionResult(
      kTestGuid, /*is_auto_connect=*/false,
      /*is_repeated_error=*/false, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kEthernetConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectResultAllHistogram,
                                      0);

  histogram_tester_->ExpectTotalCount(kEthernetConnectResultFilteredHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultFilteredHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultFilteredHistogram, 0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected,
      /*shill_error=*/std::nullopt);
  histogram_tester_->ExpectTotalCount(kEthernetConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectionStateHistogram,
                                      0);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectionStateShillErrorHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid,
      NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
      /*shill_error=*/ShillConnectResult::kUnknown);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEthernetConnectionStateShillErrorHistogram, ShillConnectResult::kUnknown,
      1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectionStateShillErrorHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      kEthernetEapConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectionStateShillErrorHistogram, 0);
  histogram_tester_->ExpectBucketCount(
      kEthernetNoEapConnectionStateShillErrorHistogram,
      ShillConnectResult::kUnknown, 0);
}

}  // namespace ash
