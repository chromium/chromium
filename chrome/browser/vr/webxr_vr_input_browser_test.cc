// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "device/vr/public/mojom/test/browser_test_interfaces.mojom.h"
#include "device/vr/test/test_hook.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/transform.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrInputTest.java.
// End-to-end tests for user input interaction with WebXR.

namespace vr {

namespace {
const std::vector<std::string>& GetDefaultOpenXrProfiles() {
  static base::NoDestructor<std::vector<std::string>> kDefaultOpenXrProfiles{
      {"microsoft-mixed-reality", "windows-mixed-reality",
       "generic-trigger-squeeze-touchpad-thumbstick"}};

  return *kDefaultOpenXrProfiles;
}
}  // namespace

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
                       uint32_t expected_input_sources,
                       uint32_t expected_gamepads) {
  t->PollJavaScriptBooleanOrFail("inputSourceCount() === " +
                                 base::NumberToString(expected_input_sources));
  t->PollJavaScriptBooleanOrFail("inputSourceWithGamepadCount() === " +
                                 base::NumberToString(expected_gamepads));
}

// Test that focus is locked to the presenting display for the purposes of VR/XR
// input.
void TestPresentationLocksFocusImpl(WebXrVrBrowserTestBase* t,
                                    std::string filename) {
  MockXRDeviceHookBase mock;
  t->LoadFileAndAwaitInitialization(filename);
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepSetupFocusLoss()");
  t->EndTest();
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentationLocksFocus) {
  TestPresentationLocksFocusImpl(t, "webxr_test_presentation_locks_focus");
}

class WebXrControllerInputMock : public MockXRDeviceHookBase {
 public:
  // TODO(crbug.com/41416308): Figure out why waiting for OpenVR to grab
  // the updated state instead of waiting for a number of frames causes frames
  // to be submitted at an extremely slow rate. Once fixed, switch away from
  // waiting on number of frames.
  void UpdateControllerAndWait(
      uint32_t index,
      const device::ControllerFrameData& controller_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    UpdateController(index, controller_data);
    WaitNumFrames(30);
  }

