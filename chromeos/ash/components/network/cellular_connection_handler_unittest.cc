// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_connection_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

const char kTestCellularDevicePath[] = "cellular_path";
const char kTestCellularDeviceName[] = "cellular_name";

const char kTestBaseProfilePath[] = "profile_path_";
const char kTestBaseServicePath[] = "service_path_";
const char kTestBaseGuid[] = "guid_";
const char kTestBaseName[] = "name_";

const char kTestBaseIccid[] = "1234567890123456789";
const char kTestEuiccBasePath[] = "/org/chromium/Hermes/Euicc/";
const char kTestBaseEid[] = "12345678901234567890123456789012";

std::string CreateTestServicePath(int profile_num) {
  return base::StringPrintf("%s%d", kTestBaseServicePath, profile_num);
}

std::string CreateTestProfilePath(int profile_num) {
  return base::StringPrintf("%s%d", kTestBaseProfilePath, profile_num);
}

std::string CreateTestGuid(int profile_num) {
  return base::StringPrintf("%s%d", kTestBaseGuid, profile_num);
}

std::string CreateTestName(int profile_num) {
  return base::StringPrintf("%s%d", kTestBaseName, profile_num);
}

std::string CreateTestIccid(int profile_num) {
  return base::StringPrintf("%s%d", kTestBaseIccid, profile_num);
}

std::string CreateTestEuiccPath(int euicc_num) {
  return base::StringPrintf("%s%d", kTestEuiccBasePath, euicc_num);
}

std::string CreateTestEid(int euicc_num) {
  return base::StringPrintf("%s%d", kTestBaseEid, euicc_num);
}

}  // namespace

class CellularConnectionHandlerTest : public testing::Test {
 public:
  CellularConnectionHandlerTest(const CellularConnectionHandlerTest&) = delete;
  CellularConnectionHandlerTest& operator=(
      const CellularConnectionHandlerTest&) = delete;

