// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "third_party/openvr/src/headers/openvr.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrInputTest.java.
// End-to-end tests for user input interaction with WebXR.

namespace vr {

// Helper function for verifying the XRInputSource.profiles array contents.
void VerifyInputSourceProfilesArray(
    WebXrVrBrowserTestBase* t,
    const std::vector<std::string>& expected_values) {
  t->PollJavaScriptBooleanOrFail(
      "isProfileCountEqualTo(" + base::NumberToString(expected_values.size()) +
          ")",
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  // We don't expect the contents of the profiles array to change once we've
  // verified its size above, so we can check the expressions a single time
  // here instead of polling them.
  for (size_t i = 0; i < expected_values.size(); ++i) {
    ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
        "isProfileEqualTo(" + base::NumberToString(i) + ", '" +
        expected_values[i] + "')"));
  }
}

void VerifyInputCounts(WebXrVrBrowserTestBase* t,
                       unsigned int expected_input_sources,
                       unsigned int expected_gamepads) {
  t->PollJavaScriptBooleanOrFail("inputSourceCount() === " +
                                 base::NumberToString(expected_input_sources));
  t->PollJavaScriptBooleanOrFail("inputSourceWithGamepadCount() === " +
                                 base::NumberToString(expected_gamepads));
}

// Test that focus is locked to the presenting display for the purposes of VR/XR
// input.
void TestPresentationLocksFocusImpl(WebXrVrBrowserTestBase* t,
                                    std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetFileUrlForHtmlTestFile(filename));
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepSetupFocusLoss()");
  t->EndTest();
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentationLocksFocus) {
  TestPresentationLocksFocusImpl(t, "webxr_test_presentation_locks_focus");
}

class WebXrControllerInputMock : public MockXRDeviceHookBase {
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

  // TODO(https://crbug.com/887726): Figure out why waiting for OpenVR to grab
  // the updated state instead of waiting for a number of frames causes frames
  // to be submitted at an extremely slow rate. Once fixed, switch away from
  // waiting on number of frames.
  void UpdateControllerAndWait(
      unsigned int index,
      const device::ControllerFrameData& controller_data) {
    UpdateController(index, controller_data);
    WaitNumFrames(30);
  }

  void ToggleButtonTouches(unsigned int index, uint64_t button_mask) {
    auto controller_data = GetCurrentControllerData(index);

    controller_data.packet_number++;
    controller_data.buttons_touched ^= button_mask;

    UpdateControllerAndWait(index, controller_data);
  }

  void ToggleButtons(unsigned int index, uint64_t button_mask) {
    auto controller_data = GetCurrentControllerData(index);

    controller_data.packet_number++;
    controller_data.buttons_pressed ^= button_mask;
    controller_data.buttons_touched ^= button_mask;
    UpdateControllerAndWait(index, controller_data);
  }

  void ToggleTriggerButton(unsigned int index, device::XrButtonId button_id) {
    auto controller_data = GetCurrentControllerData(index);
    uint64_t button_mask = device::XrButtonMaskFromId(button_id);

    controller_data.packet_number++;
    controller_data.buttons_pressed ^= button_mask;
    controller_data.buttons_touched ^= button_mask;

    bool is_pressed = ((controller_data.buttons_pressed & button_mask) != 0);

    unsigned int axis_offset = device::XrAxisOffsetFromId(button_id);
    DCHECK(controller_data.axis_data[axis_offset].axis_type ==
           device::XrAxisType::kTrigger);
    controller_data.axis_data[axis_offset].x = is_pressed ? 1.0 : 0.0;
    UpdateControllerAndWait(index, controller_data);
  }

  void SetAxes(unsigned int index,
               device::XrButtonId button_id,
               float x,
               float y) {
    auto controller_data = GetCurrentControllerData(index);
    unsigned int axis_offset = device::XrAxisOffsetFromId(button_id);
    DCHECK(controller_data.axis_data[axis_offset].axis_type != 0);

    controller_data.packet_number++;
    controller_data.axis_data[axis_offset].x = x;
    controller_data.axis_data[axis_offset].y = y;
    UpdateControllerAndWait(index, controller_data);
  }