  void ToggleButtonTouches(uint32_t index, uint64_t button_mask) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);

    controller_data.packet_number++;
    controller_data.buttons_touched ^= button_mask;

    UpdateControllerAndWait(index, controller_data);
  }

  void ToggleButtons(uint32_t index, uint64_t button_mask) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);

    controller_data.packet_number++;
    controller_data.buttons_pressed ^= button_mask;
    controller_data.buttons_touched ^= button_mask;
    UpdateControllerAndWait(index, controller_data);
  }

  void ToggleTriggerButton(uint32_t index, device::XrButtonId button_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);
    uint64_t button_mask = device::XrButtonMaskFromId(button_id);

    controller_data.packet_number++;
    controller_data.buttons_pressed ^= button_mask;
    controller_data.buttons_touched ^= button_mask;

    bool is_pressed = ((controller_data.buttons_pressed & button_mask) != 0);

    uint32_t axis_offset = device::XrAxisOffsetFromId(button_id);
    DCHECK(controller_data.axis_data[axis_offset].axis_type ==
           device::XrAxisType::kTrigger);
    controller_data.axis_data[axis_offset].x = is_pressed ? 1.0 : 0.0;
    UpdateControllerAndWait(index, controller_data);
  }

  void SetAxes(uint32_t index, device::XrButtonId button_id, float x, float y) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);
    uint32_t axis_offset = device::XrAxisOffsetFromId(button_id);
    DCHECK(controller_data.axis_data[axis_offset].axis_type != 0);

    controller_data.packet_number++;
    controller_data.axis_data[axis_offset].x = x;
    controller_data.axis_data[axis_offset].y = y;
    UpdateControllerAndWait(index, controller_data);
  }

  void TogglePrimaryTrigger(uint32_t index) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    ToggleTriggerButton(index, device::XrButtonId::kAxisTrigger);
  }

  void PressReleasePrimaryTrigger(uint32_t index) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    TogglePrimaryTrigger(index);
    TogglePrimaryTrigger(index);
  }

  void SetControllerPose(uint32_t index,
                         const gfx::Transform& device_to_origin) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);
    controller_data.pose_data = device_to_origin;
    UpdateControllerAndWait(index, controller_data);
  }

  void AssignDefaultHandData(auto& controller_data,
                             gfx::Quaternion orientation = gfx::Quaternion()) {
    // Stateless helper may be called on any thread.
    gfx::DecomposedTransform decomposed_transform;
    decomposed_transform.quaternion = orientation;
    auto& joint_data = controller_data.hand_data;
    for (uint32_t i = 0; i < std::size(joint_data); i++) {
      decomposed_transform.translate[0] = i / 100.0;
      joint_data[i] = {static_cast<device::mojom::XRHandJoint>(i),
                       gfx::Transform::Compose(decomposed_transform),
                       static_cast<float>(i)};
    }

    controller_data.has_hand_data = true;
  }

  void SetDefaultHandData(uint32_t index,
                          gfx::Quaternion orientation = gfx::Quaternion()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);
    AssignDefaultHandData(controller_data, orientation);
    UpdateControllerAndWait(index, controller_data);
  }

  void ClearHandData(uint32_t index) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(index);
    controller_data.has_hand_data = false;
    UpdateControllerAndWait(index, controller_data);
  }

  uint32_t CreateAndConnectMinimalGamepad(
      device::ControllerRole role =
          device::ControllerRole::kControllerRoleRight) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    // Create a controller that only supports select via a trigger, i.e. it has
    // just enough data to be considered a gamepad.
    uint64_t supported_buttons =
        device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger);

    std::map<device::XrButtonId, uint32_t> axis_types = {
        {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
    };

    return CreateAndConnectController(role, axis_types, supported_buttons);
  }

  uint32_t CreateAndConnectController(
      device::ControllerRole role,
      std::map<device::XrButtonId, uint32_t> axis_types = {},
      uint64_t supported_buttons = UINT64_MAX) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller = CreateValidController(role);
    controller.supported_buttons = supported_buttons;
    for (const auto& axis_type : axis_types) {
      uint32_t axis_offset = device::XrAxisOffsetFromId(axis_type.first);
      controller.axis_data[axis_offset].axis_type = axis_type.second;
    }

    return ConnectController(controller);
  }

  void UpdateControllerSupport(
      uint32_t controller_index,
      const std::map<device::XrButtonId, uint32_t>& axis_types,
      uint64_t supported_buttons) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(controller_index);

    for (uint32_t i = 0; i < device::kMaxNumAxes; i++) {
      auto button_id = GetAxisId(i);
      auto it = axis_types.find(button_id);
      uint32_t new_axis_type = device::XrAxisType::kNone;
      if (it != axis_types.end())
        new_axis_type = it->second;
      controller_data.axis_data[i].axis_type = new_axis_type;
    }

    controller_data.supported_buttons = supported_buttons;

    UpdateControllerAndWait(controller_index, controller_data);
  }

  void UpdateControllerRole(uint32_t controller_index,
                            device::ControllerRole role) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    auto controller_data = GetCurrentControllerData(controller_index);
    controller_data.role = role;
    UpdateControllerAndWait(controller_index, controller_data);
  }

  void UpdateInteractionProfile(
      device::mojom::OpenXrInteractionProfileType new_profile) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    device_test::mojom::EventData data = {};
    data.type = device_test::mojom::EventType::kInteractionProfileChanged;
    data.interaction_profile = new_profile;
    PopulateEvent(std::move(data));
  }

  // A controller is necessary to simulate voice input because of how the test
  // API works.
  uint32_t CreateVoiceController() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    return CreateAndConnectMinimalGamepad(
        device::ControllerRole::kControllerRoleVoice);
  }

 private:
  // kAxisTrackpad is the first entry in XrButtonId that maps to an axis and the
  // subsequent entries are also for input axes.
  device::XrButtonId GetAxisId(uint32_t offset) {
    // Stateless helper may be called on any thread.
    return static_cast<device::XrButtonId>(device::XrButtonId::kAxisTrackpad +
                                           offset);
  }

  device::ControllerFrameData GetCurrentControllerData(uint32_t index) {
    // Getter may be called on any thread.
    base::AutoLock lock(lock_);
    auto iter = controller_data_map_.find(index);
    CHECK(iter != controller_data_map_.end());
    return iter->second;
  }
};

