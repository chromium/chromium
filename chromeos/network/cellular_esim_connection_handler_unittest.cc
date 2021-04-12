// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_connection_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
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

class CellularESimConnectionHandlerTest : public testing::Test {
 protected:
  CellularESimConnectionHandlerTest()
      : helper_(/*use_default_devices_and_services=*/false) {}
  ~CellularESimConnectionHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    inhibitor_.Init(helper_.network_state_handler(),
                    helper_.network_device_handler());
    profile_handler_.Init(helper_.network_state_handler(), &inhibitor_);
    handler_.Init(helper_.network_state_handler(), &inhibitor_,
                  &profile_handler_);
  }

  void StartEnableProfileForConnection(int profile_num) {
    handler_.EnableProfileForConnection(
        CreateTestServicePath(profile_num),
        base::BindOnce(&CellularESimConnectionHandlerTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&CellularESimConnectionHandlerTest::OnFailure,
                       base::Unretained(this)));
  }

  void StartEnableNewProfileForConnection(
      int profile_num,
      int euicc_num,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
    handler_.EnableNewProfileForConnection(
        dbus::ObjectPath(CreateTestEuiccPath(profile_num)),
        dbus::ObjectPath(CreateTestProfilePath(profile_num)),
        std::move(inhibit_lock),
        base::BindOnce(&CellularESimConnectionHandlerTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&CellularESimConnectionHandlerTest::OnFailure,
                       base::Unretained(this)));
  }

  void AddCellularService(int profile_num) {
    helper_.service_test()->AddService(
        CreateTestServicePath(profile_num), CreateTestGuid(profile_num),
        CreateTestName(profile_num), shill::kTypeCellular, shill::kStateIdle,
        /*visible=*/true);
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

  void AddProfile(int profile_num, int euicc_num) {
    helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(CreateTestProfilePath(profile_num)),
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)),
        CreateTestIccid(profile_num), CreateTestName(profile_num),
        "service_provider", "activation_code",
        CreateTestServicePath(profile_num), hermes::profile::State::kInactive,
        hermes::profile::ProfileClass::kOperational, /*service_only=*/false);
    base::RunLoop().RunUntilIdle();
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

  void ExpectSuccess(base::RunLoop* run_loop) {
    on_success_callback_ = run_loop->QuitClosure();
  }

  void ExpectFailure(const std::string& expected_error_name,
                     base::RunLoop* run_loop) {
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

 private:
  void OnSuccess(const std::string& service_only) {
    std::move(on_success_callback_).Run();
  }

  void OnFailure(const std::string& error_name,
                 std::unique_ptr<base::DictionaryValue> error_data) {
    EXPECT_EQ(expected_error_name_, error_name);
    std::move(on_failure_callback_).Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper helper_;
  CellularInhibitor inhibitor_;
  TestCellularESimProfileHandler profile_handler_;
  CellularESimConnectionHandler handler_;

  base::OnceClosure on_success_callback_;
  base::OnceClosure on_failure_callback_;
  std::string expected_error_name_;
};

TEST_F(CellularESimConnectionHandlerTest, NoService) {
  // Note: No cellular service added.

  base::RunLoop run_loop;
  ExpectFailure(NetworkConnectionHandler::kErrorNotFound, &run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  run_loop.Run();
}

TEST_F(CellularESimConnectionHandlerTest, ServiceAlreadyConnectable) {
  AddCellularDevice();
  AddCellularService(/*profile_num=*/1);
  SetServiceConnectable(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(&run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  run_loop.Run();
}

TEST_F(CellularESimConnectionHandlerTest, FailsInhibiting) {
  // Note: No cellular device added. This causes the inhibit operation to fail.

  AddCellularService(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectFailure(NetworkConnectionHandler::kErrorCellularInhibitFailure,
                &run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  run_loop.Run();
}

TEST_F(CellularESimConnectionHandlerTest, NoRelevantEuicc) {
  AddCellularDevice();
  AddCellularService(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectFailure(NetworkConnectionHandler::kErrorESimProfileIssue, &run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  run_loop.Run();
}

TEST_F(CellularESimConnectionHandlerTest, FailsRequestingInstalledProfiles) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);

  QueueEuiccErrorStatus();

  base::RunLoop run_loop;
  ExpectFailure(NetworkConnectionHandler::kErrorESimProfileIssue, &run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  run_loop.Run();
}

TEST_F(CellularESimConnectionHandlerTest, TimeoutWaitingForConnectable) {
  const base::TimeDelta kWaitingForConnectableTimeout =
      base::TimeDelta::FromSeconds(30);

  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(&run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  ExpectFailure(NetworkConnectionHandler::kErrorESimProfileIssue, &run_loop);

  // Let all operations run, then wait for the timeout to occur.
  base::RunLoop().RunUntilIdle();
  AdvanceClock(kWaitingForConnectableTimeout);

  run_loop.Run();
}

TEST_F(CellularESimConnectionHandlerTest, Success) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(&run_loop);
  StartEnableProfileForConnection(/*profile_num=*/1);
  run_loop.Run();
  ExpectServiceConnectable(/*profile_num=*/1);
}

TEST_F(CellularESimConnectionHandlerTest, MultipleRequests) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);
  AddProfile(/*profile_num=*/2, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/1, /*euicc_num=*/1);
  SetServiceEid(/*profile_num=*/2, /*euicc_num=*/1);
  SetServiceIccid(/*profile_num=*/1);
  SetServiceIccid(/*profile_num=*/2);

  base::RunLoop run_loop1;
  ExpectSuccess(&run_loop1);

  // Start both operations.
  StartEnableProfileForConnection(/*profile_num=*/1);
  StartEnableProfileForConnection(/*profile_num=*/2);

  // Verify that the first service becomes connectable.
  run_loop1.Run();
  ExpectServiceConnectable(/*profile_num=*/1);

  base::RunLoop run_loop2;
  ExpectSuccess(&run_loop2);

  // Verify that the second service becomes connectable.
  run_loop2.Run();
  ExpectServiceConnectable(/*profile_num=*/2);
}

TEST_F(CellularESimConnectionHandlerTest, NewProfileTest) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*profile_num=*/1, /*euicc_num=*/1);

  base::RunLoop run_loop;
  ExpectSuccess(&run_loop);
  std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock =
      InhibitCellular();
  StartEnableNewProfileForConnection(/*profile_num=*/1, /*euicc_num=*/1,
                                     std::move(inhibit_lock));

  // Verify that service corresponding to new profile becomes
  // connectable.
  run_loop.Run();
  ExpectServiceConnectable(/*profile_num=*/1);
}

}  // namespace chromeos