  void TogglePrimaryTrigger(unsigned int index) {
    ToggleTriggerButton(index, device::XrButtonId::kAxisTrigger);
  }

  void PressReleasePrimaryTrigger(unsigned int index) {
    TogglePrimaryTrigger(index);
    TogglePrimaryTrigger(index);
  }

  void SetControllerPose(unsigned int index,
                         const gfx::Transform& device_to_origin,
                         bool is_valid) {
    auto controller_data = GetCurrentControllerData(index);
    controller_data.pose_data.is_valid = is_valid;
    device_to_origin.matrix().asColMajorf(
        controller_data.pose_data.device_to_origin);
    UpdateControllerAndWait(index, controller_data);
  }

  unsigned int CreateAndConnectMinimalGamepad(
      device::ControllerRole role =
          device::ControllerRole::kControllerRoleRight) {
    // Create a controller that only supports select via a trigger, i.e. it has
    // just enough data to be considered a gamepad.
    uint64_t supported_buttons =
        device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger);

    std::map<device::XrButtonId, unsigned int> axis_types = {
        {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
    };

    return CreateAndConnectController(role, axis_types, supported_buttons);
  }

  unsigned int CreateAndConnectController(
      device::ControllerRole role,
      std::map<device::XrButtonId, unsigned int> axis_types = {},
      uint64_t supported_buttons = UINT64_MAX) {
    auto controller = CreateValidController(role);
    controller.supported_buttons = supported_buttons;
    for (const auto& axis_type : axis_types) {
      unsigned int axis_offset = device::XrAxisOffsetFromId(axis_type.first);
      controller.axis_data[axis_offset].axis_type = axis_type.second;
    }

    return ConnectController(controller);
  }

  void UpdateControllerSupport(
      unsigned int controller_index,
      const std::map<device::XrButtonId, unsigned int>& axis_types,
      uint64_t supported_buttons) {
    auto controller_data = GetCurrentControllerData(controller_index);

    for (unsigned int i = 0; i < device::kMaxNumAxes; i++) {
      auto button_id = GetAxisId(i);
      auto it = axis_types.find(button_id);
      unsigned int new_axis_type = device::XrAxisType::kNone;
      if (it != axis_types.end())
        new_axis_type = it->second;
      controller_data.axis_data[i].axis_type = new_axis_type;
    }

    controller_data.supported_buttons = supported_buttons;

    UpdateControllerAndWait(controller_index, controller_data);
  }

  void UpdateControllerRole(unsigned int controller_index,
                            device::ControllerRole role) {
    auto controller_data = GetCurrentControllerData(controller_index);
    controller_data.role = role;
    UpdateControllerAndWait(controller_index, controller_data);
  }

  // A controller is necessary to simulate voice input because of how the test
  // API works.
  unsigned int CreateVoiceController() {
    return CreateAndConnectMinimalGamepad(
        device::ControllerRole::kControllerRoleVoice);
  }

 private:
  // kAxisTrackpad is the first entry in XrButtonId that maps to an axis and the
  // subsequent entries are also for input axes.
  device::XrButtonId GetAxisId(unsigned int offset) {
    return static_cast<device::XrButtonId>(device::XrButtonId::kAxisTrackpad +
                                           offset);
  }

  device::ControllerFrameData GetCurrentControllerData(unsigned int index) {
    auto iter = controller_data_map_.find(index);
    DCHECK(iter != controller_data_map_.end());
    return iter->second;
  }

  base::RunLoop* wait_loop_ = nullptr;
  unsigned int num_submitted_frames_ = 0;
  unsigned int target_submitted_frames_ = 0;
};

void WebXrControllerInputMock::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  num_submitted_frames_++;
  if (wait_loop_ && target_submitted_frames_ == num_submitted_frames_) {
    wait_loop_->Quit();
  }
  std::move(callback).Run();
}