// Ensure that when an input source's handedness changes, an input source change
// event is fired and a new input source is created.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInputHandednessChange) {
  WebXrControllerInputMock my_mock;
  uint32_t controller_index = my_mock.CreateAndConnectMinimalGamepad();

  t->LoadFileAndAwaitInitialization("test_webxr_input_same_object");
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

  // TODO(crbug.com/41459138): Figure out if the race is a product or test bug.
  // There's a potential for a race causing the input sources change event to
  // fire multiple times if we disconnect a controller that has a gamepad.
  // Even just a select trigger is sufficient to have an xr-standard mapping, so
  // just expose a grip trigger instead so that we don't connect a gamepad.
  uint64_t insufficient_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);
  std::map<device::XrButtonId, uint32_t> insufficient_axis_types = {};
  uint32_t controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, insufficient_axis_types,
      insufficient_buttons);

  t->LoadFileAndAwaitInitialization("test_webxr_input_sources_change_event");
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

  // At least currently, there is no way for OpenXR to have insufficient
  // buttons for a gamepad as long as a controller is connected, so skip this
  // part on OpenXR since it'll always fail
  if (t->GetRuntimeType() != XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
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

// Ensure that if a Gamepad has the minimum required number of axes/buttons to
// be considered an xr-standard Gamepad, that it is exposed as such, and that
// we can check the state of it's priamry axes/button.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestGamepadMinimumData) {
  WebXrControllerInputMock my_mock;

  uint32_t controller_index = my_mock.CreateAndConnectMinimalGamepad();

  t->LoadFileAndAwaitInitialization("test_webxr_gamepad_support");
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 1, 1);

  // We only actually connect the data for the one button, but OpenXR
  // expects the OpenXR  controller (which has all of the required and
  // optional buttons) and so adds dummy/placeholder buttons regardless of what
  // data we send up.
  std::string button_count = "1";
  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR)
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

  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    VerifyInputSourceProfilesArray(t, GetDefaultOpenXrProfiles());
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Make sure the input gets plumbed to the correct gamepad, including when
// button presses are interleaved.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestMultipleGamepads) {
  WebXrControllerInputMock my_mock;

  uint32_t controller_index1 = my_mock.CreateAndConnectMinimalGamepad(
      device::ControllerRole::kControllerRoleLeft);
  uint32_t controller_index2 = my_mock.CreateAndConnectMinimalGamepad();

  t->LoadFileAndAwaitInitialization("test_webxr_gamepad_support");
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 2, 2);

  // We only actually connect the data for the one button, but OpenXR
  // expects the OpenXR controller (which has all of the required and
  // optional buttons) and so adds dummy/placeholder buttons regardless of what
  // data we send up.
  std::string button_count = "1";
  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR)
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

  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    VerifyInputSourceProfilesArray(t, GetDefaultOpenXrProfiles());
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

  std::map<device::XrButtonId, uint32_t> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
      {device::XrButtonId::kAxisThumbstick, device::XrAxisType::kJoystick},
  };

  uint32_t controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  t->LoadFileAndAwaitInitialization("test_webxr_gamepad_support");
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

  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    VerifyInputSourceProfilesArray(t, GetDefaultOpenXrProfiles());
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Ensure that if OpenXR Runtime receive interaction profile changes event,
// input profile name will be changed accordingly.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInteractionProfileChanged) {
  WebXrControllerInputMock my_mock;

  // Create a controller that supports all reserved buttons.
  uint64_t supported_buttons =
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad) |
      device::XrButtonMaskFromId(device::XrButtonId::kAxisThumbstick) |
      device::XrButtonMaskFromId(device::XrButtonId::kGrip);

  std::map<device::XrButtonId, uint32_t> axis_types = {
      {device::XrButtonId::kAxisTrackpad, device::XrAxisType::kTrackpad},
      {device::XrButtonId::kAxisTrigger, device::XrAxisType::kTrigger},
      {device::XrButtonId::kAxisThumbstick, device::XrAxisType::kJoystick},
  };

  my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  t->LoadFileAndAwaitInitialization("test_webxr_input_same_object");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // We only expect one input source, cache it.
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Simulate the runtime sending an interaction profile change event to change
  // from Windows motion controller to Khronos simple Controller.
  my_mock.UpdateInteractionProfile(
      device::mojom::OpenXrInteractionProfileType::kKHRSimple);
  // Make sure change events happens again since interaction profile changed
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 2",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("validateCachedSourcePresence(false)");

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Set up an initial constant and some compile time validations for it.
constexpr device::mojom::OpenXrInteractionProfileType
    kInitialInteractionProfile =
        device::mojom::OpenXrInteractionProfileType::kMinValue;