 protected:
  explicit CellularConnectionHandlerTest()
      : helper_(/*use_default_devices_and_services=*/false) {}
  ~CellularConnectionHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    helper_.network_state_handler()->set_stub_cellular_networks_provider(
        &fake_stubs_provider_);
    inhibitor_.Init(helper_.network_state_handler(),
                    helper_.network_device_handler());
    profile_handler_.Init(helper_.network_state_handler(), &inhibitor_);
    handler_.Init(helper_.network_state_handler(), &inhibitor_,
                  &profile_handler_);
  }

  void CallPrepareExistingCellularNetworkForConnection(int profile_num) {
    handler_.PrepareExistingCellularNetworkForConnection(
        CreateTestIccid(profile_num),
        base::BindOnce(&CellularConnectionHandlerTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&CellularConnectionHandlerTest::OnFailure,
                       base::Unretained(this)));
  }

  void CallPrepareNewlyInstalledCellularNetworkForConnection(int profile_num,
                                                             int euicc_num) {
    handler_.PrepareNewlyInstalledCellularNetworkForConnection(
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)),
        dbus::ObjectPath(CreateTestProfilePath(profile_num)), InhibitCellular(),
        base::BindOnce(&CellularConnectionHandlerTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&CellularConnectionHandlerTest::OnFailure,
                       base::Unretained(this)));
  }

  void AddCellularService(int profile_num) {
    helper_.service_test()->AddService(
        CreateTestServicePath(profile_num), CreateTestGuid(profile_num),
        CreateTestName(profile_num), shill::kTypeCellular, shill::kStateIdle,
        /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularServiceConnected(int profile_num) {
    helper_.service_test()->SetServiceProperty(
        CreateTestServicePath(profile_num), shill::kStateProperty,
        base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectServiceConnectable(int profile_num) {
    const NetworkState* network_state =
        helper_.network_state_handler()->GetNetworkState(
            CreateTestServicePath(profile_num));
    EXPECT_TRUE(network_state->connectable());
  }

  void SetServiceConnectable(int profile_num) {
    helper_.service_test()->SetServiceProperty(
        CreateTestServicePath(profile_num), shill::kConnectableProperty,
        base::Value(true));
    base::RunLoop().RunUntilIdle();
  }

  void SetServiceEid(int profile_num, int euicc_num) {
    helper_.service_test()->SetServiceProperty(
        CreateTestServicePath(profile_num), shill::kEidProperty,
        base::Value(CreateTestEid(euicc_num)));
    base::RunLoop().RunUntilIdle();
  }

  void SetServiceIccid(int profile_num) {
    helper_.service_test()->SetServiceProperty(
        CreateTestServicePath(profile_num), shill::kIccidProperty,
        base::Value(CreateTestIccid(profile_num)));
    base::RunLoop().RunUntilIdle();
  }

  void AddCellularDevice() {
    helper_.device_test()->AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular, kTestCellularDeviceName);
    base::RunLoop().RunUntilIdle();
  }

  void QueueEuiccErrorStatus() {
    helper_.hermes_euicc_test()->QueueHermesErrorStatus(
        HermesResponseStatus::kErrorUnknown);
  }

  void AddEuicc(int euicc_num) {
    helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)),
        CreateTestEid(euicc_num), /*is_active=*/true, /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();
  }

  void AddProfile(int profile_num,
                  int euicc_num,
                  bool add_service = true,
                  bool already_enabled = false) {
    auto add_profile_behavior =
        add_service
            ? HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
                  kAddProfileWithService
            : HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
                  kAddDelayedProfileWithoutService;
    auto state = already_enabled ? hermes::profile::State::kActive
                                 : hermes::profile::State::kInactive;
    std::string iccid = CreateTestIccid(profile_num);

    helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(CreateTestProfilePath(profile_num)),
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)), iccid,
        CreateTestName(profile_num), "nickname", "service_provider",
        "activation_code", CreateTestServicePath(profile_num), state,
        hermes::profile::ProfileClass::kOperational, add_profile_behavior);

    if (!add_service) {
      fake_stubs_provider_.AddStub(iccid, CreateTestEid(euicc_num));
      helper_.network_state_handler()->SyncStubCellularNetworks();
    }

    base::RunLoop().RunUntilIdle();
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

  void ExpectSuccess(const std::string& expected_service_path,
                     base::RunLoop* run_loop,
                     bool auto_connected) {
    expected_service_path_ = expected_service_path;
    expected_auto_connected_ = auto_connected;
    on_success_callback_ = run_loop->QuitClosure();
  }

  void ExpectFailure(const std::string& expected_service_path,
                     const std::string& expected_error_name,
                     base::RunLoop* run_loop) {
    expected_service_path_ = expected_service_path;
    expected_error_name_ = expected_error_name;
    on_failure_callback_ = run_loop->QuitClosure();
  }

  std::unique_ptr<CellularInhibitor::InhibitLock> InhibitCellular() {
    base::RunLoop run_loop;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    inhibitor_.InhibitCellularScanning(
        CellularInhibitor::InhibitReason::kRemovingProfile,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<CellularInhibitor::InhibitLock>
                    new_inhibit_lock) {
              inhibit_lock = std::move(new_inhibit_lock);
              run_loop.Quit();
            }));
    run_loop.Run();
    return inhibit_lock;
  }

  void SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior behavior) {
    helper_.hermes_profile_test()->SetEnableProfileBehavior(behavior);
  }

  void ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult result,
      int expected_count = 1) {
    histogram_tester_.ExpectBucketCount(
        "Network.Cellular.PrepareCellularConnection.OperationResult", result,
        expected_count);
  }

 private:
  void OnSuccess(const std::string& service_path, bool auto_connected) {
    EXPECT_EQ(expected_service_path_, service_path);
    EXPECT_EQ(expected_auto_connected_, auto_connected);
    std::move(on_success_callback_).Run();
  }

  void OnFailure(const std::string& service_path,
                 const std::string& error_name) {
    EXPECT_EQ(expected_service_path_, service_path);
    EXPECT_EQ(expected_error_name_, error_name);
    std::move(on_failure_callback_).Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  NetworkStateTestHelper helper_;
  FakeStubCellularNetworksProvider fake_stubs_provider_;
  CellularInhibitor inhibitor_;
  TestCellularESimProfileHandler profile_handler_;
  CellularConnectionHandler handler_;

  base::OnceClosure on_success_callback_;
  base::OnceClosure on_failure_callback_;
  std::string expected_service_path_;
  bool expected_auto_connected_;
  std::string expected_error_name_;
};