// Ensure that when an input source's handedness changes, an input source change
// event is fired and a new input source is created.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInputHandednessChange) {
  WebXrControllerInputMock my_mock;
  unsigned int controller_index = my_mock.CreateAndConnectMinimalGamepad();

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_input_same_object"));
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // We only expect one input source, cache it.
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Change the handedness from right to left and verify that we get a change
  // event.  Then cache the new input source.
  my_mock.UpdateControllerRole(controller_index,
                               device::ControllerRole::kControllerRoleLeft);
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 2",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Switch back to the right hand and confirm that we get the change.
  my_mock.UpdateControllerRole(controller_index,
                               device::ControllerRole::kControllerRoleRight);
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 3",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Test that inputsourceschange events contain only the expected added/removed
// input sources when a mock controller is connected/disconnected.
// Also validates that if an input source changes substantially we get an event
// containing both the removal of the old one and the additon of the new one,
// rather than two events.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInputSourcesChange) {
  WebXrControllerInputMock my_mock;

  // TODO(crbug.com/963676): Figure out if the race is a product or test bug.
  // There's a potential for a race causing the input sources change event to
  // fire multiple times if we disconnect a controller that has a gamepad.
  // Even just a select trigger is sufficient to have an xr-standard mapping, so
  // just expose a grip trigger instead so that we don't connect a gamepad.
  uint64_t insufficient_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);
  std::map<device::XrButtonId, unsigned int> insufficient_axis_types = {};
  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, insufficient_axis_types,
      insufficient_buttons);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_input_sources_change_event"));
  t->EnterSessionWithUserGestureOrFail();

  // Wait for the first changed event
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Validate that we only have one controller added, and no controller removed.
  t->RunJavaScriptOrFail("validateAdded(1)");
  t->RunJavaScriptOrFail("validateRemoved(0)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");
  t->RunJavaScriptOrFail("validateCachedAddedPresence(true)");

  // Disconnect the controller and validate that we only have one controller
  // removed, and that our previously cached controller is in the removed array.
  my_mock.DisconnectController(controller_index);
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 2",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("validateAdded(0)");
  t->RunJavaScriptOrFail("validateRemoved(1)");
  t->RunJavaScriptOrFail("validateCachedRemovedPresence(true)");

  // Connect a controller, and then change enough properties that the system
  // recalculates its status as a valid controller, so that we can verify
  // it is both added and removed.
  // Since we're changing the controller state without disconnecting it, we can
  // (and should) use the minimal gamepad here.
  controller_index = my_mock.CreateAndConnectMinimalGamepad();
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 3",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // At least currently, there is no way for WMR/OpenXR to have insufficient
  // buttons for a gamepad as long as a controller is connected, so skip this
  // part on WMR/OpenXR since it'll always fail
  if (t->GetRuntimeType() != XrBrowserTestBase::RuntimeType::RUNTIME_WMR &&
      t->GetRuntimeType() != XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    my_mock.UpdateControllerSupport(controller_index, insufficient_axis_types,
                                    insufficient_buttons);

    t->PollJavaScriptBooleanOrFail("inputChangeEvents === 4",
                                   WebXrVrBrowserTestBase::kPollTimeoutShort);
    t->RunJavaScriptOrFail("validateAdded(1)");
    t->RunJavaScriptOrFail("validateRemoved(1)");
    t->RunJavaScriptOrFail("validateCachedAddedPresence(false)");
    t->RunJavaScriptOrFail("validateCachedRemovedPresence(true)");
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Ensure that when an input source's profiles array changes, an input source
// change event is fired and a new input source is created.
// OpenVR-only since WMR/OpenXR only supports one kind of gamepad, so it's not
// possible to update the connected gamepad functionality to force the profiles
// array to change.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestInputProfilesChange) {
  WebXrControllerInputMock my_mock;
  unsigned int controller_index = my_mock.CreateAndConnectMinimalGamepad();

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_input_same_object"));
  EnterSessionWithUserGestureOrFail();

  // Wait for the first changed event
  PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                              WebXrVrBrowserTestBase::kPollTimeoutShort);

  // We only expect one input source, cache it.
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Add a touchpad so that the profiles array changes and verify that we get a
  // change event.
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad);
  std::map<device::XrButtonId, unsigned int> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
  };
  my_mock.UpdateControllerSupport(controller_index, axis_types,
                                  supported_buttons);

  PollJavaScriptBooleanOrFail("inputChangeEvents === 2",
                              WebXrVrBrowserTestBase::kPollTimeoutShort);
  RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that changes to a gamepad object respect that it is the same object
