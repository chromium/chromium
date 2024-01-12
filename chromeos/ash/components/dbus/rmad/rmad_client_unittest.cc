// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rmad/rmad_client.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

namespace {

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

class RmadClientTest : public testing::Test {
 public:
  RmadClientTest() = default;
  RmadClientTest(const RmadClientTest&) = delete;
  RmadClientTest& operator=(const RmadClientTest&) = delete;

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock rmad daemon proxy.
    mock_proxy_ =
        new dbus::MockObjectProxy(mock_bus_.get(), rmad::kRmadInterfaceName,
                                  dbus::ObjectPath(rmad::kRmadServicePath));

    // |client_|'s Init() method should request a proxy for communicating with
    // the rmad daemon.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(rmad::kRmadInterfaceName,
                               dbus::ObjectPath(rmad::kRmadServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    // Save |client_|'s signal callbacks.
    EXPECT_CALL(*mock_proxy_,
                DoConnectToSignal(rmad::kRmadInterfaceName, _, _, _))
        .WillRepeatedly(Invoke(this, &RmadClientTest::ConnectToSignal));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    RmadClient::Initialize(mock_bus_.get());
    client_ = RmadClient::Get();
  }

  void TearDown() override {
    mock_bus_->ShutdownAndBlock();
    RmadClient::Shutdown();
  }

  // Responsible for responding to a rmad API method call.
  void OnCallDbusMethod(dbus::MethodCall* method_call,
                        int timeout_ms,
                        dbus::ObjectProxy::ResponseCallback* callback) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  // Synchronously passes |signal| to |client_|'s handler, simulating the signal
  // being emitted by rmad.
  void EmitSignal(dbus::Signal* signal) {
    const std::string signal_name = signal->GetMember();
    const auto it = signal_callbacks_.find(signal_name);
    ASSERT_TRUE(it != signal_callbacks_.end())
        << "Client didn't register for signal " << signal_name;
    it->second.Run(signal);
  }

  // Used to trigger errors on signals with parameters.
  void EmitEmptySignal(const std::string& signal_name) {
    dbus::Signal signal(rmad::kRmadInterfaceName, signal_name);
    EmitSignal(&signal);
  }

  // Passes an error signal to |client_|.
  void EmitErrorSignal(rmad::RmadErrorCode error) {
    dbus::Signal signal(rmad::kRmadInterfaceName, rmad::kErrorSignal);
    dbus::MessageWriter(&signal).AppendInt32(static_cast<int32_t>(error));
    EmitSignal(&signal);
  }

