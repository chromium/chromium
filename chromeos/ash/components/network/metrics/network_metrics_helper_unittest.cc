// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"

#include <memory>

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
const char kCellularPSimConnectResultAllHistogram[] =
    "Network.Ash.Cellular.PSim.ConnectionResult.All";
const char kCellularESimPolicyConnectResultAllHistogram[] =
    "Network.Ash.Cellular.ESim.Policy.ConnectionResult.All";

// LogAllConnectionResult() VPN histograms.
const char kVpnConnectResultAllHistogram[] =
    "Network.Ash.VPN.ConnectionResult.All";
const char kVpnBuiltInConnectResultAllHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.ConnectionResult.All";
const char kVpnThirdPartyConnectResultAllHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.ConnectionResult.All";
const char kVpnUnknownConnectResultAllHistogram[] =
    "Network.Ash.VPN.TypeUnknown.ConnectionResult.All";

// LogAllConnectionResult() WiFi histograms.
const char kWifiConnectResultAllHistogram[] =
    "Network.Ash.WiFi.ConnectionResult.All";
const char kWifiOpenConnectResultAllHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.ConnectionResult.All";
const char kWifiPasswordProtectedConnectResultAllHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected.ConnectionResult.All";

// LogAllConnectionResult() Ethernet histograms.
const char kEthernetConnectResultAllHistogram[] =
    "Network.Ash.Ethernet.ConnectionResult.All";
const char kEthernetEapConnectResultAllHistogram[] =
    "Network.Ash.Ethernet.Eap.ConnectionResult.All";
const char kEthernetNoEapConnectResultAllHistogram[] =
    "Network.Ash.Ethernet.NoEap.ConnectionResult.All";

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

// LogConnectionStateResult() VPN histograms.
const char kVpnConnectionStateHistogram[] =
    "Network.Ash.VPN.DisconnectionsWithoutUserAction";
const char kVpnBuiltInConnectionStateHistogram[] =
    "Network.Ash.VPN.TypeBuiltIn.DisconnectionsWithoutUserAction";
const char kVpnThirdPartyConnectionStateHistogram[] =
    "Network.Ash.VPN.TypeThirdParty.DisconnectionsWithoutUserAction";
const char kVpnUnknownConnectionStateHistogram[] =
    "Network.Ash.VPN.TypeUnknown.DisconnectionsWithoutUserAction";

// LogConnectionStateResult() WiFi histograms.
const char kWifiConnectionStateHistogram[] =
    "Network.Ash.WiFi.DisconnectionsWithoutUserAction";
const char kWifiOpenConnectionStateHistogram[] =
    "Network.Ash.WiFi.SecurityOpen.DisconnectionsWithoutUserAction";
const char kWifiPasswordProtectedConnectionStateHistogram[] =
    "Network.Ash.WiFi.SecurityPasswordProtected."
    "DisconnectionsWithoutUserAction";

// LogConnectionStateResult() Ethernet histograms.
const char kEthernetConnectionStateHistogram[] =
    "Network.Ash.Ethernet.DisconnectionsWithoutUserAction";
const char kEthernetEapConnectionStateHistogram[] =
    "Network.Ash.Ethernet.Eap.DisconnectionsWithoutUserAction";
const char kEthernetNoEapConnectionStateHistogram[] =
    "Network.Ash.Ethernet.NoEap.DisconnectionsWithoutUserAction";

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

// Emitting a metric for an unknown VPN provider will always cause a NOTREACHED
// to be hit. This can cause a CHECK to fail, depending on the build flags. We
// catch any failing CHECK below by asserting that we will crash when emitting.
#if !BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
  if (provider == kTestUnknownVpn) {
    ASSERT_DEATH({ func.Run(); }, "");
    *failed_to_log_result = true;
    return;
  }
#endif  // !BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
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
  ShillServiceClient::TestInterface* shill_service_client_;
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

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectionStateHistogram, 0);
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

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      0);

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
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimPolicyConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectionStateHistogram, 0);
}

TEST_F(NetworkMetricsHelperTest, CellularPSim) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeCellular, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kIccidProperty, base::Value("iccid"));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kCellularConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectResultAllHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectResultAllHistogram,
                                      0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kCellularConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularPSimConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kCellularESimConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kCellularPSimConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kCellularESimConnectionStateHistogram, 0);
}