// and that if whether or not an input source has a gamepad changes that the
// input source change event is fired and a new input source is created.
// OpenVR-only since WMR/OpenXR doesn't support the notion of an incomplete
// gamepad except if using voice input.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestInputGamepadSameObject) {
  WebXrControllerInputMock my_mock;

  // Create a set of buttons and axes that don't have enough data to be made
  // into an xr-standard gamepad (which we expect the runtimes to not report).
  // Even just setting the select trigger is now enough to create an xr-standard
  // gamepad, so we only set the grip trigger in this case.
  uint64_t insufficient_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);
  std::map<device::XrButtonId, unsigned int> insufficient_axis_types = {};

  // Create a set of buttons and axes that we expect to have enough data to be
  // made into an xr-standard gamepad (which we expect the runtimes to report).
  uint64_t sufficient_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad);
  std::map<device::XrButtonId, unsigned int> sufficient_axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
  };

  // Start off without a gamepad.
  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, insufficient_axis_types,
      insufficient_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_input_same_object"));
  EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  PollJavaScriptBooleanOrFail("inputChangeEvents === 1", kPollTimeoutShort);

  // We only expect one input source, cache it.
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Toggle a button and confirm that the controller is still the same.
  my_mock.ToggleButtons(controller_index, insufficient_buttons);
  RunJavaScriptOrFail("validateCachedSourcePresence(true)");
  RunJavaScriptOrFail("validateCurrentAndCachedGamepadMatch()");

  // Update the controller to now support a gamepad and verify that we get a
  // change event and that the old controller isn't present.  Then cache the new
  // one.
  my_mock.UpdateControllerSupport(controller_index, sufficient_axis_types,
                                  sufficient_buttons);
  PollJavaScriptBooleanOrFail("inputChangeEvents === 2", kPollTimeoutShort);
  RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Toggle a button and confirm that the controller is still the same.
  my_mock.PressReleasePrimaryTrigger(controller_index);
  RunJavaScriptOrFail("validateCachedSourcePresence(true)");
  RunJavaScriptOrFail("validateCurrentAndCachedGamepadMatch()");

  // Switch back to the insufficient gamepad and confirm that we get the change.
  my_mock.UpdateControllerSupport(controller_index, insufficient_axis_types,
                                  insufficient_buttons);
  PollJavaScriptBooleanOrFail("inputChangeEvents === 3", kPollTimeoutShort);
  RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if the controller lacks enough data to be considered a Gamepad