  // Passes a calibration progress signal to |client_|.
  void EmitCalibrationProgressSignal(
      rmad::RmadComponent component,
      rmad::CalibrationComponentStatus::CalibrationStatus status,
      double progress) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kCalibrationProgressSignal);
    dbus::MessageWriter writer(&signal);
    dbus::MessageWriter struct_writer(nullptr);
    writer.OpenStruct(&struct_writer);
    struct_writer.AppendInt32(static_cast<int32_t>(component));
    struct_writer.AppendInt32(static_cast<int32_t>(status));
    struct_writer.AppendDouble(progress);
    writer.CloseContainer(&struct_writer);
    EmitSignal(&signal);
  }

  // Passes a calibration overall progress signal to |client_|.
  void EmitCalibrationOverallProgressSignal(
      rmad::CalibrationOverallStatus status) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kCalibrationOverallSignal);
    dbus::MessageWriter(&signal).AppendInt32(static_cast<int32_t>(status));
    EmitSignal(&signal);
  }

  // Passes a provisioning progress signal to |client_|.
  void EmitHardwareWriteProtectionStateSignal(bool enabled) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kHardwareWriteProtectionStateSignal);
    dbus::MessageWriter(&signal).AppendBool(enabled);
    EmitSignal(&signal);
  }

  // Passes a power cable state signal to |client_|.
  void EmitPowerCableStateSignal(bool plugged_in) {
    dbus::Signal signal(rmad::kRmadInterfaceName, rmad::kPowerCableStateSignal);
    dbus::MessageWriter(&signal).AppendBool(plugged_in);
    EmitSignal(&signal);
  }

  // Passes an external disk signal to |client_|.
  void EmitExternalDiskStateSignal(bool detected) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kExternalDiskDetectedSignal);
    dbus::MessageWriter(&signal).AppendBool(detected);
    EmitSignal(&signal);
  }

  // Passes a provisioning progress signal to |client_|.
  void EmitProvisioningProgressSignal(rmad::ProvisionStatus::Status status,
                                      double progress,
                                      rmad::ProvisionStatus::Error error) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kProvisioningProgressSignal);
    dbus::MessageWriter writer(&signal);
    dbus::MessageWriter struct_writer(nullptr);
    writer.OpenStruct(&struct_writer);
    struct_writer.AppendInt32(static_cast<int32_t>(status));
    struct_writer.AppendDouble(progress);
    struct_writer.AppendInt32(static_cast<int32_t>(error));
    writer.CloseContainer(&struct_writer);
    EmitSignal(&signal);
  }

  // Passes a hardware verification status signal to |client_|.
  void EmitHardwareVerificationResultSignal(bool is_compliant,
                                            std::string error_message) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kHardwareVerificationResultSignal);
    dbus::MessageWriter writer(&signal);
    dbus::MessageWriter struct_writer(nullptr);
    writer.OpenStruct(&struct_writer);
    struct_writer.AppendBool(is_compliant);
    struct_writer.AppendString(error_message);
    writer.CloseContainer(&struct_writer);
    EmitSignal(&signal);
  }

  // Passes a finalization status signal to |client_|.
  void EmitFinalizationProgressSignal(rmad::FinalizeStatus::Status status,
                                      double progress,
                                      rmad::FinalizeStatus::Error error) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kFinalizeProgressSignal);
    dbus::MessageWriter writer(&signal);
    dbus::MessageWriter struct_writer(nullptr);
    writer.OpenStruct(&struct_writer);
    struct_writer.AppendInt32(static_cast<int32_t>(status));
    struct_writer.AppendDouble(progress);
    struct_writer.AppendInt32(static_cast<int32_t>(error));
    writer.CloseContainer(&struct_writer);
    EmitSignal(&signal);
  }

  // Passes a ro firmware update status signal to |client_|.
  void EmitRoFirmwareUpdateProgressSignal(rmad::UpdateRoFirmwareStatus status) {
    dbus::Signal signal(rmad::kRmadInterfaceName,
                        rmad::kUpdateRoFirmwareStatusSignal);
    dbus::MessageWriter(&signal).AppendInt32(static_cast<int32_t>(status));
    EmitSignal(&signal);
  }

 protected:
  // Maps from rmad signal name to the corresponding callback provided by
  // |client_|.
  std::map<std::string, dbus::ObjectProxy::SignalCallback> signal_callbacks_;

  raw_ptr<RmadClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<dbus::Response, DanglingUntriaged> response_ = nullptr;
  // Mock D-Bus objects for |client_| to interact with.
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;

 private:
  // Handles calls to |mock_proxy_|'s ConnectToSignal() method.
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    CHECK_EQ(interface_name, rmad::kRmadInterfaceName);
    signal_callbacks_[signal_name] = signal_callback;

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*on_connected_callback), interface_name,
                       signal_name, true /* success */));
  }
};