TEST_F(CellularConnectionHandlerTest, NoService) {
  // Note: No cellular service added.

  base::RunLoop run_loop;
  ExpectFailure(/*service_path=*/std::string(),
                NetworkConnectionHandler::kErrorNotFound, &run_loop);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  run_loop.Run();

  ExpectResult(CellularConnectionHandler::PrepareCellularConnectionResult::
                   kCouldNotFindNetworkWithIccid);
}

TEST_F(CellularConnectionHandlerTest, ServiceAlreadyConnectable) {
  AddCellularDevice();
  AddCellularService(/*profile_num=*/1);
  SetServiceIccid(/*profile_num=*/1);
  SetServiceConnectable(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop,
                /*auto_connected=*/false);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  run_loop.Run();

  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess);
}

TEST_F(CellularConnectionHandlerTest, FailsInhibiting) {
  // Note: No cellular device added. This causes the inhibit operation to fail.

  AddCellularService(/*profile_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectFailure(CreateTestServicePath(/*profile_num=*/1),
                NetworkConnectionHandler::kErrorCellularInhibitFailure,
                &run_loop);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  run_loop.Run();

  ExpectResult(CellularConnectionHandler::PrepareCellularConnectionResult::
                   kInhibitFailed);
}

TEST_F(CellularConnectionHandlerTest, NoRelevantEuicc) {
  AddCellularDevice();
  AddCellularService(/*profile_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectFailure(CreateTestServicePath(/*profile_num=*/1),
                NetworkConnectionHandler::kErrorESimProfileIssue, &run_loop);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  run_loop.Run();

  ExpectResult(CellularConnectionHandler::PrepareCellularConnectionResult::
                   kCouldNotFindRelevantEuicc);
}

TEST_F(CellularConnectionHandlerTest, FailsRequestingInstalledProfiles) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);

  QueueEuiccErrorStatus();

  base::RunLoop run_loop;
  ExpectFailure(CreateTestServicePath(/*profile_num=*/1),
                NetworkConnectionHandler::kErrorESimProfileIssue, &run_loop);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  run_loop.Run();

  ExpectResult(CellularConnectionHandler::PrepareCellularConnectionResult::
                   kRefreshProfilesFailed);
}

TEST_F(CellularConnectionHandlerTest, TimeoutWaitingForConnectable_ESim) {
  const base::TimeDelta kWaitingForConnectableTimeout = base::Seconds(30);

  SetEnableProfileBehavior(HermesProfileClient::TestInterface::
                               EnableProfileBehavior::kNotConnectable);

  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectFailure(CreateTestServicePath(/*profile_num=*/1),
                NetworkConnectionHandler::kConnectableCellularTimeout,
                &run_loop);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);

  // Let all operations run, then wait for the timeout to occur.
  base::RunLoop().RunUntilIdle();
  AdvanceClock(kWaitingForConnectableTimeout);

  run_loop.Run();

  ExpectResult(CellularConnectionHandler::PrepareCellularConnectionResult::
                   kTimeoutWaitingForConnectable);
}