// that the input source that it is associated with does not have a Gamepad.
// OpenVR-only because WMR/OpenXR does not currently support the notion of
// incomplete gamepads other than voice input.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestGamepadIncompleteData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that only supports select, i.e. it lacks enough data
  // to be considered a gamepad.
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger);
  my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, {}, supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();
  PollJavaScriptBooleanOrFail("inputSourceHasNoGamepad()", kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isProfileCountEqualTo(0)", kPollTimeoutShort);
  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if a Gamepad has the minimum required number of axes/buttons to
// be considered an xr-standard Gamepad, that it is exposed as such, and that
// we can check the state of it's priamry axes/button.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestGamepadMinimumData) {
  WebXrControllerInputMock my_mock;

  unsigned int controller_index = my_mock.CreateAndConnectMinimalGamepad();

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 1, 1);

  // We only actually connect the data for the one button, but WMR/OpenXR
  // expects the WMR/OpenXR  controller (which has all of the required and
  // optional buttons) and so adds dummy/placeholder buttons regardless of what
  // data we send up.
  std::string button_count = "1";
  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_WMR ||
      t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR)
    button_count = "4";

  t->PollJavaScriptBooleanOrFail("isButtonCountEqualTo(" + button_count + ")",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Press the trigger and set the axis to a non-zero amount, so we can ensure
  // we aren't getting just default gamepad data.
  my_mock.TogglePrimaryTrigger(controller_index);

  // The trigger should be button 0.
  t->PollJavaScriptBooleanOrFail("isMappingEqualTo('xr-standard')",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_WMR) {
    // WMR will still report having grip, touchpad, and thumbstick because it
    // only supports that type of controller and fills in default values if
    // those inputs don't exist.
    VerifyInputSourceProfilesArray(
        t, {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  } else if (t->GetRuntimeType() ==
             XrBrowserTestBase::RuntimeType::RUNTIME_OPENVR) {
    VerifyInputSourceProfilesArray(
        t, {"test-value-test-value", "generic-trigger"});
  } else if (t->GetRuntimeType() ==
             XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    // OpenXR will still report having squeeze, menu, touchpad, and thumbstick
    // because it only supports that type of controller and fills in default
    // values if those inputs don't exist.
    VerifyInputSourceProfilesArray(
        t, {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Make sure the input gets plumbed to the correct gamepad, including when
// button presses are interleaved.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestMultipleGamepads) {
  WebXrControllerInputMock my_mock;

  unsigned int controller_index1 = my_mock.CreateAndConnectMinimalGamepad(
      device::ControllerRole::kControllerRoleLeft);
  unsigned int controller_index2 = my_mock.CreateAndConnectMinimalGamepad();

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 2, 2);

  // We only actually connect the data for the one button, but WMR/OpenXR
  // expects the WMR/OpenXR controller (which has all of the required and
  // optional buttons) and so adds dummy/placeholder buttons regardless of what
  // data we send up.
  std::string button_count = "1";
  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_WMR ||
      t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR)
    button_count = "4";

  // Make sure both gamepads have the expected button count and mapping.
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("isButtonCountEqualTo(" +
                                                   button_count + ", 0)"));
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("isButtonCountEqualTo(" +
                                                   button_count + ", 1)"));
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
      "isMappingEqualTo('xr-standard', 0)"));
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
      "isMappingEqualTo('xr-standard', 1)"));

  // Press the trigger and set the axis to a non-zero amount, so we can ensure
  // we aren't getting just default gamepad data.
  my_mock.TogglePrimaryTrigger(controller_index1);

  // The trigger should be button 0. Make sure it is only pressed on the first
  // gamepad.
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true, 0)");
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, false, 1)");

  // Now press the other gamepad's button and make sure it's registered.
  my_mock.TogglePrimaryTrigger(controller_index2);
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true, 1)");

  // Then release the trigger. The second gamepad's button should no longer be
  // pressed, but the first gamepad's button should still be pressed because we
  // haven't released that trigger yet.
  my_mock.TogglePrimaryTrigger(controller_index2);
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, false, 1)");
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true, 0)");

  // Finally, release the trigger on the first gamepad.
  my_mock.TogglePrimaryTrigger(controller_index1);
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, false, 0)");

  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_WMR) {
    // WMR will still report having grip, touchpad, and thumbstick because it
    // only supports that type of controller and fills in default values if
    // those inputs don't exist.
    VerifyInputSourceProfilesArray(
        t, {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  } else if (t->GetRuntimeType() ==
             XrBrowserTestBase::RuntimeType::RUNTIME_OPENVR) {
    VerifyInputSourceProfilesArray(
        t, {"test-value-test-value", "generic-trigger"});
  } else if (t->GetRuntimeType() ==
             XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    // OpenXR will still report having squeeze, menu, touchpad, and thumbstick
    // because it only supports that type of controller and fills in default
    // values if those inputs don't exist.
    VerifyInputSourceProfilesArray(
        t, {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Ensure that if a Gamepad has all of the required and optional buttons as
// specified by the xr-standard mapping, that those buttons are plumbed up
// in their required places.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestGamepadCompleteData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that supports all reserved buttons.
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisThumbstick) |
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);

  std::map<device::XrButtonId, unsigned int> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
      {device::XrButtonId::kAxisThumbstick, device::XrAxisType::kJoystick},
  };

  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 1, 1);

  // Setup some state on the optional buttons (as TestGamepadMinimumData should
  // ensure proper state on the required buttons).
  // Set a value on the touchpad.
  my_mock.SetAxes(controller_index, device::XrButtonId::kAxisTrackpad, 0.25,
                  -0.25);

  // Set the touchpad to be touched.
  my_mock.ToggleButtonTouches(
      controller_index,
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad));

  // Also test the thumbstick.
  my_mock.SetAxes(controller_index, device::XrButtonId::kAxisThumbstick, 0.67,
                  -0.67);
  my_mock.ToggleButtons(
      controller_index,
      device::XrButtonMaskFromId(device::XrButtonId::kAxisThumbstick));

  // Set the grip button to be pressed.
  my_mock.ToggleButtons(controller_index,
                        device::XrButtonMaskFromId(device::XrButtonId::kGrip));

  // Controller should meet the requirements for the 'xr-standard' mapping.
  t->PollJavaScriptBooleanOrFail("isMappingEqualTo('xr-standard')",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Controller should have all required and optional xr-standard buttons
  t->PollJavaScriptBooleanOrFail("isButtonCountEqualTo(4)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // The touchpad axes should be set appropriately.
  t->PollJavaScriptBooleanOrFail("areAxesValuesEqualTo(0, 0.25, -0.25)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // The thumbstick axes should be set appropriately.
  t->PollJavaScriptBooleanOrFail("areAxesValuesEqualTo(1, 0.67, -0.67)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Button 1 is reserved for the Grip, and should be pressed.
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(1, true)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Button 2 is reserved for the trackpad and should be touched but not
  // pressed.
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(2, false)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->PollJavaScriptBooleanOrFail("isButtonTouchedEqualTo(2, true)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Button 3 is reserved for the thumbstick and should be touched and pressed.
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(3, true)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->PollJavaScriptBooleanOrFail("isButtonTouchedEqualTo(3, true)",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_WMR) {
    // WMR will still report having grip, touchpad, and thumbstick because it
    // only supports that type of controller and fills in default values if
    // those inputs don't exist.
    VerifyInputSourceProfilesArray(
        t, {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  } else if (t->GetRuntimeType() ==
             XrBrowserTestBase::RuntimeType::RUNTIME_OPENVR) {
    VerifyInputSourceProfilesArray(
        t, {"test-value-test-value",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  } else if (t->GetRuntimeType() ==
             XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    // OpenXR will still report having squeeze, menu, touchpad, and thumbstick
    // because it only supports that type of controller and fills in default
    // values if those inputs don't exist.
    VerifyInputSourceProfilesArray(
        t, {"windows-mixed-reality",
            "generic-trigger-squeeze-touchpad-thumbstick"});
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Tests that axes data is still reported on the secondary axes even if
// the button is not supported (we see this case with WMR through OpenVR where
// the secondary axes button is reserved by the system, but we still get valid
// data for the axes, there may be other controllers where this is the case).
// Because this is specifically a bug in the OpenVR runtime/with configurable
// controllers, not testing WMR/OpenXR.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestInputAxesWithNoButton) {
  WebXrControllerInputMock my_mock;

  // Create a controller that supports all reserved buttons, except the
  // secondary axis. (Though it is a valid axis)
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad) |
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);

  std::map<device::XrButtonId, unsigned int> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
      {device::XrButtonId::kAxisThumbstick, device::XrAxisType::kJoystick},
  };

  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(this, 1, 1);

  // Setup some state on the optional buttons (as TestGamepadMinimumData should
  // ensure proper state on the required buttons).
  // Set a value on the secondary set of axes.
  my_mock.SetAxes(controller_index, device::XrButtonId::kAxisThumbstick, 0.25,
                  -0.25);
  // Controller should meet the requirements for the 'xr-standard' mapping.
  PollJavaScriptBooleanOrFail("isMappingEqualTo('xr-standard')",
                              WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Controller should have all required and optional xr-standard buttons
  PollJavaScriptBooleanOrFail("isButtonCountEqualTo(4)",
                              WebXrVrBrowserTestBase::kPollTimeoutShort);

  // The secondary set of axes should be set appropriately.
  PollJavaScriptBooleanOrFail("areAxesValuesEqualTo(1, 0.25, -0.25)",
                              WebXrVrBrowserTestBase::kPollTimeoutShort);

  // If we have a non-zero axis value, the button should be touched.
  PollJavaScriptBooleanOrFail("isButtonTouchedEqualTo(3, true)",
                              WebXrVrBrowserTestBase::kPollTimeoutShort);

  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if a Gamepad has all required buttons, an extra button not
// mapped in the xr-standard specification, and is missing reserved buttons
// from the XR Standard specification, that the extra button does not appear
// in either of the reserved button slots. OpenVR-only since WMR/OpenXR only
// supports one controller type.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestGamepadReservedData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that is missing reserved buttons, but supports an
  // extra button to guarantee that the reserved button is held.
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad) |
      device::XrButtonMaskFromId(device::XrButtonId::kA);

  std::map<device::XrButtonId, unsigned int> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
  };

  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(this, 1, 1);

  // Claim that all buttons are pressed, note that any non-supported buttons
  // should be ignored.
  my_mock.ToggleButtons(controller_index, UINT64_MAX);

  // Index 1 and 3 are reserved for the grip and joystick.
  // As our controller doesn't support them, they should be present but not
  // pressed, and our "extra" button should be index 4 and should be pressed.
  PollJavaScriptBooleanOrFail("isMappingEqualTo('xr-standard')",
                              kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonCountEqualTo(5)", kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true)",
                              kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(1, false)",
                              kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(2, true)",
                              kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(3, false)",
                              kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(4, true)",
                              kPollTimeoutShort);

  VerifyInputSourceProfilesArray(
      this, {"test-value-test-value", "generic-trigger-touchpad"});

  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if a gamepad has a grip, but not any extra buttons or a secondary
// axis, that no trailing placeholder button is added.  This is a slight
// variation on TestGamepadMinimalData, but won't re-test whether or not buttons
// get sent up.  Note that since WMR/OpenXR always builds the WMR/OpenXR
// controller which supports all required and optional buttons specified by the
// xr-standard mapping, this test is OpenVR-only.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestGamepadOptionalData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that supports the trigger, primary axis, and grip
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad) |
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);

  std::map<device::XrButtonId, unsigned int> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
  };

  my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(this, 1, 1);

  // There should be enough buttons for an xr-standard mapping, and it should
  // have one optional button, but not the other.
  PollJavaScriptBooleanOrFail("isMappingEqualTo('xr-standard')",
                              kPollTimeoutShort);
  PollJavaScriptBooleanOrFail("isButtonCountEqualTo(3)", kPollTimeoutShort);

  VerifyInputSourceProfilesArray(
      this, {"test-value-test-value", "generic-trigger-squeeze-touchpad"});

  RunJavaScriptOrFail("done()");
  EndTest();
}

// Test that controller input is registered via WebXR's input method. This uses
// multiple controllers to make sure the input is going to the correct one.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestMultipleControllerInputRegistered) {
  WebXrControllerInputMock my_mock;

  unsigned int controller_index1 = my_mock.CreateAndConnectMinimalGamepad(
      device::ControllerRole::kControllerRoleLeft);
  unsigned int controller_index2 = my_mock.CreateAndConnectMinimalGamepad();

  // Load the test page and enter presentation.
  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_input"));
  t->EnterSessionWithUserGestureOrFail();

  t->RunJavaScriptOrFail("stepSetupListeners(2)");

  // Press and release the first controller's trigger and make sure the select
  // events are registered for it. After trigger release, must wait for JS to
  // receive the "select" event.
  t->RunJavaScriptOrFail("expectedInputSourceIndex = 0");
  my_mock.PressReleasePrimaryTrigger(controller_index1);
  t->WaitOnJavaScriptStep();

  // Do the same thing for the other controller.
  t->RunJavaScriptOrFail("expectedInputSourceIndex = 1");
  my_mock.PressReleasePrimaryTrigger(controller_index2);
  t->WaitOnJavaScriptStep();

  t->EndTest();
}

// Test that voice input is registered via WebXR's input method. WMR only since
// it's the only platform we are testing that supports select via voice.
IN_PROC_BROWSER_TEST_F(WebXrVrWmrBrowserTest, TestVoiceSelectRegistered) {
  WebXrControllerInputMock my_mock;
  unsigned int index = my_mock.CreateVoiceController();

  // Load the test page and enter presentation.
  LoadUrlAndAwaitInitialization(GetFileUrlForHtmlTestFile("test_webxr_input"));
  EnterSessionWithUserGestureOrFail();

  RunJavaScriptOrFail("stepSetupListeners(1)");

  // Simulate the user saying "select" and make sure the select events are
  // registered for it. Must wait for JS to receive the "select" event.
  my_mock.PressReleasePrimaryTrigger(index);
  WaitOnJavaScriptStep();

  EndTest();
}

// Test that controller input is registered via WebXR's input method.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream_WebXr.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestControllerInputRegistered) {
  WebXrControllerInputMock my_mock;

  unsigned int controller_index = my_mock.CreateAndConnectMinimalGamepad();

  // Load the test page and enter presentation.
  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_input"));
  t->EnterSessionWithUserGestureOrFail();

  unsigned int num_iterations = 5;
  t->RunJavaScriptOrFail("stepSetupListeners(" +
                         base::NumberToString(num_iterations) + ")");

  // Press and unpress the controller's trigger a bunch of times and make sure
  // they're all registered.
  for (unsigned int i = 0; i < num_iterations; ++i) {
    my_mock.PressReleasePrimaryTrigger(controller_index);
    // After each trigger release, wait for the JavaScript to receive the
    // "select" event.
    t->WaitOnJavaScriptStep();
  }
  t->EndTest();
}

