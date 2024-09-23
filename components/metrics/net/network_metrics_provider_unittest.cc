// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/network_metrics_provider.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_IOS)
#include "ios/web/public/test/web_task_environment.h"
#else  // !BUILDFLAG(IS_IOS)
#include "content/public/test/browser_task_environment.h"
#endif  // BUILDFLAG(IS_IOS)

namespace metrics {

class NetworkMetricsProviderTest : public testing::Test {
 public:
  NetworkMetricsProviderTest(const NetworkMetricsProviderTest&) = delete;
  NetworkMetricsProviderTest& operator=(const NetworkMetricsProviderTest&) =
      delete;

 protected:
  NetworkMetricsProviderTest() = default;
  ~NetworkMetricsProviderTest() override = default;

 private:
#if BUILDFLAG(IS_IOS)
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
#else
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

// Verifies that the effective connection type is correctly set.
TEST_F(NetworkMetricsProviderTest, EffectiveConnectionType) {
  NetworkMetricsProvider network_metrics_provider(
      network::TestNetworkConnectionTracker::CreateAsyncGetter());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.max_effective_connection_type_);
  SystemProfileProto system_profile;
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            system_profile.network().max_effective_connection_type());

  network_metrics_provider.OnEffectiveConnectionTypeChanged(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.max_effective_connection_type_);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  network_metrics_provider.OnEffectiveConnectionTypeChanged(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.max_effective_connection_type_);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  // Effective connection type changed from 2G to SLOW_2G during the lifetime of
  // the log. Minimum value of ECT must be different from the maximum value.
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  // Getting the system profile again should return the current effective
  // connection type.
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            system_profile.network().max_effective_connection_type());
}

// Verifies that the effective connection type is not set to UNKNOWN when there
// is a change in the connection type.
TEST_F(NetworkMetricsProviderTest, ECTAmbiguousOnConnectionTypeChange) {
  NetworkMetricsProvider network_metrics_provider(
      network::TestNetworkConnectionTracker::CreateAsyncGetter());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.max_effective_connection_type_);

  network_metrics_provider.OnEffectiveConnectionTypeChanged(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            network_metrics_provider.max_effective_connection_type_);

  // There is no change in the connection type. Effective connection types
  // should be reported as 2G.
  SystemProfileProto system_profile;
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  // Even with change in the connection type, effective connection types
  // should be reported as 2G.
  network_metrics_provider.OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_2G);
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());
}

// Verifies that the effective connection type is not set to UNKNOWN when the
// connection type is OFFLINE.
TEST_F(NetworkMetricsProviderTest, ECTNotAmbiguousOnUnknownOrOffline) {
  for (net::EffectiveConnectionType force_ect :
       {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE}) {
    NetworkMetricsProvider network_metrics_provider(
        network::TestNetworkConnectionTracker::CreateAsyncGetter());
    base::RunLoop().RunUntilIdle();

    network_metrics_provider.OnEffectiveConnectionTypeChanged(
        net::EFFECTIVE_CONNECTION_TYPE_2G);

    SystemProfileProto system_profile;
    network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);

    network_metrics_provider.OnEffectiveConnectionTypeChanged(force_ect);

    network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
              system_profile.network().min_effective_connection_type());
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
              system_profile.network().max_effective_connection_type());

    network_metrics_provider.OnEffectiveConnectionTypeChanged(
        net::EFFECTIVE_CONNECTION_TYPE_4G);
    network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G,
              system_profile.network().min_effective_connection_type());
    EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G,
              system_profile.network().max_effective_connection_type());
  }
}

// Verifies that the connection type is ambiguous boolean is correctly set.
TEST_F(NetworkMetricsProviderTest, ConnectionTypeIsAmbiguous) {
  NetworkMetricsProvider network_metrics_provider(
      network::TestNetworkConnectionTracker::CreateAsyncGetter());

  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_UNKNOWN,
            network_metrics_provider.connection_type_);
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_FALSE(
      network_metrics_provider.network_connection_tracker_initialized_);

  // When a connection type change callback is received, network change notifier
  // should be marked as initialized.
  network_metrics_provider.OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_2G);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_2G,
            network_metrics_provider.connection_type_);
  // Connection type should not be marked as ambiguous when a delayed connection
  // type change callback is received due to delayed initialization of the
  // network change notifier.
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_connection_tracker_initialized_);

  // On collection of the system profile, |connection_type_is_ambiguous_| should
  // stay false, and |network_connection_tracker_initialized_| should remain
  // true.
  SystemProfileProto system_profile;
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_connection_tracker_initialized_);
  EXPECT_FALSE(system_profile.network().connection_type_is_ambiguous());
  EXPECT_EQ(SystemProfileProto::Network::CONNECTION_2G,
            system_profile.network().connection_type());

  network_metrics_provider.OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_connection_tracker_initialized_);

  // On collection of the system profile, |connection_type_is_ambiguous_| should
  // be reset to false, and |network_connection_tracker_initialized_| should
  // remain true.
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_connection_tracker_initialized_);
  EXPECT_TRUE(system_profile.network().connection_type_is_ambiguous());
  EXPECT_EQ(SystemProfileProto::Network::CONNECTION_3G,
            system_profile.network().connection_type());
}

}  // namespace metrics
