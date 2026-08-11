// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/mock_xr_input_source.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "device/vr/test/webxr_test_gamepad_utils.h"
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

// Ensure that when an input source's handedness changes, an input source change
// event is fired and a new input source is created.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInputHandednessChange) {
  MockXRDeviceHookBase my_mock;
  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

  t->LoadFileAndAwaitInitialization("test_webxr_input_same_object");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // We only expect one input source, cache it.
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Change the handedness from right to left and verify that we get a change
  // event. Then cache the new input source.
  controller.SetHandedness(device::mojom::XRHandedness::LEFT);
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 2",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Switch back to the right hand and confirm that we get the change.
  controller.SetHandedness(device::mojom::XRHandedness::RIGHT);
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
// containing both the removal of the old one and the addition of the new one,
// rather than two events.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestInputSourcesChange) {
  MockXRDeviceHookBase my_mock;

  // Start with a controller without a gamepad.
  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);
  controller.ClearGamepad();

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
  controller.SetConnected(false);
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 2",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("validateAdded(0)");
  t->RunJavaScriptOrFail("validateRemoved(1)");
  t->RunJavaScriptOrFail("validateCachedRemovedPresence(true)");

  // Connect a controller, and then change enough properties that the system
  // recalculates its status as a valid controller, so that we can verify
  // it is both added and removed.
  auto& controller2 =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 3",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // At least currently, there is no way for OpenXR to have insufficient
  // buttons for a gamepad as long as a controller is connected, so skip this
  // part on OpenXR since it'll always fail
  if (t->GetRuntimeType() != XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    controller2.ClearGamepad();

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
// we can check the state of it's primary axes/button.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestGamepadMinimumData) {
  MockXRDeviceHookBase my_mock;

  auto& controller = my_mock.CreateMinimalGamepad();

  t->LoadFileAndAwaitInitialization("test_webxr_gamepad_support");
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 1, 1);

  // We only actually connect the data for the one button, but OpenXR
  // expects the OpenXR controller (which has all of the required and
  // optional buttons) and so adds dummy/placeholder buttons regardless of what
  // data we send up.
  std::string button_count = "1";
  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    button_count = "4";
  }

  t->PollJavaScriptBooleanOrFail("isButtonCountEqualTo(" + button_count + ")",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // Press the trigger.
  controller.PressTrigger();
  my_mock.WaitNumFrames(5);

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
  MockXRDeviceHookBase my_mock;

  auto& controller1 =
      my_mock.CreateMinimalGamepad(device::mojom::XRHandedness::LEFT);
  auto& controller2 =
      my_mock.CreateMinimalGamepad(device::mojom::XRHandedness::RIGHT);

  t->LoadFileAndAwaitInitialization("test_webxr_gamepad_support");
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 2, 2);

  // We only actually connect the data for the one button, but OpenXR
  // expects the OpenXR controller (which has all of the required and
  // optional buttons) and so adds dummy/placeholder buttons regardless of what
  // data we send up.
  std::string button_count = "1";
  if (t->GetRuntimeType() == XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR) {
    button_count = "4";
  }

  // Make sure both gamepads have the expected button count and mapping.
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("isButtonCountEqualTo(" +
                                                   button_count + ", 0)"));
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("isButtonCountEqualTo(" +
                                                   button_count + ", 1)"));
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
      "isMappingEqualTo('xr-standard', 0)"));
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
      "isMappingEqualTo('xr-standard', 1)"));

  // Press the trigger on the first gamepad.
  controller1.PressTrigger();

  // The trigger should be button 0. Make sure it is only pressed on the first
  // gamepad.
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true, 0)");
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, false, 1)");

  // Now press the other gamepad's button and make sure it's registered.
  controller2.PressTrigger();
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true, 1)");

  // Then release the second trigger. The second gamepad's button should no
  // longer be pressed, but the first gamepad's button should still be pressed
  // because we haven't released that trigger yet.
  controller2.ReleaseTrigger();
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, false, 1)");
  t->PollJavaScriptBooleanOrFail("isButtonPressedEqualTo(0, true, 0)");

  // Finally, release the trigger on the first gamepad.
  controller1.ReleaseTrigger();
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
  MockXRDeviceHookBase my_mock;

  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);
  const std::vector<device::XrButtonId> supported_buttons = {
      device::XrButtonId::kAxisTrigger, device::XrButtonId::kAxisTrackpad,
      device::XrButtonId::kAxisThumbstick, device::XrButtonId::kGrip};
  controller.SetSupportedButtons(supported_buttons);

  t->LoadFileAndAwaitInitialization("test_webxr_gamepad_support");
  t->EnterSessionWithUserGestureOrFail();

  VerifyInputCounts(t, 1, 1);

  // Setup some state on the optional buttons (as TestGamepadMinimumData should
  // ensure proper state on the required buttons).
  // Set a value on the touchpad (touched but not pressed).
  controller.SetAxis(device::XrButtonId::kAxisTrackpad, 0.25f, -0.25f);
  controller.SetButton(device::XrButtonId::kAxisTrackpad, /*pressed=*/false,
                       /*touched=*/true, /*value=*/0.0);

  // Also test the thumbstick.
  controller.SetAxis(device::XrButtonId::kAxisThumbstick, 0.67f, -0.67f);
  controller.PressButton(device::XrButtonId::kAxisThumbstick);

  // Set the grip button to be pressed.
  controller.PressButton(device::XrButtonId::kGrip);

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
  MockXRDeviceHookBase my_mock;
  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);
  const std::vector<device::XrButtonId> supported_buttons = {
      device::XrButtonId::kAxisTrigger, device::XrButtonId::kAxisTrackpad,
      device::XrButtonId::kAxisThumbstick, device::XrButtonId::kGrip};
  controller.SetSupportedButtons(supported_buttons);

  t->LoadFileAndAwaitInitialization("test_webxr_input_same_object");
  t->EnterSessionWithUserGestureOrFail();

  // We should only have seen the first change indicating we have input sources.
  t->PollJavaScriptBooleanOrFail("inputChangeEvents === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutShort);

  // We only expect one input source, cache it.
  t->RunJavaScriptOrFail("validateInputSourceLength(1)");
  t->RunJavaScriptOrFail("updateCachedInputSource(0)");

  // In OpenXR, interaction profile changes are session-level events
  // (dispatched via XrEventDataInteractionProfileChanged) rather than
  // per-input-source, so they are simulated on the mock device hook.
  my_mock.SimulateInteractionProfileChanged(
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
  MockXRDeviceHookBase my_mock;
  my_mock.SimulateInteractionProfileChanged(
      device::mojom::OpenXrInteractionProfileType::kExtHand);
  my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

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
  MockXRDeviceHookBase my_mock;
  my_mock.SimulateInteractionProfileChanged(kInitialInteractionProfile);
  my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

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
    if (std::ranges::contains(kSkippedInteractionProfiles, profile)) {
      continue;
    }
    my_mock.SimulateInteractionProfileChanged(profile);
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
  MockXRDeviceHookBase my_mock;

  // Connect a controller.
  my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

  // Load the test page and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_input_visibility");
  t->EnterSessionWithUserGestureOrFail();

  // Check that the input source is visible and the visibility state is
  // 'visible'.
  t->PollJavaScriptBooleanOrFail("checkVisibilityState('visible')");
  my_mock.WaitNumFrames(1);
  t->RunJavaScriptOrFail("validateInputSourceVisible()");

  // Blur the session.
  my_mock.SimulateVisibilityBlurred();

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
  MockXRDeviceHookBase my_mock;

  auto& controller1 =
      my_mock.CreateInputSource(device::mojom::XRHandedness::LEFT);
  auto& controller2 =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

  // Load the test page and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_input");
  t->EnterSessionWithUserGestureOrFail();

  t->RunJavaScriptOrFail("stepSetupListeners(2)");

  // Press and release the first controller's trigger and make sure the select
  // events are registered for it. After trigger release, must wait for JS to
  // receive the "select" event.
  t->RunJavaScriptOrFail("expectedInputSourceIndex = 0");
  controller1.PressReleaseTrigger();
  t->WaitOnJavaScriptStep();

  // Do the same thing for the other controller.
  t->RunJavaScriptOrFail("expectedInputSourceIndex = 1");
  controller2.PressReleaseTrigger();
  t->WaitOnJavaScriptStep();

  t->EndTest();
}

// Test that controller input is registered via WebXR's input method.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream_WebXr.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestControllerInputRegistered) {
  MockXRDeviceHookBase my_mock;

  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

  // Load the test page and enter presentation.
  t->LoadFileAndAwaitInitialization("test_webxr_input");
  t->EnterSessionWithUserGestureOrFail();

  uint32_t num_iterations = 5;
  t->RunJavaScriptOrFail("stepSetupListeners(" +
                         base::NumberToString(num_iterations) + ")");

  // Press and release the controller's trigger a bunch of times and make sure
  // they're all registered.
  for (uint32_t i = 0; i < num_iterations; ++i) {
    controller.PressReleaseTrigger();
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
  MockXRDeviceHookBase my_mock;

  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

  t->LoadFileAndAwaitInitialization("webxr_test_controller_poses");
  t->EnterSessionWithUserGestureOrFail();

  auto pose = gfx::Transform();
  pose.RotateAboutXAxis(90);
  pose.RotateAboutYAxis(45);
  pose.RotateAboutZAxis(180);
  pose.Translate3d(0.5f, 2, -3);
  controller.SetPose(pose);

  // Apply any offset we expect the runtime to add.
  pose.Translate3d(t->GetControllerOffset());

  t->ExecuteStepAndWait("stepWaitForMatchingPose(" +
                        TransformToColMajorString(pose) + ")");
  t->AssertNoJavaScriptErrors();
}

// Test that the `hand` property on the Input Source remains null, even if the
// runtime reports it, without the appropriate feature request.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHandDataNotVisibleWithoutFeature) {
  MockXRDeviceHookBase my_mock;

  my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT,
                            /*has_hand_tracking=*/true);

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
  MockXRDeviceHookBase my_mock;

  my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT,
                            /*has_hand_tracking=*/true);

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
  MockXRDeviceHookBase my_mock;

  auto& controller =
      my_mock.CreateInputSource(device::mojom::XRHandedness::RIGHT);

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
  controller.SetDefaultHandData();
  expected_change_events++;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandsPresent()");

  // Remove hand data, it should no longer be visible.
  controller.ClearHandData();
  expected_change_events++;
  t->PollJavaScriptBooleanOrFail(
      "inputChangeEvents === " + base::NumberToString(expected_change_events),
      WebXrVrBrowserTestBase::kPollTimeoutShort);

  t->RunJavaScriptOrFail("assertHandsNotPresent()");

  t->AssertNoJavaScriptErrors();

  t->RunJavaScriptOrFail("done()");
  t->EndTest();
}

// Test that head pose changes are properly reflected in the viewer pose
// provided by WebXR.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestHeadPosesUpdate) {
  MockXRDeviceHookBase my_mock;

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