// If intentionally changing `Invalid` to be the 0th profile, please update the
// assignment above.
static_assert(kInitialInteractionProfile !=
                  device::mojom::OpenXrInteractionProfileType::kInvalid,
              "TestAllKnownInteractionProfileTypes expects the 0th profile in "
              "OpenXrInteractionProfileType to be valid.");

// A list of interaction profiles that should be skipped by the below test. Each
// profile must have a comment indicating why it is skipped.
constexpr device::mojom::OpenXrInteractionProfileType
    kSkippedInteractionProfiles[] = {
        // The "Invalid" entry is not a real profile.
        device::mojom::OpenXrInteractionProfileType::kInvalid,
        // kMetaHandAim is a "synthetic" interaction profile type which is
        // synthesized via it's own set of extension methods and needs to use a
        // different mechanism to send button clicks rather than the rest of the
        // methods.
        device::mojom::OpenXrInteractionProfileType::kMetaHandAim,
};

void TestHandProfiles(WebXrVrBrowserTestBase* t, bool joint_support) {
  WebXrControllerInputMock my_mock;
  my_mock.UpdateInteractionProfile(
      device::mojom::OpenXrInteractionProfileType::kExtHand);
  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  my_mock.ConnectController(controller_data);

  t->LoadFileAndAwaitInitialization("test_webxr_profiles");
  if (joint_support) {
    t->RunJavaScriptOrFail("setupImmersiveSessionToRequestHands()");
  }

  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  std::string expected_string =
      joint_support ? "generic-hand" : "generic-fixed-hand";
  std::string unexpected_string =
      joint_support ? "generic-fixed-hand" : "generic-hand";

  t->RunJavaScriptOrFail("validateAllInputSourcesContainProfile('" +
                         expected_string + "')");
  t->RunJavaScriptOrFail("validateNoInputSourcesContainProfile('" +
                         unexpected_string + "')");
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestProfilesHandJoint) {
  TestHandProfiles(this, true);
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestProfilesFixedHand) {
  TestHandProfiles(this, false);
}

