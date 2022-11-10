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

std::vector<mojom::DiagnosticRoutineEnum> MakeAvailableRoutines() {
  return std::vector<mojom::DiagnosticRoutineEnum>{
      mojom::DiagnosticRoutineEnum::kUrandom,
      mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      mojom::DiagnosticRoutineEnum::kBatteryHealth,
      mojom::DiagnosticRoutineEnum::kSmartctlCheck,
      mojom::DiagnosticRoutineEnum::kCpuCache,
      mojom::DiagnosticRoutineEnum::kCpuStress,
      mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy,
      mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
  };
}

mojom::RunRoutineResponsePtr MakeRunRoutineResponse() {
  return mojom::RunRoutineResponse::New(
      /*id=*/13, /*status=*/mojom::DiagnosticRoutineStatusEnum::kReady);
}

mojom::RoutineUpdatePtr MakeInteractiveRoutineUpdate() {
  mojom::InteractiveRoutineUpdate interactive_update(
      /*user_message=*/mojom::DiagnosticRoutineUserMessageEnum::kUnplugACPower);

  mojom::RoutineUpdateUnion update_union;
  update_union.set_interactive_update(interactive_update.Clone());

  return mojom::RoutineUpdate::New(
      /*progress_percent=*/42,
      /*output=*/mojo::ScopedHandle(), update_union.Clone());
}

mojom::RoutineUpdatePtr MakeNonInteractiveRoutineUpdate() {
  mojom::NonInteractiveRoutineUpdate noninteractive_update(
      /*status=*/mojom::DiagnosticRoutineStatusEnum::kRunning,
      /*status_message=*/"status_message");

  mojom::RoutineUpdateUnion update_union;
  update_union.set_noninteractive_update(noninteractive_update.Clone());

  return mojom::RoutineUpdate::New(
      /*progress_percent=*/43,
      /*output=*/mojo::ScopedHandle(), update_union.Clone());
}

class MockCrosHealthdBluetoothObserver
    : public mojom::CrosHealthdBluetoothObserver {
 public:
  MockCrosHealthdBluetoothObserver() : receiver_{this} {}
  MockCrosHealthdBluetoothObserver(const MockCrosHealthdBluetoothObserver&) =
      delete;
  MockCrosHealthdBluetoothObserver& operator=(
      const MockCrosHealthdBluetoothObserver&) = delete;

  MOCK_METHOD(void, OnAdapterAdded, (), (override));
  MOCK_METHOD(void, OnAdapterRemoved, (), (override));
  MOCK_METHOD(void, OnAdapterPropertyChanged, (), (override));
  MOCK_METHOD(void, OnDeviceAdded, (), (override));
  MOCK_METHOD(void, OnDeviceRemoved, (), (override));
  MOCK_METHOD(void, OnDevicePropertyChanged, (), (override));

  mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::CrosHealthdBluetoothObserver> receiver_;
};

class MockCrosHealthdLidObserver : public mojom::CrosHealthdLidObserver {
 public:
  MockCrosHealthdLidObserver() : receiver_{this} {}
  MockCrosHealthdLidObserver(const MockCrosHealthdLidObserver&) = delete;
  MockCrosHealthdLidObserver& operator=(const MockCrosHealthdLidObserver&) =
      delete;

  MOCK_METHOD(void, OnLidClosed, (), (override));
  MOCK_METHOD(void, OnLidOpened, (), (override));

  mojo::PendingRemote<mojom::CrosHealthdLidObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::CrosHealthdLidObserver> receiver_;
};

class MockCrosHealthdPowerObserver : public mojom::CrosHealthdPowerObserver {
 public:
  MockCrosHealthdPowerObserver() : receiver_{this} {}
  MockCrosHealthdPowerObserver(const MockCrosHealthdPowerObserver&) = delete;
  MockCrosHealthdPowerObserver& operator=(const MockCrosHealthdPowerObserver&) =
      delete;

  MOCK_METHOD(void, OnAcInserted, (), (override));
  MOCK_METHOD(void, OnAcRemoved, (), (override));
  MOCK_METHOD(void, OnOsSuspend, (), (override));
  MOCK_METHOD(void, OnOsResume, (), (override));

  mojo::PendingRemote<mojom::CrosHealthdPowerObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::CrosHealthdPowerObserver> receiver_;
};

class MockCrosHealthdNetworkObserver : public NetworkEventsObserver {
 public:
  MockCrosHealthdNetworkObserver() : receiver_{this} {}
  MockCrosHealthdNetworkObserver(const MockCrosHealthdNetworkObserver&) =
      delete;
  MockCrosHealthdNetworkObserver& operator=(
      const MockCrosHealthdNetworkObserver&) = delete;