// Interface for observing changes from rmad.
class TestObserver : public RmadClient::Observer {
 public:
  explicit TestObserver(RmadClient* client) : client_(client) {
    client_->AddObserver(this);
  }
  ~TestObserver() override { client_->RemoveObserver(this); }

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  int num_error() const { return num_error_; }
  rmad::RmadErrorCode last_error() const { return last_error_; }
  int num_calibration_progress() const { return num_calibration_progress_; }
  const rmad::CalibrationComponentStatus& last_calibration_component_status()
      const {
    return last_calibration_component_status_;
  }
  int num_calibration_overall_progress() const {
    return num_calibration_overall_progress_;
  }
  rmad::CalibrationOverallStatus last_calibration_overall_status() const {
    return last_calibration_overall_status_;
  }
  int num_provisioning_progress() const { return num_provisioning_progress_; }
  rmad::ProvisionStatus last_provisioning_status() const {
    return last_provisioning_status_;
  }
  int num_hardware_write_protection_state() const {
    return num_hardware_write_protection_state_;
  }
  bool last_hardware_write_protection_state() const {
    return last_hardware_write_protection_state_;
  }
  int num_power_cable_state() const { return num_power_cable_state_; }
  bool last_power_cable_state() const { return last_power_cable_state_; }
  int num_external_disk_state() const { return num_external_disk_state_; }
  bool last_external_disk_state() const { return last_external_disk_state_; }
  int num_hardware_verification_result() const {
    return num_hardware_verification_result_;
  }
  const rmad::HardwareVerificationResult& last_hardware_verification_result()
      const {
    return last_hardware_verification_result_;
  }
  int num_finalization_progress() const { return num_finalization_progress_; }
  const rmad::FinalizeStatus& last_finalization_progress() const {
    return last_finalization_progress_;
  }

  int num_ro_firmware_update_progress() const {
    return num_ro_firmware_update_progress_;
  }
  rmad::UpdateRoFirmwareStatus last_ro_firmware_update_status() const {
    return last_ro_firmware_update_status_;
  }

  // Called when an error occurs outside of state transitions.
  // e.g. while calibrating devices.
  void Error(rmad::RmadErrorCode error) override {
    num_error_++;
    last_error_ = error;
  }

  // Called when calibration progress is updated.
  void CalibrationProgress(
      const rmad::CalibrationComponentStatus& componentStatus) override {
    num_calibration_progress_++;
    last_calibration_component_status_ = componentStatus;
  }

  void CalibrationOverallProgress(
      const rmad::CalibrationOverallStatus status) override {
    num_calibration_overall_progress_++;
    last_calibration_overall_status_ = status;
  }

  // Called when provisioning progress is updated.
  void ProvisioningProgress(const rmad::ProvisionStatus& status) override {
    num_provisioning_progress_++;
    last_provisioning_status_ = status;
  }

  // Called when hardware write protection state changes.
  void HardwareWriteProtectionState(bool enabled) override {
    num_hardware_write_protection_state_++;
    last_hardware_write_protection_state_ = enabled;
  }

  // Called when power cable is plugged in or removed.
  void PowerCableState(bool plugged_in) override {
    num_power_cable_state_++;
    last_power_cable_state_ = plugged_in;
  }

  // Called when power cable is plugged in or removed.
  void ExternalDiskState(bool detected) override {
    num_external_disk_state_++;
    last_external_disk_state_ = detected;
  }

  // Called when hardware verification completes.
  void HardwareVerificationResult(
      const rmad::HardwareVerificationResult& result) override {
    num_hardware_verification_result_++;
    last_hardware_verification_result_ = result;
  }

  // Called when hardware verification completes.
  void FinalizationProgress(const rmad::FinalizeStatus& status) override {
    num_finalization_progress_++;
    last_finalization_progress_ = status;
  }

  // Called when overall calibration progress is updated.
  void RoFirmwareUpdateProgress(rmad::UpdateRoFirmwareStatus status) override {
    num_ro_firmware_update_progress_++;
    last_ro_firmware_update_status_ = status;
  }

 private:
  raw_ptr<RmadClient> client_;  // Not owned.
  int num_error_ = 0;
  rmad::RmadErrorCode last_error_ = rmad::RmadErrorCode::RMAD_ERROR_NOT_SET;
  int num_calibration_progress_ = 0;
  rmad::CalibrationComponentStatus last_calibration_component_status_;
  int num_calibration_overall_progress_ = 0;
  rmad::CalibrationOverallStatus last_calibration_overall_status_ =
      rmad::CalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_UNKNOWN;
  int num_provisioning_progress_ = 0;
  rmad::ProvisionStatus last_provisioning_status_;
  int num_hardware_write_protection_state_ = 0;
  bool last_hardware_write_protection_state_ = true;
  int num_power_cable_state_ = 0;
  bool last_power_cable_state_ = true;
  int num_external_disk_state_ = 0;
  bool last_external_disk_state_ = true;
  int num_hardware_verification_result_ = 0;
  rmad::HardwareVerificationResult last_hardware_verification_result_;
  int num_finalization_progress_ = 0;
  rmad::FinalizeStatus last_finalization_progress_;
  int num_ro_firmware_update_progress_ = 0;
  rmad::UpdateRoFirmwareStatus last_ro_firmware_update_status_ =
      rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_UNKNOWN;
};

