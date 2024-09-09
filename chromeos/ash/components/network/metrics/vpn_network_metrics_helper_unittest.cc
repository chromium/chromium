// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/vpn_network_metrics_helper.h"

#include <memory>
#include <utility>

#include "base/debug/debugging_buildflags.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kVpnHistogramConfigurationSourceArc[] =
    "Network.Ash.VPN.ARC.ConfigurationSource";
const char kVpnHistogramConfigurationSourceIKEv2[] =
    "Network.Ash.VPN.IKEv2.ConfigurationSource";
const char kVpnHistogramConfigurationSourceL2tpIpsec[] =
    "Network.Ash.VPN.L2TPIPsec.ConfigurationSource";
const char kVpnHistogramConfigurationSourceOpenVpn[] =
    "Network.Ash.VPN.OpenVPN.ConfigurationSource";
const char kVpnHistogramConfigurationSourceThirdParty[] =
    "Network.Ash.VPN.ThirdParty.ConfigurationSource";
const char kVpnHistogramConfigurationSourceWireGuard[] =
    "Network.Ash.VPN.WireGuard.ConfigurationSource";
const char kVpnHistogramConfigurationSourceUnknown[] =
    "Network.Ash.VPN.Unknown.ConfigurationSource";

const char kTestUnknownVpn[] = "test_unknown_vpn";

void ErrorCallback(const std::string& error_name) {
  ADD_FAILURE() << "Unexpected error: " << error_name;
}

// Helper function to create a VPN network using NetworkConfigurationHandler.
void CreateTestShillConfiguration(const std::string& vpn_provider_type,
                                  bool is_managed) {
  base::Value::Dict properties;

  properties.Set(shill::kGuidProperty, "vpn_guid");
  properties.Set(shill::kTypeProperty, shill::kTypeVPN);
  properties.Set(shill::kStateProperty, shill::kStateIdle);
  properties.Set(shill::kProviderHostProperty, "vpn_host");
  properties.Set(shill::kProviderTypeProperty, vpn_provider_type);
  properties.Set(shill::kProfileProperty,
                 NetworkProfileHandler::GetSharedProfilePath());

  if (is_managed) {
    properties.Set(shill::kONCSourceProperty, shill::kONCSourceDevicePolicy);
    std::unique_ptr<NetworkUIData> ui_data = NetworkUIData::CreateFromONC(
        ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
    properties.Set(shill::kUIDataProperty, ui_data->GetAsJson());
  }

  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(std::move(properties), base::DoNothing(),
                                 base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class VpnNetworkMetricsHelperTest : public testing::Test {
 public:
  VpnNetworkMetricsHelperTest() = default;
  VpnNetworkMetricsHelperTest(const VpnNetworkMetricsHelperTest&) = delete;
  VpnNetworkMetricsHelperTest& operator=(const VpnNetworkMetricsHelperTest&) =
      delete;
  ~VpnNetworkMetricsHelperTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ClearServices();
  }

  void TearDown() override {
    ClearServices();
    network_handler_test_helper_.reset();
  }

 protected:
  void ClearServices() {
    ShillServiceClient::Get()->GetTestInterface()->ClearServices();
    base::RunLoop().RunUntilIdle();
  }

  void ExpectConfigurationSourceCounts(const char* histogram,
                                       size_t manual_count,
                                       size_t policy_count) {
    histogram_tester_->ExpectBucketCount<
        VpnNetworkMetricsHelper::VPNConfigurationSource>(
        histogram,
        VpnNetworkMetricsHelper::VPNConfigurationSource::kConfiguredManually,
        manual_count);
    histogram_tester_->ExpectBucketCount<
        VpnNetworkMetricsHelper::VPNConfigurationSource>(
        histogram,
        VpnNetworkMetricsHelper::VPNConfigurationSource::kConfiguredByPolicy,
        policy_count);
    histogram_tester_->ExpectTotalCount(histogram, manual_count + policy_count);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(VpnNetworkMetricsHelperTest, LogVpnVPNConfigurationSource) {
  const std::vector<std::pair<const std::string, const char*>>
      kProvidersAndHistograms{{
          {shill::kProviderIKEv2, kVpnHistogramConfigurationSourceIKEv2},
          {shill::kProviderL2tpIpsec,
           kVpnHistogramConfigurationSourceL2tpIpsec},
          {shill::kProviderArcVpn, kVpnHistogramConfigurationSourceArc},
          {shill::kProviderOpenVpn, kVpnHistogramConfigurationSourceOpenVpn},
          {shill::kProviderThirdPartyVpn,
           kVpnHistogramConfigurationSourceThirdParty},
          {shill::kProviderWireGuard,
           kVpnHistogramConfigurationSourceWireGuard},
          {kTestUnknownVpn, kVpnHistogramConfigurationSourceUnknown},
      }};

  for (const auto& it : kProvidersAndHistograms) {
    ExpectConfigurationSourceCounts(it.first.c_str(), /*manual_count=*/0,
                                    /*policy_count=*/0);

// Emitting a metric for an unknown VPN provider will always cause a
// DUMP_WILL_BE_NOTREACHED() to be hit. This is fatal outside official builds,
// so make sure that we die in those configurations.
#if !defined(OFFICIAL_BUILD)
    if (it.first == kTestUnknownVpn) {
      ASSERT_DEATH(
          {
            CreateTestShillConfiguration(kTestUnknownVpn, /*is_managed=*/false);
          },
          "");
      ClearServices();
      ASSERT_DEATH(
          {
            CreateTestShillConfiguration(kTestUnknownVpn, /*is_managed=*/true);
          },
          "");
      ClearServices();
      ExpectConfigurationSourceCounts(it.second, /*manual_count=*/0,
                                      /*policy_count=*/0);
      continue;
    }
#endif  // !defined(OFFICIAL_BUILD)

    CreateTestShillConfiguration(it.first, /*is_managed=*/false);
    ExpectConfigurationSourceCounts(it.second, /*manual_count=*/1,
                                    /*policy_count=*/0);
    ClearServices();

    CreateTestShillConfiguration(it.first, /*is_managed=*/true);
    ExpectConfigurationSourceCounts(it.second, /*manual_count=*/1,
                                    /*policy_count=*/1);
    ClearServices();
  }
}

}  // namespace ash