  MOCK_METHOD(void,
              OnConnectionStateChanged,
              (const std::string&, NetworkState),
              (override));
  MOCK_METHOD(void,
              OnSignalStrengthChanged,
              (const std::string&, UInt32ValuePtr),
              (override));

  mojo::PendingRemote<NetworkEventsObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<NetworkEventsObserver> receiver_;
};

class MockCrosHealthdAudioObserver : public mojom::CrosHealthdAudioObserver {
 public:
  MockCrosHealthdAudioObserver() : receiver_{this} {}
  MockCrosHealthdAudioObserver(const MockCrosHealthdAudioObserver&) = delete;
  MockCrosHealthdAudioObserver& operator=(const MockCrosHealthdAudioObserver&) =
      delete;

  MOCK_METHOD(void, OnUnderrun, (), (override));
  MOCK_METHOD(void, OnSevereUnderrun, (), (override));

  mojo::PendingRemote<mojom::CrosHealthdAudioObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::CrosHealthdAudioObserver> receiver_;
};

class MockCrosHealthdThunderboltObserver
    : public mojom::CrosHealthdThunderboltObserver {
 public:
  MockCrosHealthdThunderboltObserver() : receiver_{this} {}
  MockCrosHealthdThunderboltObserver(
      const MockCrosHealthdThunderboltObserver&) = delete;
  MockCrosHealthdThunderboltObserver& operator=(
      const MockCrosHealthdThunderboltObserver&) = delete;

  MOCK_METHOD(void, OnAdd, (), (override));
  MOCK_METHOD(void, OnRemove, (), (override));
  MOCK_METHOD(void, OnAuthorized, (), (override));
  MOCK_METHOD(void, OnUnAuthorized, (), (override));

  mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::CrosHealthdThunderboltObserver> receiver_;
};

class MockCrosHealthdUsbObserver : public mojom::CrosHealthdUsbObserver {
 public:
  MockCrosHealthdUsbObserver() : receiver_{this} {}
  MockCrosHealthdUsbObserver(const MockCrosHealthdUsbObserver&) = delete;
  MockCrosHealthdUsbObserver& operator=(const MockCrosHealthdUsbObserver&) =
      delete;

  MOCK_METHOD(void, OnAdd, (mojom::UsbEventInfoPtr), (override));
  MOCK_METHOD(void, OnRemove, (mojom::UsbEventInfoPtr), (override));

  mojo::PendingRemote<mojom::CrosHealthdUsbObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::CrosHealthdUsbObserver> receiver_;
};

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