// Ensure that OpenXR can change between all known Interaction Profile types.
// If you're adding a new interaction profile, you may need to validate that
// openxr_test_helper has any required extensions listed as supported in it's
// header and that it knows about all of the buttons/input types that you're
// adding with the new interaction profile.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestAllKnownInteractionProfileTypes) {
  WebXrControllerInputMock my_mock;
  my_mock.UpdateInteractionProfile(kInitialInteractionProfile);
  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  my_mock.ConnectController(controller_data);

  t->LoadFileAndAwaitInitialization("test_webxr_input_sources_change_event");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  uint32_t expected_change_events = 1;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Note that since we explicitly set ourselves to the 0th value above, we want
  // to start changing to the first item in the enum.
  static uint32_t kFinalValue = static_cast<uint32_t>(
      device::mojom::OpenXrInteractionProfileType::kMaxValue);
  static uint32_t kFirstChangedProfileIndex =
      static_cast<uint32_t>(kInitialInteractionProfile) + 1;
  for (uint32_t i = kFirstChangedProfileIndex; i <= kFinalValue; i++) {
    auto profile = static_cast<device::mojom::OpenXrInteractionProfileType>(i);
    if (base::Contains(kSkippedInteractionProfiles, profile)) {
      continue;
    }
    my_mock.UpdateInteractionProfile(profile);
    expected_change_events++;
    // Make sure change events happens again since interaction profile changed
    t->PollJavaScriptBooleanOrFail(
        "inputChangeEvents === " + base::NumberToString(expected_change_events),
        WebXrVrBrowserTestBase::kPollTimeoutShort);
  }

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Test that when a session is blurred, input sources are removed, and trying
// to get a pose from a cached input source throws an exception.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInputNotVisibleWhenBlurred) {
  WebXrControllerInputMock my_mock;

  // Connect a controller.
  my_mock.CreateAndConnectMinimalGamepad();

  // Load the test page and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_input_visibility");
  t->EnterSessionWithUserGestureOrFail();

  // Check that the input source is visible and the visibility state is
  // 'visible'.
  t->PollJavaScriptBooleanOrFail("checkVisibilityState('visible')");
  my_mock.WaitNumFrames(1);
  t->RunJavaScriptOrFail("validateInputSourceVisible()");

  // Blur the session.
  device_test::mojom::EventData event_data = {};
  event_data.type = device_test::mojom::EventType::kVisibilityVisibleBlurred;
  my_mock.PopulateEvent(event_data);

  // Check that the input source is no longer visible and the visibility state
  // is 'visible-blurred'.
  t->PollJavaScriptBooleanOrFail("checkVisibilityState('visible-blurred')");
  t->PollJavaScriptBooleanOrFail("checkInputSourceCount(0)");

  // Validate that querying poses from the cached controller are null.
  t->RunJavaScriptOrFail("validateNullInputPoses()");
  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Test that controller input is registered via WebXR's input method. This uses
// multiple controllers to make sure the input is going to the correct one.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestMultipleControllerInputRegistered) {
  WebXrControllerInputMock my_mock;

  uint32_t controller_index1 = my_mock.CreateAndConnectMinimalGamepad(
      device::ControllerRole::kControllerRoleLeft);
  uint32_t controller_index2 = my_mock.CreateAndConnectMinimalGamepad();

  // Load the test page and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_input");
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

// Test that controller input is registered via WebXR's input method.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream_WebXr.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestControllerInputRegistered) {
  WebXrControllerInputMock my_mock;

  uint32_t controller_index = my_mock.CreateAndConnectMinimalGamepad();

  // Load the test page and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_input");
  t->EnterSessionWithUserGestureOrFail();

  uint32_t num_iterations = 5;
  t->RunJavaScriptOrFail("stepSetupListeners(" +
                         base::NumberToString(num_iterations) + ")");

  // Press and unpress the controller's trigger a bunch of times and make sure
  // they're all registered.
  for (uint32_t i = 0; i < num_iterations; ++i) {
    my_mock.PressReleasePrimaryTrigger(controller_index);
    // After each trigger release, wait for the JavaScript to receive the
    // "select" event.
    t->WaitOnJavaScriptStep();
  }
  t->EndTest();
}