TEST_F(RmadClientTest, GetCurrentState) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetStateReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetCurrentStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
        EXPECT_FALSE(response->has_state());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetCurrentState_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetCurrentStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetCurrentState_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetCurrentStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionNextState) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetStateReply expected_proto;
  rmad::RmadState* expected_state = new rmad::RmadState();
  expected_state->set_allocated_device_destination(
      new rmad::DeviceDestinationState());
  expected_proto.set_allocated_state(expected_state);
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionNextStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  rmad::RmadState request;
  request.set_allocated_welcome(new rmad::WelcomeState());

  base::RunLoop run_loop;
  client_->TransitionNextState(
      request, base::BindLambdaForTesting(
                   [&](std::optional<rmad::GetStateReply> response) {
                     EXPECT_TRUE(response.has_value());
                     EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
                     EXPECT_TRUE(response->has_state());
                     EXPECT_TRUE(response->state().has_device_destination());
                     run_loop.Quit();
                   }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionNextState_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionNextStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  rmad::RmadState request;
  request.set_allocated_welcome(new rmad::WelcomeState());
  base::RunLoop run_loop;
  client_->TransitionNextState(
      request, base::BindLambdaForTesting(
                   [&](std::optional<rmad::GetStateReply> response) {
                     EXPECT_FALSE(response.has_value());
                     run_loop.Quit();
                   }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionNextState_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionNextStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  rmad::RmadState request;
  request.set_allocated_welcome(new rmad::WelcomeState());
  base::RunLoop run_loop;
  client_->TransitionNextState(
      request, base::BindLambdaForTesting(
                   [&](std::optional<rmad::GetStateReply> response) {
                     EXPECT_FALSE(response.has_value());
                     run_loop.Quit();
                   }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionPreviousState) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetStateReply expected_proto;
  rmad::RmadState* expected_state = new rmad::RmadState();
  expected_state->set_allocated_welcome(new rmad::WelcomeState());
  expected_proto.set_allocated_state(expected_state);
  expected_proto.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionPreviousStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_TRANSITION_FAILED);
        EXPECT_TRUE(response->has_state());
        EXPECT_TRUE(response->state().has_welcome());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionPreviousState_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionPreviousStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionPreviousState_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionPreviousStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, AbortRma) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::AbortRmaReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kAbortRmaMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](std::optional<rmad::AbortRmaReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, AbortRma_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kAbortRmaMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](std::optional<rmad::AbortRmaReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, AbortRma_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kAbortRmaMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](std::optional<rmad::AbortRmaReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetLog) {
  const std::string expected_log = "test rma log";
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetLogReply expected_proto;
  expected_proto.set_log(expected_log);
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetLogMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetLog(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetLogReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->log(), expected_log);
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetLog_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetLogMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetLog(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetLogReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetLog_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetLogMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetLog(base::BindLambdaForTesting(
      [&](std::optional<rmad::GetLogReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, SaveLog) {
  const std::string expected_save_path = "test save path";
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::SaveLogReply expected_proto;
  expected_proto.set_save_path(expected_save_path);
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kSaveLogMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->SaveLog("Diagnostics log text",
                   base::BindLambdaForTesting(
                       [&](std::optional<rmad::SaveLogReply> response) {
                         EXPECT_TRUE(response.has_value());
                         EXPECT_EQ(response->save_path(), expected_save_path);
                         EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
                         run_loop.Quit();
                       }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, SaveLog_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kSaveLogMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->SaveLog("Diagnostics log text",
                   base::BindLambdaForTesting(
                       [&](std::optional<rmad::SaveLogReply> response) {
                         EXPECT_FALSE(response.has_value());
                         run_loop.Quit();
                       }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, SaveLog_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kSaveLogMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->SaveLog("Diagnostics log text",
                   base::BindLambdaForTesting(
                       [&](std::optional<rmad::SaveLogReply> response) {
                         EXPECT_FALSE(response.has_value());
                         run_loop.Quit();
                       }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, RecordBrowserActionMetric) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  rmad::RecordBrowserActionMetricRequest request;
  request.set_diagnostics(true);
  request.set_os_update(false);
  rmad::RecordBrowserActionMetricReply expected_reply;
  expected_reply.set_error(rmad::RMAD_ERROR_OK);

  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_reply));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kRecordBrowserActionMetricMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->RecordBrowserActionMetric(
      request,
      base::BindLambdaForTesting(
          [&](std::optional<rmad::RecordBrowserActionMetricReply> response) {
            EXPECT_TRUE(response.has_value());
            EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, RecordBrowserActionMetric_NullResponse) {
  response_ = nullptr;

  rmad::RecordBrowserActionMetricRequest request;
  request.set_diagnostics(true);
  request.set_os_update(false);

  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kRecordBrowserActionMetricMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->RecordBrowserActionMetric(
      request,
      base::BindLambdaForTesting(
          [&](std::optional<rmad::RecordBrowserActionMetricReply> response) {
            EXPECT_FALSE(response.has_value());
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, RecordBrowserActionMetric_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  rmad::RecordBrowserActionMetricRequest request;
  request.set_diagnostics(true);
  request.set_os_update(false);

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kRecordBrowserActionMetricMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->RecordBrowserActionMetric(
      request,
      base::BindLambdaForTesting(
          [&](std::optional<rmad::RecordBrowserActionMetricReply> response) {
            EXPECT_FALSE(response.has_value());
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, ExtractExternalDiagnosticsApp) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::ExtractExternalDiagnosticsAppReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  expected_proto.set_diagnostics_app_swbn_path("swbn_path");
  expected_proto.set_diagnostics_app_crx_path("crx_path");
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(
      *mock_proxy_.get(),
      DoCallMethod(HasMember(rmad::kExtractExternalDiagnosticsAppMethod),
                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::test::TestFuture<
      std::optional<rmad::ExtractExternalDiagnosticsAppReply>>
      future;
  client_->ExtractExternalDiagnosticsApp(future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->error(), expected_proto.error());
  EXPECT_EQ(future.Get()->diagnostics_app_swbn_path(),
            expected_proto.diagnostics_app_swbn_path());
  EXPECT_EQ(future.Get()->diagnostics_app_crx_path(),
            expected_proto.diagnostics_app_crx_path());
}

TEST_F(RmadClientTest, InstallExtractedDiagnosticsApp) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::InstallExtractedDiagnosticsAppReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(
      *mock_proxy_.get(),
      DoCallMethod(HasMember(rmad::kInstallExtractedDiagnosticsAppMethod),
                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::test::TestFuture<
      std::optional<rmad::InstallExtractedDiagnosticsAppReply>>
      future;
  client_->InstallExtractedDiagnosticsApp(future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->error(), expected_proto.error());
}

TEST_F(RmadClientTest, GetInstalledDiagnosticsApp) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetInstalledDiagnosticsAppReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  expected_proto.set_diagnostics_app_swbn_path("swbn_path");
  expected_proto.set_diagnostics_app_crx_path("crx_path");
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetInstalledDiagnosticsAppMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::test::TestFuture<std::optional<rmad::GetInstalledDiagnosticsAppReply>>
      future;
  client_->GetInstalledDiagnosticsApp(future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->error(), expected_proto.error());
  EXPECT_EQ(future.Get()->diagnostics_app_swbn_path(),
            expected_proto.diagnostics_app_swbn_path());
  EXPECT_EQ(future.Get()->diagnostics_app_crx_path(),
            expected_proto.diagnostics_app_crx_path());
}

// Tests that synchronous observers are notified about errors that occur outside
// of state transitions.
TEST_F(RmadClientTest, Error) {
  TestObserver observer_1(client_);

  EmitErrorSignal(rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
  EXPECT_EQ(observer_1.num_error(), 1);
  EXPECT_EQ(observer_1.last_error(),
            rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
}

// Tests that synchronous observers are notified about component calibration
// progress.
TEST_F(RmadClientTest, CalibrationProgress) {
  TestObserver observer_1(client_);

  EmitCalibrationProgressSignal(
      rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER,
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS, 0.5);
  EXPECT_EQ(observer_1.num_calibration_progress(), 1);
  EXPECT_EQ(observer_1.last_calibration_component_status().component(),
            rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER);
  EXPECT_EQ(observer_1.last_calibration_component_status().status(),
            rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS);
  EXPECT_EQ(observer_1.last_calibration_component_status().progress(), 0.5);
}

TEST_F(RmadClientTest, CalibrationOverallProgress) {
  TestObserver observer_1(client_);

  EmitCalibrationOverallProgressSignal(
      rmad::CalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
  EXPECT_EQ(observer_1.num_calibration_overall_progress(), 1);
  EXPECT_EQ(observer_1.last_calibration_overall_status(),
            rmad::CalibrationOverallStatus::
                RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
}

TEST_F(RmadClientTest, CalibrationOverallProgressBadParameterFails) {
  TestObserver observer_1(client_);

  EmitEmptySignal(rmad::kCalibrationOverallSignal);
  EXPECT_EQ(observer_1.num_calibration_overall_progress(), 0);
  EXPECT_EQ(observer_1.last_calibration_overall_status(),
            rmad::CalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_UNKNOWN);
}

// Tests that synchronous observers are notified about provisioning progress.
TEST_F(RmadClientTest, ProvisioningProgress) {
  TestObserver observer_1(client_);

  EmitProvisioningProgressSignal(
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS, 0.25,
      rmad::ProvisionStatus::RMAD_PROVISION_ERROR_UNKNOWN);
  EXPECT_EQ(observer_1.num_provisioning_progress(), 1);
  EXPECT_EQ(observer_1.last_provisioning_status().status(),
            rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS);
  EXPECT_EQ(observer_1.last_provisioning_status().progress(), 0.25);
  EXPECT_EQ(observer_1.last_provisioning_status().error(),
            rmad::ProvisionStatus::RMAD_PROVISION_ERROR_UNKNOWN);
}

// Tests that synchronous observers are notified about provisioning errors.
TEST_F(RmadClientTest, ProvisioningErrors) {
  TestObserver observer_1(client_);

  EmitProvisioningProgressSignal(
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING, 0.25,
      rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CR50);
  EXPECT_EQ(observer_1.num_provisioning_progress(), 1);
  EXPECT_EQ(observer_1.last_provisioning_status().status(),
            rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING);
  EXPECT_EQ(observer_1.last_provisioning_status().progress(), 0.25);
  EXPECT_EQ(observer_1.last_provisioning_status().error(),
            rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CR50);
}

// Tests that synchronous observers are notified about provisioning progress.
TEST_F(RmadClientTest, HardwareWriteProtectionState) {
  TestObserver observer_1(client_);

  EmitHardwareWriteProtectionStateSignal(false);
  EXPECT_EQ(observer_1.num_hardware_write_protection_state(), 1);
  EXPECT_FALSE(observer_1.last_hardware_write_protection_state());

  EmitHardwareWriteProtectionStateSignal(true);
  EXPECT_EQ(observer_1.num_hardware_write_protection_state(), 2);
  EXPECT_TRUE(observer_1.last_hardware_write_protection_state());
}

// Tests that synchronous observers are notified about power cable state.
TEST_F(RmadClientTest, PowerCableState) {
  TestObserver observer_1(client_);

  EmitPowerCableStateSignal(false);
  EXPECT_EQ(observer_1.num_power_cable_state(), 1);
  EXPECT_FALSE(observer_1.last_power_cable_state());

  EmitPowerCableStateSignal(true);
  EXPECT_EQ(observer_1.num_power_cable_state(), 2);
  EXPECT_TRUE(observer_1.last_power_cable_state());
}

// Tests that synchronous observers are notified about external disk state.
TEST_F(RmadClientTest, ExternalDiskState) {
  TestObserver observer_1(client_);

  EmitExternalDiskStateSignal(false);
  EXPECT_EQ(observer_1.num_external_disk_state(), 1);
  EXPECT_FALSE(observer_1.last_external_disk_state());

  EmitExternalDiskStateSignal(true);
  EXPECT_EQ(observer_1.num_external_disk_state(), 2);
  EXPECT_TRUE(observer_1.last_external_disk_state());
}

// Tests that synchronous observers are notified about hardware verification
// status.
TEST_F(RmadClientTest, HardwareVerificationResult) {
  TestObserver observer_1(client_);

  EmitHardwareVerificationResultSignal(false, "fatal error");
  EXPECT_EQ(observer_1.num_hardware_verification_result(), 1);
  EXPECT_FALSE(observer_1.last_hardware_verification_result().is_compliant());
  EXPECT_EQ(observer_1.last_hardware_verification_result().error_str(),
            "fatal error");

  EmitHardwareVerificationResultSignal(true, "ok");
  EXPECT_EQ(observer_1.num_hardware_verification_result(), 2);
  EXPECT_TRUE(observer_1.last_hardware_verification_result().is_compliant());
  EXPECT_EQ(observer_1.last_hardware_verification_result().error_str(), "ok");
}

// Tests that synchronous observers are notified about finalization status.
TEST_F(RmadClientTest, FinalizationProgress) {
  TestObserver observer_1(client_);

  EmitFinalizationProgressSignal(
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS, 0.5,
      rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_UNKNOWN);
  EXPECT_EQ(observer_1.num_finalization_progress(), 1);
  EXPECT_EQ(observer_1.last_finalization_progress().status(),
            rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS);
  EXPECT_EQ(observer_1.last_finalization_progress().progress(), 0.5);

  EmitFinalizationProgressSignal(
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE, 1.0,
      rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_UNKNOWN);
  EXPECT_EQ(observer_1.num_finalization_progress(), 2);
  EXPECT_EQ(observer_1.last_finalization_progress().status(),
            rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE);
  EXPECT_EQ(observer_1.last_finalization_progress().progress(), 1.0);
}

// Tests that synchronous observers are notified about finalization errors.
TEST_F(RmadClientTest, FinalizationErrors) {
  TestObserver observer_1(client_);

  EmitFinalizationProgressSignal(
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING, 0.5,
      rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CR50);
  EXPECT_EQ(observer_1.num_finalization_progress(), 1);
  EXPECT_EQ(observer_1.last_finalization_progress().status(),
            rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING);
  EXPECT_EQ(observer_1.last_finalization_progress().progress(), 0.5);
  EXPECT_EQ(observer_1.last_finalization_progress().error(),
            rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CR50);
}

TEST_F(RmadClientTest, RoFirmwareUpdateProgress) {
  TestObserver observer_1(client_);

  EmitRoFirmwareUpdateProgressSignal(
      rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING);
  EXPECT_EQ(observer_1.num_ro_firmware_update_progress(), 1);
  EXPECT_EQ(observer_1.last_ro_firmware_update_status(),
            rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING);
}

TEST_F(RmadClientTest, RoFirmwareUpdateProgressBadParameterFails) {
  TestObserver observer_1(client_);

  EmitEmptySignal(rmad::kUpdateRoFirmwareStatusSignal);
  EXPECT_EQ(observer_1.num_ro_firmware_update_progress(), 0);
  EXPECT_EQ(observer_1.last_ro_firmware_update_status(),
            rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_UNKNOWN);
}

}  // namespace

}  // namespace ash
