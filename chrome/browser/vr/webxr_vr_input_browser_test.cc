// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"
#include "chrome/browser/vr/test/webvr_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/openvr/test/test_hook.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "third_party/openvr/src/headers/openvr.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrInputTest.java.
// End-to-end tests for user input interaction with WebXR/WebVR.

namespace vr {

// Test that focus is locked to the presenting display for the purposes of VR/XR
// input.
void TestPresentationLocksFocusImpl(WebXrVrBrowserTestBase* t,
                                    std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepSetupFocusLoss()");
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationLocksFocus)) {
  TestPresentationLocksFocusImpl(this, "test_presentation_locks_focus");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationLocksFocus)) {
  TestPresentationLocksFocusImpl(this, "webxr_test_presentation_locks_focus");
}

class WebXrControllerInputOpenVRMock : public MockOpenVRDeviceHookBase {
 public:
  void OnFrameSubmitted(
      device_test::mojom::SubmittedFrameDataPtr frame_data,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;

  void WaitNumFrames(unsigned int num_frames) {
    DCHECK(!wait_loop_);
    target_submitted_frames_ = num_submitted_frames_ + num_frames;
    wait_loop_ = new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed);
    wait_loop_->Run();
    delete wait_loop_;
    wait_loop_ = nullptr;
  }

  void ToggleTrigger(unsigned int index,
                     device::ControllerFrameData& controller_data) {
    uint64_t trigger_mask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
    controller_data.packet_number++;
    controller_data.buttons_pressed ^= trigger_mask;
    UpdateController(index, controller_data);
    // TODO(https://crbug.com/887726): Figure out why waiting for OpenVR to grab
    // the updated state instead of waiting for a number of frames causes frames
    // to be submitted at an extremely slow rate. Once fixed, switch away from
    // waiting on number of frames.
    WaitNumFrames(30);
  }

  void PressReleaseTrigger(unsigned int index,
                           device::ControllerFrameData& controller_data) {
    ToggleTrigger(index, controller_data);
    ToggleTrigger(index, controller_data);
  }

 private:
  base::RunLoop* wait_loop_ = nullptr;
  unsigned int num_submitted_frames_ = 0;
  unsigned int target_submitted_frames_ = 0;
};

void WebXrControllerInputOpenVRMock::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  num_submitted_frames_++;
  if (wait_loop_ && target_submitted_frames_ == num_submitted_frames_) {
    wait_loop_->Quit();
  }
  std::move(callback).Run();
}

// Test that OpenVR controller input is registered via WebXR's input method.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream_WebXr.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestControllerInputRegistered)) {
  WebXrControllerInputOpenVRMock my_mock;

  // Connect a controller.
  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  unsigned int controller_index = my_mock.ConnectController(controller_data);

  // Load the test page and enter presentation.
  this->LoadUrlAndAwaitInitialization(
      this->GetHtmlTestFile("test_webxr_input"));
  this->EnterSessionWithUserGestureOrFail();

  unsigned int num_iterations = 10;
  this->RunJavaScriptOrFail("stepSetupListeners(" +
                            std::to_string(num_iterations) + ")");

  // Press and unpress the controller's trigger a bunch of times and make sure
  // they're all registered.
  for (unsigned int i = 0; i < num_iterations; ++i) {
    my_mock.PressReleaseTrigger(controller_index, controller_data);
    // After each trigger release, wait for the JavaScript to receive the
    // "select" event.
    this->WaitOnJavaScriptStep();
  }
  this->EndTest();
}

// Test that OpenVR controller input is registered via the Gamepad API.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream
IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       REQUIRES_GPU(TestControllerInputRegistered)) {
  WebXrControllerInputOpenVRMock my_mock;

  // Connect a controller.
  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  // openvr_gamepad_helper assumes axis index 1 is the trigger, so we need to
  // set that here, otherwise it won't check whether it's pressed or not.
  controller_data.axis_data[1].axis_type = vr::k_eControllerAxis_Trigger;
  unsigned int controller_index = my_mock.ConnectController(controller_data);

  // Load the test page and enter presentation.
  this->LoadUrlAndAwaitInitialization(
      this->GetHtmlTestFile("test_gamepad_button"));
  this->EnterSessionWithUserGestureOrFail();

  // We need to have this, otherwise the JavaScript side of the Gamepad API
  // doesn't seem to pick up the correct button state? I.e. if we don't have
  // this, openvr_gamepad_helper properly sets the gamepad's button state,
  // but JavaScript still shows no buttons pressed.
  // TODO(bsheedy): Figure out why this is the case.
  my_mock.PressReleaseTrigger(controller_index, controller_data);

  // Setting this in the Android version of the test needs to happen after a
  // flakiness workaround. Coincidentally, it's also helpful for the different
  // issue solved by the above PressReleaseTrigger, so make sure to set it here
  // so that the above press/release isn't caught by the test code.
  this->RunJavaScriptOrFail("canStartTest = true");
  // Press and release the trigger, ensuring the Gamepad API detects both.
  my_mock.ToggleTrigger(controller_index, controller_data);
  this->WaitOnJavaScriptStep();
  my_mock.ToggleTrigger(controller_index, controller_data);
  this->WaitOnJavaScriptStep();
  this->EndTest();
}

}  // namespace vr