TEST_F(NetworkMetricsHelperTest, VPN) {
  const std::vector<const std::string> kProviders{{
      shill::kProviderIKEv2,
      shill::kProviderL2tpIpsec,
      shill::kProviderArcVpn,
      shill::kProviderOpenVpn,
      shill::kProviderThirdPartyVpn,
      shill::kProviderWireGuard,
      kTestUnknownVpn,
  }};

  size_t expected_all_count = 0;
  size_t expected_user_initiated_count = 0;
  size_t expected_built_in_count = 0;
  size_t expected_third_party_count = 0;
  size_t expected_unknown_count = 0;

  base::RepeatingClosure log_all_connection_result =
      base::BindRepeating(&NetworkMetricsHelper::LogAllConnectionResult,
                          kTestGuid, shill::kErrorNotRegistered);
  base::RepeatingClosure log_user_initiated_connection_result =
      base::BindRepeating(
          &NetworkMetricsHelper::LogUserInitiatedConnectionResult, kTestGuid,
          shill::kErrorNotRegistered);
  base::RepeatingClosure log_connection_state_result = base::BindRepeating(
      &NetworkMetricsHelper::LogConnectionStateResult, kTestGuid,
      NetworkMetricsHelper::ConnectionState::kConnected);

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

    LogVpnResult(provider, log_all_connection_result, &failed_to_log_result);
    LogVpnResult(provider, log_user_initiated_connection_result,
                 &failed_to_log_result);
    LogVpnResult(provider, log_connection_state_result, &failed_to_log_result);

    if (!failed_to_log_result) {
      if (provider == shill::kProviderThirdPartyVpn ||
          provider == shill::kProviderArcVpn) {
        ++expected_third_party_count;
      } else if (provider == shill::kProviderIKEv2 ||
                 provider == shill::kProviderL2tpIpsec ||
                 provider == shill::kProviderOpenVpn ||
                 provider == shill::kProviderWireGuard) {
        ++expected_built_in_count;
      } else {
        ++expected_unknown_count;
      }
      ++expected_all_count;
      ++expected_user_initiated_count;
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
    histogram_tester_->ExpectTotalCount(
        kVpnBuiltInConnectResultUserInitiatedHistogram,
        expected_built_in_count);
    histogram_tester_->ExpectTotalCount(
        kVpnThirdPartyConnectResultUserInitiatedHistogram,
        expected_third_party_count);
    histogram_tester_->ExpectTotalCount(
        kVpnUnknownConnectResultUserInitiatedHistogram, expected_unknown_count);

    histogram_tester_->ExpectTotalCount(kVpnConnectionStateHistogram,
                                        expected_user_initiated_count);
    histogram_tester_->ExpectTotalCount(kVpnBuiltInConnectionStateHistogram,
                                        expected_built_in_count);
    histogram_tester_->ExpectTotalCount(kVpnThirdPartyConnectionStateHistogram,
                                        expected_third_party_count);
    histogram_tester_->ExpectTotalCount(kVpnUnknownConnectionStateHistogram,
                                        expected_unknown_count);

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

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 0);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultUserInitiatedHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateHistogram, 0);
}

TEST_F(NetworkMetricsHelperTest, WifiPasswordProtected) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeWifi, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityClassPsk));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultAllHistogram, 1);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kWifiConnectResultUserInitiatedHistogram,
                                      1);
  histogram_tester_->ExpectTotalCount(
      kWifiOpenConnectResultUserInitiatedHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectResultUserInitiatedHistogram, 1);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kWifiConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kWifiOpenConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kWifiPasswordProtectedConnectionStateHistogram, 1);
}

TEST_F(NetworkMetricsHelperTest, EthernetNoEap) {
  shill_service_client_->AddService(kTestServicePath, kTestGuid, kTestName,
                                    shill::kTypeEthernet, shill::kStateIdle,
                                    /*visible=*/true);
  shill_service_client_->SetServiceProperty(
      kTestServicePath, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityClassNone));
  base::RunLoop().RunUntilIdle();

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kEthernetConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectResultAllHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectResultAllHistogram,
                                      1);

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultUserInitiatedHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultUserInitiatedHistogram, 1);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kEthernetConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectionStateHistogram, 0);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectionStateHistogram,
                                      1);
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

  NetworkMetricsHelper::LogAllConnectionResult(kTestGuid,
                                               shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(kEthernetConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectResultAllHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectResultAllHistogram,
                                      0);
  NetworkMetricsHelper::LogUserInitiatedConnectionResult(
      kTestGuid, shill::kErrorNotRegistered);
  histogram_tester_->ExpectTotalCount(
      kEthernetConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetEapConnectResultUserInitiatedHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      kEthernetNoEapConnectResultUserInitiatedHistogram, 0);

  NetworkMetricsHelper::LogConnectionStateResult(
      kTestGuid, NetworkMetricsHelper::ConnectionState::kConnected);
  histogram_tester_->ExpectTotalCount(kEthernetConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetEapConnectionStateHistogram, 1);
  histogram_tester_->ExpectTotalCount(kEthernetNoEapConnectionStateHistogram,
                                      0);
}

}  // namespace ash