TEST_F(CrosHealthdServiceConnectionTest, GetAvailableRoutines) {
  // Test that we can retrieve a list of available routines.
  auto routines = MakeAvailableRoutines();
  FakeCrosHealthd::Get()->SetAvailableRoutinesForTesting(routines);
  bool callback_done = false;
  ServiceConnection::GetInstance()->GetAvailableRoutines(base::BindOnce(
      [](bool* callback_done,
         const std::vector<mojom::DiagnosticRoutineEnum>& response) {
        EXPECT_EQ(response, MakeAvailableRoutines());
        *callback_done = true;
      },
      &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, GetRoutineUpdate) {
  // Test that we can get an interactive routine update.
  auto interactive_update = MakeInteractiveRoutineUpdate();
  FakeCrosHealthd::Get()->SetGetRoutineUpdateResponseForTesting(
      interactive_update);
  bool callback_done = false;
  ServiceConnection::GetInstance()->GetRoutineUpdate(
      /*id=*/542, /*command=*/mojom::DiagnosticRoutineCommandEnum::kGetStatus,
      /*include_output=*/true,
      base::BindOnce(
          [](bool* callback_done, mojom::RoutineUpdatePtr response) {
            EXPECT_EQ(response, MakeInteractiveRoutineUpdate());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);

  // Test that we can get a noninteractive routine update.
  auto noninteractive_update = MakeNonInteractiveRoutineUpdate();
  FakeCrosHealthd::Get()->SetGetRoutineUpdateResponseForTesting(
      noninteractive_update);
  callback_done = false;
  ServiceConnection::GetInstance()->GetRoutineUpdate(
      /*id=*/543, /*command=*/mojom::DiagnosticRoutineCommandEnum::kCancel,
      /*include_output=*/false,
      base::BindOnce(
          [](bool* callback_done, mojom::RoutineUpdatePtr response) {
            EXPECT_EQ(response, MakeNonInteractiveRoutineUpdate());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunUrandomRoutine) {
  // Test that we can run the urandom routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunUrandomRoutine(
      /*length_seconds=*/absl::nullopt,
      base::BindOnce(
          [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
            EXPECT_EQ(response, MakeRunRoutineResponse());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunBatteryCapacityRoutine) {
  // Test that we can run the battery capacity routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunBatteryCapacityRoutine(base::BindOnce(
      [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        *callback_done = true;
      },
      &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunBatteryHealthRoutine) {
  // Test that we can run the battery health routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunBatteryHealthRoutine(base::BindOnce(
      [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        *callback_done = true;
      },
      &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunSmartctlCheckRoutine) {
  // Test that we can run the smartctl check routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunSmartctlCheckRoutine(base::BindOnce(
      [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        *callback_done = true;
      },
      &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunAcPowerRoutine) {
  // Test that we can run the AC power routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunAcPowerRoutine(
      mojom::AcPowerStatusEnum::kConnected,
      /*expected_power_type=*/"power_type",
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunCpuCacheRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunCpuCacheRoutine(
      /*exec_duration=*/absl::nullopt,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunCpuStressRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunCpuStressRoutine(
      /*exec_duration=*/absl::nullopt,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunFloatingPointAccuracyRoutine) {
  // Test that we can run the floating point accuracy routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunFloatingPointAccuracyRoutine(
      /*exec_duration=*/absl::nullopt,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunNvmeWearLevelRoutine) {
  // Test that we can run the NVMe wear-level routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  const std::vector<absl::optional<uint32_t>> test_cases = {50, absl::nullopt};
  for (const auto wear_level_threshold : test_cases) {
    base::RunLoop run_loop;
    ServiceConnection::GetInstance()->RunNvmeWearLevelRoutine(
        wear_level_threshold,
        base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
          EXPECT_EQ(response, MakeRunRoutineResponse());
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(CrosHealthdServiceConnectionTest, RunNvmeSelfTestRoutine) {
  // Test that we can run the NVMe self-test routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunNvmeSelfTestRoutine(
      mojom::NvmeSelfTestTypeEnum::kShortSelfTest,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunDiskReadRoutine) {
  // Test that we can run the disk read routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  base::TimeDelta exec_duration = base::Seconds(10);
  ServiceConnection::GetInstance()->RunDiskReadRoutine(
      mojom::DiskReadRoutineTypeEnum::kLinearRead,
      /*exec_duration=*/exec_duration, /*file_size_mb=*/1024,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunPrimeSearchRoutine) {
  // Test that we can run the prime search routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunPrimeSearchRoutine(
      /*exec_duration=*/absl::nullopt,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunBatteryDischargeRoutine) {
  // Test that we can run the battery discharge routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunBatteryDischargeRoutine(
      /*exec_duration=*/base::Seconds(12),
      /*maximum_discharge_percent_allowed=*/99,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the battery charge routine.
TEST_F(CrosHealthdServiceConnectionTest, RunBatteryChargeRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunBatteryChargeRoutine(
      /*exec_duration=*/base::Seconds(30),
      /*minimum_charge_percent_required=*/10,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the memory routine.
TEST_F(CrosHealthdServiceConnectionTest, RunMemoryRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunMemoryRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the LAN connectivity routine.
TEST_F(CrosHealthdServiceConnectionTest, RunLanConnectivityRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunLanConnectivityRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the signal strength routine.
TEST_F(CrosHealthdServiceConnectionTest, RunSignalStrengthRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunSignalStrengthRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the gateway can be pinged routine.
TEST_F(CrosHealthdServiceConnectionTest, RunGatewayCanBePingedRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunGatewayCanBePingedRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the has secure wifi connection routine.
TEST_F(CrosHealthdServiceConnectionTest, RunHasSecureWiFiConnectionRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunHasSecureWiFiConnectionRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the DNS resolver present routine.
TEST_F(CrosHealthdServiceConnectionTest, RunDnsResolverPresentRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunDnsResolverPresentRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the DNS latency routine.
TEST_F(CrosHealthdServiceConnectionTest, RunDnsLatencyRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunDnsLatencyRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the DNS resolution routine.
TEST_F(CrosHealthdServiceConnectionTest, RunDnsResolutionRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunDnsResolutionRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the captive portal routine.
TEST_F(CrosHealthdServiceConnectionTest, RunCaptivePortalRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunCaptivePortalRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the HTTP firewall routine.
TEST_F(CrosHealthdServiceConnectionTest, RunHttpFirewallRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunHttpFirewallRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the HTTPS firewall routine.
TEST_F(CrosHealthdServiceConnectionTest, RunHttpsFirewallRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunHttpsFirewallRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the HTTPS latency routine.
TEST_F(CrosHealthdServiceConnectionTest, RunHttpsLatencyRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunHttpsLatencyRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the video conferencing routine.
TEST_F(CrosHealthdServiceConnectionTest, RunVideoConferencingRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunVideoConferencingRoutine(
      /*stun_server_hostname=*/absl::nullopt,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the ARC HTTP routine.
TEST_F(CrosHealthdServiceConnectionTest, RunArcHttpRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunArcHttpRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the ARC PING routine.
TEST_F(CrosHealthdServiceConnectionTest, RunArcPingRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunArcPingRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can run the ARC DNS resolution routine.
TEST_F(CrosHealthdServiceConnectionTest, RunArcDnsResolutionRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunArcDnsResolutionRoutine(
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can add a Bluetooth observer.
TEST_F(CrosHealthdServiceConnectionTest, AddBluetoothObserver) {
  MockCrosHealthdBluetoothObserver observer;
  ServiceConnection::GetInstance()->AddBluetoothObserver(
      observer.pending_remote());

  // Send out an event to verify the observer is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAdapterAdded()).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));
  FakeCrosHealthd::Get()->EmitAdapterAddedEventForTesting();

  run_loop.Run();
}

// Test that we can add a lid observer.
TEST_F(CrosHealthdServiceConnectionTest, AddLidObserver) {
  MockCrosHealthdLidObserver observer;
  ServiceConnection::GetInstance()->AddLidObserver(observer.pending_remote());

  // Send out an event to make sure the observer is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLidClosed()).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));
  FakeCrosHealthd::Get()->EmitLidClosedEventForTesting();

  run_loop.Run();
}

// Test that we can add a audio observer.
TEST_F(CrosHealthdServiceConnectionTest, AddAudioObserver) {
  MockCrosHealthdAudioObserver observer;
  ServiceConnection::GetInstance()->AddAudioObserver(observer.pending_remote());

  // Send out an event to make sure the observer is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnUnderrun()).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));
  FakeCrosHealthd::Get()->EmitAudioUnderrunEventForTesting();

  run_loop.Run();
}

// Test that we can add a Thunderbolt observer.
TEST_F(CrosHealthdServiceConnectionTest, AddThunderboltObserver) {
  MockCrosHealthdThunderboltObserver observer;
  ServiceConnection::GetInstance()->AddThunderboltObserver(
      observer.pending_remote());

  // Send out an event to make sure the observer is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAdd()).WillOnce(Invoke([&]() { run_loop.Quit(); }));
  FakeCrosHealthd::Get()->EmitThunderboltAddEventForTesting();

  run_loop.Run();
}

// Test that we can add a USB observer.
TEST_F(CrosHealthdServiceConnectionTest, AddUsbObserver) {
  MockCrosHealthdUsbObserver observer;
  ServiceConnection::GetInstance()->AddUsbObserver(observer.pending_remote());

  // Send out an event to make sure the observer is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAdd(_)).WillOnce(Invoke([&]() { run_loop.Quit(); }));
  FakeCrosHealthd::Get()->EmitUsbAddEventForTesting();

  run_loop.Run();
}

// Test that we can add a power observer.
TEST_F(CrosHealthdServiceConnectionTest, AddPowerObserver) {
  MockCrosHealthdPowerObserver observer;
  ServiceConnection::GetInstance()->AddPowerObserver(observer.pending_remote());

  // Send out an event to make sure the observer is connected.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAcInserted()).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));
  FakeCrosHealthd::Get()->EmitAcInsertedEventForTesting();

  run_loop.Run();
}

// Test that we can add a network observer.
TEST_F(CrosHealthdServiceConnectionTest, AddNetworkObserver) {
  MockCrosHealthdNetworkObserver observer;
  ServiceConnection::GetInstance()->AddNetworkObserver(
      observer.pending_remote());

  // Send out an event to make sure the observer is connected.
  base::RunLoop run_loop;
  std::string network_guid = "1234";
  auto network_connection_state = NetworkState::kOnline;
  EXPECT_CALL(observer, OnConnectionStateChanged(_, _))
      .WillOnce(Invoke([&](const std::string& guid, NetworkState state) {
        EXPECT_EQ(guid, network_guid);
        EXPECT_EQ(state, network_connection_state);
        run_loop.Quit();
      }));
  FakeCrosHealthd::Get()->EmitConnectionStateChangedEventForTesting(
      network_guid, network_connection_state);

  run_loop.Run();
}

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

// Test that we can probe telemetry info.
TEST_F(CrosHealthdServiceConnectionTest, ProbeTelemetryInfo) {
  auto response = mojom::TelemetryInfo::New();
  FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      {}, base::BindLambdaForTesting([&](mojom::TelemetryInfoPtr info) {
        EXPECT_EQ(info, response);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test that we can request process info.
TEST_F(CrosHealthdServiceConnectionTest, ProbeProcessInfo) {
  auto response =
      mojom::ProcessResult::NewProcessInfo(mojom::ProcessInfo::New());
  FakeCrosHealthd::Get()->SetProbeProcessInfoResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->ProbeProcessInfo(
      /*process_id=*/13,
      base::BindLambdaForTesting([&](mojom::ProcessResultPtr result) {
        EXPECT_EQ(result, response);
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