std::string TransformToColMajorString(const gfx::Transform& t) {
  std::array<float, 16> array;
  t.GetColMajorF(array);
  std::string array_string = "[";
  for (const auto& val : array) {
    array_string += base::NumberToString(val) + ",";
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
  uint32_t controller_index = my_mock.ConnectController(controller_data);

  t->LoadFileAndAwaitInitialization("webxr_test_controller_poses");
  t->EnterSessionWithUserGestureOrFail();

  auto pose = gfx::Transform();
  pose.RotateAboutXAxis(90);
  pose.RotateAboutYAxis(45);
  pose.RotateAboutZAxis(180);
  pose.Translate3d(0.5f, 2, -3);
  my_mock.SetControllerPose(controller_index, pose);

  // Apply any offset we expect the runtime to add.
  pose.Translate3d(t->GetControllerOffset());

  t->ExecuteStepAndWait("stepWaitForMatchingPose(" +
                        base::NumberToString(controller_index) + ", " +
                        TransformToColMajorString(pose) + ")");
  t->AssertNoJavaScriptErrors();
}

// Test that the `hand` property on the Input Source remains null, even if the
// runtime reports it, without the appropriate feature request.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHandDataNotVisibleWithoutFeature) {
  WebXrControllerInputMock my_mock;

  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  my_mock.AssignDefaultHandData(controller_data);

  my_mock.ConnectController(controller_data);

  t->LoadFileAndAwaitInitialization("test_webxr_hand_tracking");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  uint32_t expected_change_events = 1;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandTrackingFeatureState(false)");
  t->RunJavaScriptOrFail("assertHandsNotPresent()");
  t->AssertNoJavaScriptErrors();

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Test that the `hand` property on the Input Source is not null, if the
// runtime reports it, with the appropriate feature request.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHandDataVisibleWithFeature) {
  WebXrControllerInputMock my_mock;

  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  my_mock.AssignDefaultHandData(controller_data);

  my_mock.ConnectController(controller_data);

  t->LoadFileAndAwaitInitialization("test_webxr_hand_tracking");
  t->RunJavaScriptOrFail("setupRequestHandTracking()");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  uint32_t expected_change_events = 1;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandTrackingFeatureState(true)");
  t->RunJavaScriptOrFail("assertHandsPresent()");
  t->AssertNoJavaScriptErrors();

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Test that the `hand` property on the Input Source is null when hand data
// cannot be provided, with the appropriate feature request.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHandDataVisibleToggle) {
  WebXrControllerInputMock my_mock;

  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);

  uint32_t index = my_mock.ConnectController(controller_data);

  t->LoadFileAndAwaitInitialization("test_webxr_hand_tracking");
  t->RunJavaScriptOrFail("setupRequestHandTracking()");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  uint32_t expected_change_events = 1;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandTrackingFeatureState(true)");
  t->RunJavaScriptOrFail("assertHandsNotPresent()");

  // Add hand data, it should now be visible.
  my_mock.SetDefaultHandData(index);
  expected_change_events++;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandsPresent()");

  // Remove hand data, it should no longer be visible.
  my_mock.ClearHandData(index);
  expected_change_events++;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandsNotPresent()");

  t->AssertNoJavaScriptErrors();

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

class WebXrHeadPoseMock : public MockXRDeviceHookBase {
 public:
  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
    std::optional<gfx::Transform> pose;
    {
      base::AutoLock lock(pose_lock);
      pose = pose_;
    }
    std::move(callback).Run(std::move(pose));
  }

  void SetHeadPose(const gfx::Transform& pose) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    base::AutoLock lock(pose_lock);
    pose_ = pose;
  }

 private:
  base::Lock pose_lock;
  gfx::Transform pose_ GUARDED_BY(pose_lock);
};

// Test that head pose changes are properly reflected in the viewer pose
// provided by WebXR.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHeadPosesUpdate) {
  WebXrHeadPoseMock my_mock;

  t->LoadFileAndAwaitInitialization("webxr_test_head_poses");
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