std::string TransformToColMajorString(const gfx::Transform& t) {
  float array[16];
  t.matrix().asColMajorf(array);
  std::string array_string = "[";
  for (int i = 0; i < 16; i++) {
    array_string += base::NumberToString(array[i]) + ",";
  }
  array_string.pop_back();
  array_string.push_back(']');
  return array_string;
}

// Test that changes in controller position are properly plumbed through to
// WebXR.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestControllerPositionTracking) {
  WebXrControllerInputMock my_mock;

  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  unsigned int controller_index = my_mock.ConnectController(controller_data);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("webxr_test_controller_poses"));
  t->EnterSessionWithUserGestureOrFail();

  auto pose = gfx::Transform();
  pose.RotateAboutXAxis(90);
  pose.RotateAboutYAxis(45);
  pose.RotateAboutZAxis(180);
  pose.Translate3d(0.5f, 2, -3);
  my_mock.SetControllerPose(controller_index, pose, true);

  // Apply any offset we expect the runtime to add.
  pose.Translate3d(t->GetControllerOffset());

  t->ExecuteStepAndWait("stepWaitForMatchingPose(" +
                        base::NumberToString(controller_index - 1) + ", " +
                        TransformToColMajorString(pose) + ")");
  t->AssertNoJavaScriptErrors();
}

