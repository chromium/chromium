// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"

#include <sys/types.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::cros_healthd {
namespace {

using ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines;
using ::chromeos::network_diagnostics::mojom::RoutineProblems;
using ::chromeos::network_diagnostics::mojom::RoutineResult;
using ::chromeos::network_diagnostics::mojom::RoutineResultPtr;
using ::chromeos::network_diagnostics::mojom::RoutineType;
using ::chromeos::network_diagnostics::mojom::RoutineVerdict;
using ::chromeos::network_health::mojom::NetworkEventsObserver;
using ::chromeos::network_health::mojom::NetworkHealthService;
using ::chromeos::network_health::mojom::NetworkHealthState;
using ::chromeos::network_health::mojom::NetworkHealthStatePtr;
using ::chromeos::network_health::mojom::NetworkState;
using ::chromeos::network_health::mojom::UInt32ValuePtr;
using ::testing::_;
using ::testing::Invoke;

class MockNetworkHealthService : public NetworkHealthService {
 public:
  MockNetworkHealthService() : receiver_{this} {}
  MockNetworkHealthService(const MockNetworkHealthService&) = delete;
  MockNetworkHealthService& operator=(const MockNetworkHealthService&) = delete;

  MOCK_METHOD(void,
              AddObserver,
              (mojo::PendingRemote<NetworkEventsObserver>),
              (override));
  MOCK_METHOD(void,
              GetNetworkList,
              (NetworkHealthService::GetNetworkListCallback),
              (override));
  MOCK_METHOD(void,
              GetHealthSnapshot,
              (NetworkHealthService::GetHealthSnapshotCallback),
              (override));
  MOCK_METHOD(void,
              GetRecentlyActiveNetworks,
              (NetworkHealthService::GetRecentlyActiveNetworksCallback),
              (override));

  mojo::PendingRemote<NetworkHealthService> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<NetworkHealthService> receiver_;
};

class MockNetworkDiagnosticsRoutines : public NetworkDiagnosticsRoutines {
 public:
  MockNetworkDiagnosticsRoutines() : receiver_{this} {}
  MockNetworkDiagnosticsRoutines(const MockNetworkDiagnosticsRoutines&) =
      delete;
  MockNetworkDiagnosticsRoutines& operator=(
      const MockNetworkDiagnosticsRoutines&) = delete;

  MOCK_METHOD(void,
              GetResult,
              (const RoutineType type,
               NetworkDiagnosticsRoutines::GetResultCallback),
              (override));
  MOCK_METHOD(void,
              GetAllResults,
              (NetworkDiagnosticsRoutines::GetAllResultsCallback),
              (override));
  MOCK_METHOD(void,
              RunLanConnectivity,
              (NetworkDiagnosticsRoutines::RunLanConnectivityCallback),
              (override));
  MOCK_METHOD(void,
              RunSignalStrength,
              (NetworkDiagnosticsRoutines::RunSignalStrengthCallback),
              (override));
  MOCK_METHOD(void,
              RunGatewayCanBePinged,
              (NetworkDiagnosticsRoutines::RunGatewayCanBePingedCallback),
              (override));
  MOCK_METHOD(void,
              RunHasSecureWiFiConnection,
              (NetworkDiagnosticsRoutines::RunHasSecureWiFiConnectionCallback),
              (override));
  MOCK_METHOD(void,
              RunDnsResolverPresent,
              (NetworkDiagnosticsRoutines::RunDnsResolverPresentCallback),
              (override));
  MOCK_METHOD(void,
              RunDnsLatency,
              (NetworkDiagnosticsRoutines::RunDnsLatencyCallback),
              (override));
  MOCK_METHOD(void,
              RunDnsResolution,
              (NetworkDiagnosticsRoutines::RunDnsResolutionCallback),
              (override));
  MOCK_METHOD(void,
              RunCaptivePortal,
              (NetworkDiagnosticsRoutines::RunCaptivePortalCallback),
              (override));
  MOCK_METHOD(void,
              RunHttpFirewall,
              (NetworkDiagnosticsRoutines::RunHttpFirewallCallback),
              (override));
  MOCK_METHOD(void,
              RunHttpsFirewall,
              (NetworkDiagnosticsRoutines::RunHttpsFirewallCallback),
              (override));
  MOCK_METHOD(void,
              RunHttpsLatency,
              (NetworkDiagnosticsRoutines::RunHttpsLatencyCallback),
              (override));
  MOCK_METHOD(void,
              RunVideoConferencing,
              (const absl::optional<std::string>&,
               NetworkDiagnosticsRoutines::RunVideoConferencingCallback),
              (override));
  MOCK_METHOD(void,
              RunArcHttp,
              (NetworkDiagnosticsRoutines::RunArcHttpCallback),
              (override));
  MOCK_METHOD(void,
              RunArcDnsResolution,
              (NetworkDiagnosticsRoutines::RunArcDnsResolutionCallback),
              (override));
  MOCK_METHOD(void,
              RunArcPing,
              (NetworkDiagnosticsRoutines::RunArcPingCallback));

