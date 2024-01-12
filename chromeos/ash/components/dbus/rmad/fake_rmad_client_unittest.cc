// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rmad/fake_rmad_client.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
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
class FakeRmadClientTest : public testing::Test {
 public:
  FakeRmadClientTest() = default;
  FakeRmadClientTest(const FakeRmadClientTest&) = delete;
  FakeRmadClientTest& operator=(const FakeRmadClientTest&) = delete;

  void SetUp() override {
    // Create a fake client.
    // The constructor registers the singleton with RmadClient.
    // Rmad::InitializeFake is not called because it sets up the fake states for
    // testing ShimlessRMA app.
    new FakeRmadClient();
    client_ = RmadClient::Get();
  }

  void TearDown() override { RmadClient::Shutdown(); }

  FakeRmadClient* fake_client_() {
    return google::protobuf::down_cast<FakeRmadClient*>(client_.get());
  }

  raw_ptr<RmadClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Interface for observing changes from rmad.
class TestObserver : public RmadClient::Observer {
 public:
  explicit TestObserver(RmadClient* client) : client_(client) {
    client_->AddObserver(this);
  }
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override { client_->RemoveObserver(this); }

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
  int num_hardware_write_protection_state() {
    return num_hardware_write_protection_state_;
  }
  bool last_hardware_write_protection_state() {
    return last_hardware_write_protection_state_;
  }
  int num_power_cable_state() { return num_power_cable_state_; }
  bool last_power_cable_state() { return last_power_cable_state_; }
  int num_external_disk_state() const { return num_external_disk_state_; }
  bool last_external_disk_state() const { return last_external_disk_state_; }
  int num_hardware_verification_result() const {
    return num_hardware_verification_result_;
  }
  const rmad::HardwareVerificationResult& last_hardware_verification_result()
      const {
    return last_hardware_verification_result_;
  }
  int num_ro_firmware_update_progress() const {
    return num_ro_firmware_update_progress_;
  }
  rmad::UpdateRoFirmwareStatus last_ro_firmware_update_progress() const {
    return last_ro_firmware_update_progress_;
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

  // Called when calibration progress is updated.
  void CalibrationOverallProgress(
      rmad::CalibrationOverallStatus status) override {
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

  // Called when external disk is plugged in or removed.
  void ExternalDiskState(bool detected) override {
    num_external_disk_state_++;
    last_external_disk_state_ = detected;
  }

  // Called when hardware verification completes.
  void HardwareVerificationResult(
      const rmad::HardwareVerificationResult& last_hardware_verification_result)
      override {
    num_hardware_verification_result_++;
    last_hardware_verification_result_ = last_hardware_verification_result;
  }

  // Called when overall calibration progress is updated.
  void RoFirmwareUpdateProgress(rmad::UpdateRoFirmwareStatus status) override {
    num_ro_firmware_update_progress_++;
    last_ro_firmware_update_progress_ = status;
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
  int num_ro_firmware_update_progress_ = 0;
  rmad::UpdateRoFirmwareStatus last_ro_firmware_update_progress_;
};

rmad::RmadState CreateWelcomeState() {
  rmad::RmadState state;
  state.set_allocated_welcome(new rmad::WelcomeState());
  return state;
}

rmad::RmadState CreateDeviceDestinationState() {
  rmad::RmadState state;
  state.set_allocated_device_destination(new rmad::DeviceDestinationState());
  return state;
}

rmad::GetStateReply CreateWelcomeStateReply(rmad::RmadErrorCode error) {
  rmad::GetStateReply reply;
  reply.set_allocated_state(new rmad::RmadState());
  reply.mutable_state()->set_allocated_welcome(new rmad::WelcomeState());
  reply.set_error(error);
  return reply;
}

rmad::GetStateReply CreateDeviceDestinationStateReply(
    rmad::RmadErrorCode error) {
  rmad::GetStateReply reply;
  reply.set_allocated_state(new rmad::RmadState());
  reply.mutable_state()->set_allocated_device_destination(
      new rmad::DeviceDestinationState());
  reply.set_error(error);
  return reply;
}

TEST_F(FakeRmadClientTest, GetCurrentState_Default_RmaNotRequired) {
  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->GetCurrentState(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
  EXPECT_FALSE(response->has_state());
}

TEST_F(FakeRmadClientTest, GetCurrentState_Welcome_Ok) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->GetCurrentState(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
  EXPECT_TRUE(response->has_state());
  EXPECT_TRUE(response->state().has_welcome());
}

TEST_F(FakeRmadClientTest, GetCurrentState_Welcome_CorrectStateReturned) {
  std::vector<rmad::GetStateReply> fake_states;
  // Use any error to test.
  rmad::GetStateReply state =
      CreateWelcomeStateReply(rmad::RMAD_ERROR_MISSING_COMPONENT);
  state.mutable_state()->mutable_welcome()->set_choice(
      rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_FINALIZE_REPAIR);
  fake_states.push_back(std::move(state));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->GetCurrentState(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_MISSING_COMPONENT);
  EXPECT_TRUE(response->has_state());
  EXPECT_TRUE(response->state().has_welcome());
  EXPECT_EQ(response->state().welcome().choice(),
            rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_FINALIZE_REPAIR);
}

TEST_F(FakeRmadClientTest, TransitionNextState_Default_RmaNotRequired) {
  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->TransitionNextState(std::move(CreateWelcomeState()),
                               future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
  EXPECT_FALSE(response->has_state());
}

TEST_F(FakeRmadClientTest, TransitionNextState_NoNextState_Fails) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->TransitionNextState(std::move(CreateWelcomeState()),
                               future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_TRANSITION_FAILED);
  EXPECT_TRUE(response->has_state());
  EXPECT_TRUE(response->state().has_welcome());
}

TEST_F(FakeRmadClientTest, TransitionNextState_HasNextState_Ok) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_states.push_back(rmad::GetStateReply(
      CreateDeviceDestinationStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->TransitionNextState(std::move(CreateWelcomeState()),
                               future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
  EXPECT_TRUE(response->has_state());
  EXPECT_TRUE(response->state().has_device_destination());
}

TEST_F(FakeRmadClientTest, TransitionNextState_WrongCurrentState_Invalid) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_states.push_back(rmad::GetStateReply(
      CreateDeviceDestinationStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->TransitionNextState(std::move(CreateDeviceDestinationState()),
                               future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_REQUEST_INVALID);
  EXPECT_TRUE(response->has_state());
  EXPECT_TRUE(response->state().has_welcome());
}

TEST_F(FakeRmadClientTest, TransitionPreviousState_Default_RmaNotRequired) {
  base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
  client_->TransitionPreviousState(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
  EXPECT_FALSE(response->has_state());
}

TEST_F(FakeRmadClientTest, TransitionPreviousState_HasPreviousState_Ok) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_states.push_back(rmad::GetStateReply(
      CreateDeviceDestinationStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  {
    base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
    client_->TransitionNextState(std::move(CreateWelcomeState()),
                                 future.GetCallback());
    const auto& response = future.Get();
    EXPECT_TRUE(response.has_value());
    EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
    EXPECT_TRUE(response->has_state());
    EXPECT_TRUE(response->state().has_device_destination());
  }
  {
    base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
    client_->TransitionPreviousState(future.GetCallback());
    const auto& response = future.Get();
    LOG(ERROR) << "Prev started";
    EXPECT_TRUE(response.has_value());
    EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
    EXPECT_TRUE(response->has_state());
    EXPECT_TRUE(response->state().has_welcome());
  }
}

TEST_F(FakeRmadClientTest,
       TransitionPreviousState_HasPreviousState_StateUpdated) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK));
  fake_states.push_back(rmad::GetStateReply(
      CreateDeviceDestinationStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  {
    base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
    client_->GetCurrentState(future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
    EXPECT_EQ(future.Get()->error(), rmad::RMAD_ERROR_OK);
    EXPECT_TRUE(future.Get()->has_state());
    EXPECT_TRUE(future.Get()->state().has_welcome());
    EXPECT_EQ(future.Get()->state().welcome().choice(),
              rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_UNKNOWN);
  }
  {
    rmad::RmadState current_state = CreateWelcomeState();
    current_state.mutable_welcome()->set_choice(
        rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_FINALIZE_REPAIR);

    base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
    client_->TransitionNextState(std::move(current_state),
                                 future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
    EXPECT_EQ(future.Get()->error(), rmad::RMAD_ERROR_OK);
    EXPECT_TRUE(future.Get()->has_state());
    EXPECT_TRUE(future.Get()->state().has_device_destination());
  }
  {
    base::test::TestFuture<std::optional<rmad::GetStateReply>> future;
    client_->TransitionPreviousState(future.GetCallback());
    const auto& response = future.Get();
    LOG(ERROR) << "Prev started";
    EXPECT_TRUE(response.has_value());
    EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
    EXPECT_TRUE(response->has_state());
    EXPECT_TRUE(response->state().has_welcome());
    EXPECT_EQ(response->state().welcome().choice(),
              rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_FINALIZE_REPAIR);
  }
}

TEST_F(FakeRmadClientTest, Abortable_Default_Rma_Not_Required) {
  base::test::TestFuture<std::optional<rmad::AbortRmaReply>> future;
  client_->AbortRma(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
}

TEST_F(FakeRmadClientTest, Abortable_SetFalse_CannotCancel) {
  fake_client_()->SetAbortable(false);
  base::test::TestFuture<std::optional<rmad::AbortRmaReply>> future;
  client_->AbortRma(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_CANNOT_CANCEL_RMA);
}

TEST_F(FakeRmadClientTest, Abortable_SetTrue_Rma_Not_Required) {
  fake_client_()->SetAbortable(true);
  base::test::TestFuture<std::optional<rmad::AbortRmaReply>> future;
  client_->AbortRma(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
}

TEST_F(FakeRmadClientTest, GetLog) {
  const std::string expected_log = "This is my test log for the RMA process";
  fake_client_()->SetGetLogReply(expected_log, rmad::RMAD_ERROR_OK);
  base::test::TestFuture<std::optional<rmad::GetLogReply>> future;
  client_->GetLog(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->log(), expected_log);
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
}

TEST_F(FakeRmadClientTest, SaveLog) {
  const std::string expected_save_path = "fake save path for testing";
  fake_client_()->SetSaveLogReply(expected_save_path, rmad::RMAD_ERROR_OK);
  base::test::TestFuture<std::optional<rmad::SaveLogReply>> future;
  client_->SaveLog("Diagnostics log text", future.GetCallback());
  const auto& response = future.Get();

  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->save_path(), expected_save_path);
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
}

TEST_F(FakeRmadClientTest, RecordBrowserActionMetric) {
  fake_client_()->SetRecordBrowserActionMetricReply(rmad::RMAD_ERROR_OK);
  base::test::TestFuture<std::optional<rmad::RecordBrowserActionMetricReply>>
      future;

  rmad::RecordBrowserActionMetricRequest request;
  request.set_diagnostics(true);
  request.set_os_update(false);

  client_->RecordBrowserActionMetric(request, future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
}

TEST_F(FakeRmadClientTest, ExtractExternalDiagnosticsApp_NotFound) {
  base::test::TestFuture<
      std::optional<rmad::ExtractExternalDiagnosticsAppReply>>
      future;
  client_->ExtractExternalDiagnosticsApp(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND);
}

TEST_F(FakeRmadClientTest, ExtractExternalDiagnosticsApp_Found) {
  fake_client_()->external_diag_app_path() =
      base::FilePath{"/example/diag_app"};

  base::test::TestFuture<
      std::optional<rmad::ExtractExternalDiagnosticsAppReply>>
      future;
  client_->ExtractExternalDiagnosticsApp(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
  EXPECT_EQ(response->diagnostics_app_swbn_path(), "/example/diag_app.swbn");
  EXPECT_EQ(response->diagnostics_app_crx_path(), "/example/diag_app.crx");
}

TEST_F(FakeRmadClientTest, InstallExtractedDiagnosticsApp_NotFound) {
  base::test::TestFuture<
      std::optional<rmad::InstallExtractedDiagnosticsAppReply>>
      future;
  client_->InstallExtractedDiagnosticsApp(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND);
}

TEST_F(FakeRmadClientTest, InstallExtractedDiagnosticsApp_Found) {
  fake_client_()->external_diag_app_path() =
      base::FilePath{"/example/diag_app"};

  base::test::TestFuture<
      std::optional<rmad::InstallExtractedDiagnosticsAppReply>>
      future;
  client_->InstallExtractedDiagnosticsApp(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
  // The installed path was set so the next `GetInstalledDiagnosticsApp` will
  // get the installed package path.
  EXPECT_EQ(fake_client_()->installed_diag_app_path(),
            fake_client_()->external_diag_app_path());
}

TEST_F(FakeRmadClientTest, GetInstalledDiagnosticsApp_NotFound) {
  base::test::TestFuture<std::optional<rmad::GetInstalledDiagnosticsAppReply>>
      future;
  client_->GetInstalledDiagnosticsApp(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND);
}

TEST_F(FakeRmadClientTest, GetInstalledDiagnosticsApp_Found) {
  fake_client_()->installed_diag_app_path() =
      base::FilePath{"/example/diag_app"};

  base::test::TestFuture<std::optional<rmad::GetInstalledDiagnosticsAppReply>>
      future;
  client_->GetInstalledDiagnosticsApp(future.GetCallback());
  const auto& response = future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
  EXPECT_EQ(response->diagnostics_app_swbn_path(), "/example/diag_app.swbn");
  EXPECT_EQ(response->diagnostics_app_crx_path(), "/example/diag_app.crx");
}

// Tests that synchronous observers are notified about errors that occur outside
// of state transitions.
TEST_F(FakeRmadClientTest, ErrorObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerErrorObservation(
      rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
  EXPECT_EQ(observer_1.num_error(), 1);
  EXPECT_EQ(observer_1.last_error(),
            rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
}

// Tests that synchronous observers are notified about component calibration
// progress.
TEST_F(FakeRmadClientTest, CalibrationProgressObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerCalibrationProgressObservation(
      rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER,
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS, 0.5);
  EXPECT_EQ(observer_1.num_calibration_progress(), 1);
  EXPECT_EQ(observer_1.last_calibration_component_status().component(),
            rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER);
  EXPECT_EQ(observer_1.last_calibration_component_status().status(),
            rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS);
  EXPECT_EQ(observer_1.last_calibration_component_status().progress(), 0.5);
}

// Tests that synchronous observers are notified about overall calibration
// progress.
TEST_F(FakeRmadClientTest, CalibrationOverallProgressObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerCalibrationOverallProgressObservation(
      rmad::CalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
  EXPECT_EQ(observer_1.num_calibration_overall_progress(), 1);
  EXPECT_EQ(observer_1.last_calibration_overall_status(),
            rmad::CalibrationOverallStatus::
                RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
}

// Tests that synchronous observers are notified about provisioning progress.
TEST_F(FakeRmadClientTest, ProvisioningProgressObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerProvisioningProgressObservation(
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS, 0.25,
      rmad::ProvisionStatus::RMAD_PROVISION_ERROR_WP_ENABLED);
  EXPECT_EQ(observer_1.num_provisioning_progress(), 1);
  EXPECT_EQ(observer_1.last_provisioning_status().status(),
            rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS);
  EXPECT_EQ(observer_1.last_provisioning_status().progress(), 0.25);
  EXPECT_EQ(observer_1.last_provisioning_status().error(),
            rmad::ProvisionStatus::RMAD_PROVISION_ERROR_WP_ENABLED);
}

// Tests that synchronous observers are notified about hardware write protection
// state.
TEST_F(FakeRmadClientTest, HardwareWriteProtectionStateObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerHardwareWriteProtectionStateObservation(false);
  EXPECT_EQ(observer_1.num_hardware_write_protection_state(), 1);
  EXPECT_FALSE(observer_1.last_hardware_write_protection_state());

  fake_client_()->TriggerHardwareWriteProtectionStateObservation(true);
  EXPECT_EQ(observer_1.num_hardware_write_protection_state(), 2);
  EXPECT_TRUE(observer_1.last_hardware_write_protection_state());
}

// Tests that synchronous observers are notified about power cable state.
TEST_F(FakeRmadClientTest, PowerCableStateObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerPowerCableStateObservation(false);
  EXPECT_EQ(observer_1.num_power_cable_state(), 1);
  EXPECT_FALSE(observer_1.last_power_cable_state());

  fake_client_()->TriggerPowerCableStateObservation(true);
  EXPECT_EQ(observer_1.num_power_cable_state(), 2);
  EXPECT_TRUE(observer_1.last_power_cable_state());
}

// Tests that synchronous observers are notified about external disk state.
TEST_F(FakeRmadClientTest, ExternalDiskStateObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerExternalDiskStateObservation(false);
  EXPECT_EQ(observer_1.num_external_disk_state(), 1);
  EXPECT_FALSE(observer_1.last_external_disk_state());

  fake_client_()->TriggerExternalDiskStateObservation(true);
  EXPECT_EQ(observer_1.num_external_disk_state(), 2);
  EXPECT_TRUE(observer_1.last_external_disk_state());
}

// Tests that synchronous observers are notified about hardware verification
// status.
TEST_F(FakeRmadClientTest, HardwareVerificationResultObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerHardwareVerificationResultObservation(false,
                                                               "fatal error");
  EXPECT_EQ(observer_1.num_hardware_verification_result(), 1);
  EXPECT_FALSE(observer_1.last_hardware_verification_result().is_compliant());
  EXPECT_EQ(observer_1.last_hardware_verification_result().error_str(),
            "fatal error");

  fake_client_()->TriggerHardwareVerificationResultObservation(true, "ok");
  EXPECT_EQ(observer_1.num_hardware_verification_result(), 2);
  EXPECT_TRUE(observer_1.last_hardware_verification_result().is_compliant());
  EXPECT_EQ(observer_1.last_hardware_verification_result().error_str(), "ok");
}

// Tests that synchronous observers are notified about ro firmware update
// progress.
TEST_F(FakeRmadClientTest, RoFirmwareUpdateProgressObservation) {
  TestObserver observer_1(client_);

  fake_client_()->TriggerRoFirmwareUpdateProgressObservation(
      rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING);
  EXPECT_EQ(observer_1.num_ro_firmware_update_progress(), 1);
  EXPECT_EQ(observer_1.last_ro_firmware_update_progress(),
            rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING);

  fake_client_()->TriggerRoFirmwareUpdateProgressObservation(
      rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING);
  EXPECT_EQ(observer_1.num_ro_firmware_update_progress(), 2);
  EXPECT_EQ(observer_1.last_ro_firmware_update_progress(),
            rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING);
}

}  // namespace
}  // namespace ash