class WebXrHeadPoseMock : public MockXRDeviceHookBase {
 public:
  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      final {
    auto pose = device_test::mojom::PoseFrameData::New();
    pose->device_to_origin = pose_;
    std::move(callback).Run(std::move(pose));
  }

  void SetHeadPose(const gfx::Transform& pose) { pose_ = pose; }

 private:
  gfx::Transform pose_;
};

// Test that head pose changes are properly reflected in the viewer pose
// provided by WebXR.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHeadPosesUpdate) {
  WebXrHeadPoseMock my_mock;

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("webxr_test_head_poses"));
  t->EnterSessionWithUserGestureOrFail();

  auto pose = gfx::Transform();
  my_mock.SetHeadPose(pose);
  t->RunJavaScriptOrFail("stepWaitForMatchingPose(" +
                         TransformToColMajorString(pose) + ")");
  t->WaitOnJavaScriptStep();

  // No significance to this new transform other than that it's easy to tell
  // whether the correct pose got piped through to WebXR or not.
  pose.RotateAboutXAxis(90);
  pose.Translate3d(2, 3, 4);
  my_mock.SetHeadPose(pose);
  t->RunJavaScriptOrFail("stepWaitForMatchingPose(" +
                         TransformToColMajorString(pose) + ")");
  t->WaitOnJavaScriptStep();
  t->AssertNoJavaScriptErrors();
}

}  // namespace vr