TEST_F(CellularConnectionHandlerTest, TimeoutWaitingForConnectable_PSim) {
  const base::TimeDelta kWaitingForConnectableTimeout = base::Seconds(30);

  SetEnableProfileBehavior(HermesProfileClient::TestInterface::
                               EnableProfileBehavior::kNotConnectable);

  AddCellularDevice();
  AddCellularService(/*profile_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectFailure(CreateTestServicePath(/*profile_num=*/1),
                NetworkConnectionHandler::kConnectableCellularTimeout,
                &run_loop);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);

  // Let all operations run, then wait for the timeout to occur.
  base::RunLoop().RunUntilIdle();
  AdvanceClock(kWaitingForConnectableTimeout);

  run_loop.Run();

  ExpectResult(CellularConnectionHandler::PrepareCellularConnectionResult::
                   kTimeoutWaitingForConnectable);
}

TEST_F(CellularConnectionHandlerTest, Success_AutoConnected) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop,
                /*auto_connected=*/true);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  // Simulate the cellular network get connected after 10 seconds.
  AdvanceClock(base::Seconds(10));
  SetCellularServiceConnected(/*profile_num=*/1);
  run_loop.Run();

  ExpectServiceConnectable(/*profile_num=*/1);
  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess);
}

TEST_F(CellularConnectionHandlerTest, Success_TimeoutAutoConnected) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop,
                /*auto_connected=*/false);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  run_loop.Run();

  ExpectServiceConnectable(/*profile_num=*/1);
  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess);
}

TEST_F(CellularConnectionHandlerTest, Success_AlreadyEnabled) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1,
             /*euicc_num=*/1,
             /*add_service=*/true,
             /*already_enabled=*/true);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop,
                /*auto_connected=*/false);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  SetServiceConnectable(/*profile_num=*/1);
  run_loop.Run();

  ExpectServiceConnectable(/*profile_num=*/1);
  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess);
}

TEST_F(CellularConnectionHandlerTest, ConnectToStub) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  // Do not add a service; instead, this will cause a fake stub network to be
  // created.
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1, /*add_service=*/false);

  base::RunLoop run_loop;
  // Expect that by the end, we will connect to a "real" (i.e., non-stub)
  // service path.
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop,
                /*auto_connected=*/false);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  base::RunLoop().RunUntilIdle();

  // A connection has started to a stub. Because the profile gets enabled,
  // Shill exposes a service and makes it connectable.
  AddCellularService(/*profile_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);
  SetServiceConnectable(/*profile_num=*/1);

  run_loop.Run();
  ExpectServiceConnectable(/*profile_num=*/1);
  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess);
}

TEST_F(CellularConnectionHandlerTest, MultipleRequests) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  AddProfile(/*profile_num=*/2, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/2, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);
  SetServiceIccid(/*profile_num=*/2);

  base::RunLoop run_loop1;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop1,
                /*auto_connected=*/false);

  // Start both operations.
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/1);
  CallPrepareExistingCellularNetworkForConnection(/*profile_num=*/2);

  // Verify that the first service becomes connectable.
  run_loop1.Run();
  ExpectServiceConnectable(/*profile_num=*/1);

  base::RunLoop run_loop2;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/2), &run_loop2,
                /*auto_connected=*/false);

  // Verify that the second service becomes connectable.
  run_loop2.Run();
  ExpectServiceConnectable(/*profile_num=*/2);

  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess,
      /*expected_count=*/2);
}

TEST_F(CellularConnectionHandlerTest, NewProfile) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(CreateTestServicePath(/*profile_num=*/1), &run_loop,
                /*auto_connected=*/false);
  CallPrepareNewlyInstalledCellularNetworkForConnection(/*profile_num=*/1,
                                                        /*euicc_num=*/1);

  // Verify that service corresponding to new profile becomes
  // connectable.
  run_loop.Run();
  ExpectServiceConnectable(/*profile_num=*/1);
  ExpectResult(
      CellularConnectionHandler::PrepareCellularConnectionResult::kSuccess);
}

}  // namespace ash