  mojo::PendingRemote<NetworkDiagnosticsRoutines> pending_remote() {
    if (receiver_.is_bound()) {
      receiver_.reset();
    }
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<NetworkDiagnosticsRoutines> receiver_;
};

class CrosHealthdServiceConnectionTest : public testing::Test {
 public:
  CrosHealthdServiceConnectionTest() = default;

  CrosHealthdServiceConnectionTest(const CrosHealthdServiceConnectionTest&) =
      delete;
  CrosHealthdServiceConnectionTest& operator=(
      const CrosHealthdServiceConnectionTest&) = delete;

  void SetUp() override { FakeCrosHealthd::Initialize(); }

  void TearDown() override {
    FakeCrosHealthd::Shutdown();
    // Reset the callback to prevent them being called after the tests finished.
    ServiceConnection::GetInstance()->SetBindNetworkHealthServiceCallback(
        ServiceConnection::BindNetworkHealthServiceCallback());
    ServiceConnection::GetInstance()->SetBindNetworkDiagnosticsRoutinesCallback(
        ServiceConnection::BindNetworkDiagnosticsRoutinesCallback());
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// Test that we can set the callback to get the NetworkHealthService remote and
// request the network health state snapshot.
TEST_F(CrosHealthdServiceConnectionTest, SetBindNetworkHealthService) {
  MockNetworkHealthService service;
  ServiceConnection::GetInstance()->SetBindNetworkHealthServiceCallback(
      base::BindLambdaForTesting(
          [&service] { return service.pending_remote(); }));

  base::RunLoop run_loop;
  auto canned_response = NetworkHealthState::New();
  EXPECT_CALL(service, GetHealthSnapshot(_))
      .WillOnce(
          Invoke([&](NetworkHealthService::GetHealthSnapshotCallback callback) {
            std::move(callback).Run(canned_response.Clone());
          }));

  FakeCrosHealthd::Get()->RequestNetworkHealthForTesting(
      base::BindLambdaForTesting([&](NetworkHealthStatePtr response) {
        EXPECT_EQ(canned_response, response);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that we can set the callback to get the NetworkDiagnosticsRoutines
// remote and run the lan connectivity routine.
TEST_F(CrosHealthdServiceConnectionTest, SetBindNetworkDiagnosticsRoutines) {
  MockNetworkDiagnosticsRoutines network_diagnostics_routines;
  ServiceConnection::GetInstance()->SetBindNetworkDiagnosticsRoutinesCallback(
      base::BindLambdaForTesting([&network_diagnostics_routines] {
        return network_diagnostics_routines.pending_remote();
      }));

  // Run the LanConnectivity routine so we know that
  // |network_diagnostics_routines| is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(network_diagnostics_routines, RunLanConnectivity(_))
      .WillOnce(Invoke(
          [&](NetworkDiagnosticsRoutines::RunLanConnectivityCallback callback) {
            auto result = RoutineResult::New();
            result->verdict = RoutineVerdict::kNoProblem;
            result->problems = RoutineProblems::NewLanConnectivityProblems({});
            std::move(callback).Run(std::move(result));
          }));

  FakeCrosHealthd::Get()->RunLanConnectivityRoutineForTesting(
      base::BindLambdaForTesting([&](RoutineResultPtr response) {
        EXPECT_EQ(RoutineVerdict::kNoProblem, response->verdict);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that we can get diagnostics service.
TEST_F(CrosHealthdServiceConnectionTest, GetDiagnosticsService) {
  auto* service = ServiceConnection::GetInstance()->GetDiagnosticsService();
  EXPECT_TRUE(service);
}

// Test that we can get probe service.
TEST_F(CrosHealthdServiceConnectionTest, GetProbeService) {
  auto* service = ServiceConnection::GetInstance()->GetProbeService();
  EXPECT_TRUE(service);
}

// Test that we can get event service.
TEST_F(CrosHealthdServiceConnectionTest, GetEventService) {
  auto* service = ServiceConnection::GetInstance()->GetEventService();
  EXPECT_TRUE(service);
}

// Test that we can bind diagnostics service.
TEST_F(CrosHealthdServiceConnectionTest, BindDiagnosticsService) {
  mojo::Remote<mojom::CrosHealthdDiagnosticsService> remote;
  ServiceConnection::GetInstance()->BindDiagnosticsService(
      remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

// Test that we can bind probe service.
TEST_F(CrosHealthdServiceConnectionTest, BindProbeService) {
  mojo::Remote<mojom::CrosHealthdProbeService> remote;
  ServiceConnection::GetInstance()->BindProbeService(
      remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

}  // namespace
}  // namespace ash::cros_healthd
