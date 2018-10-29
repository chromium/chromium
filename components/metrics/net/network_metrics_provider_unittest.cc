// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/network_metrics_provider.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#endif  // OS_CHROMEOS

namespace metrics {

class NetworkMetricsProviderTest : public testing::Test {
 public:
 protected:
  NetworkMetricsProviderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
#endif  // OS_CHROMEOS
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

// Verifies that the effective connection type is correctly set.
TEST_F(NetworkMetricsProviderTest, EffectiveConnectionType) {
  SystemProfileProto system_profile;
  NetworkMetricsProvider network_metrics_provider;
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.min_effective_connection_type_);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            network_metrics_provider.max_effective_connection_type_);
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
  SystemProfileProto system_profile;
  NetworkMetricsProvider network_metrics_provider;
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
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            system_profile.network().max_effective_connection_type());

  // Even with change in the connection type, effective connection types
  // should be reported as 2G.
  network_metrics_provider.OnNetworkChanged(
      net::NetworkChangeNotifier::CONNECTION_2G);
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
    NetworkMetricsProvider network_metrics_provider;
    base::RunLoop().RunUntilIdle();

    SystemProfileProto system_profile;
    network_metrics_provider.OnEffectiveConnectionTypeChanged(
        net::EFFECTIVE_CONNECTION_TYPE_2G);

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
  SystemProfileProto system_profile;
  NetworkMetricsProvider network_metrics_provider;

  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            network_metrics_provider.connection_type_);
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_FALSE(network_metrics_provider.network_change_notifier_initialized_);

  // When a connection type change callback is received, network change notifier
  // should be marked as initialized.
  network_metrics_provider.OnNetworkChanged(
      net::NetworkChangeNotifier::CONNECTION_2G);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_2G,
            network_metrics_provider.connection_type_);
  // Connection type should not be marked as ambiguous when a delayed connection
  // type change callback is received due to delayed initialization of the
  // network change notifier.
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_change_notifier_initialized_);

  // On collection of the system profile, |connection_type_is_ambiguous_| should
  // stay false, and |network_change_notifier_initialized_| should remain true.
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_change_notifier_initialized_);
  EXPECT_FALSE(system_profile.network().connection_type_is_ambiguous());
  EXPECT_EQ(SystemProfileProto::Network::CONNECTION_2G,
            system_profile.network().connection_type());

  network_metrics_provider.OnNetworkChanged(
      net::NetworkChangeNotifier::CONNECTION_3G);
  EXPECT_TRUE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_change_notifier_initialized_);

  // On collection of the system profile, |connection_type_is_ambiguous_| should
  // be reset to false, and |network_change_notifier_initialized_| should remain
  // true.
  network_metrics_provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_FALSE(network_metrics_provider.connection_type_is_ambiguous_);
  EXPECT_TRUE(network_metrics_provider.network_change_notifier_initialized_);
  EXPECT_TRUE(system_profile.network().connection_type_is_ambiguous());
  EXPECT_EQ(SystemProfileProto::Network::CONNECTION_3G,
            system_profile.network().connection_type());
}

}  // namespace metrics
