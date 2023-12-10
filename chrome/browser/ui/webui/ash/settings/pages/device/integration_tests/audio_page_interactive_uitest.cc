// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>

#include "ash/ash_element_identifiers.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

namespace {

class ActiveAudioNodeStateObserver;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsElementId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ActiveAudioNodeStateObserver,
                                    kActiveInputNodeState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ActiveAudioNodeStateObserver,
                                    kActiveOutputNodeState);

// Screenplay ID for Audio settings launch.
constexpr char kAudioSettingsFeatureIdTag[] =
    "screenplay-816eefa8-76ad-43ec-8300-c747f4b59987";

// Element path to Audio settings page.
constexpr char kOsSettingsUiSelector[] = "os-settings-ui";
constexpr char kOsSettingsMainSelector[] = "os-settings-main";
constexpr char kOsSettingsMainPageContainerSelector[] = "main-page-container";
constexpr char kOsSettingsDevicePageSelector[] = "settings-device-page";
constexpr char kOsSettingsDeviceAudioPageSelector[] = "settings-audio";

// Audio settings page elements.
constexpr char kOutputDeviceDropdownSelector[] = "#audioOutputDeviceDropdown";
constexpr char kOutputMuteSelector[] = "#audioOutputMuteButton";
constexpr char kOutputSliderSelector[] = "#outputVolumeSlider";
constexpr char kInputDeviceDropdownSelector[] = "#audioInputDeviceDropdown";
constexpr char kInputMuteSelector[] = "#audioInputGainMuteButton";
constexpr char kInputSliderSelector[] = "#audioInputGainVolumeSlider";

// ActiveAudioNodeStateObserver tracks when primary input or output device
// changes. Returns state change with primary active device ID depending for
//  input or output on the value of the `is_input` parameter.
class ActiveAudioNodeStateObserver : public ui::test::ObservationStateObserver<
                                         uint64_t,
                                         ash::CrasAudioHandler,
                                         ash::CrasAudioHandler::AudioObserver> {
 public:
  ActiveAudioNodeStateObserver(ash::CrasAudioHandler* handler, bool is_input)
      : ObservationStateObserver(handler), is_input_(is_input) {}

  ~ActiveAudioNodeStateObserver() override = default;

  // ObservationStateObserver:
  uint64_t GetStateObserverInitialState() const override {
    return GetActiveNode();
  }

  // ash::CrasAudioHandler::AudioObserver:
  void OnActiveInputNodeChanged() override {
    if (is_input_) {
      OnStateObserverStateChanged(GetActiveNode());
    }
  }

  void OnActiveOutputNodeChanged() override {
    if (!is_input_) {
      OnStateObserverStateChanged(GetActiveNode());
    }
  }

 private:
  uint64_t GetActiveNode() const {
    DCHECK(source());
    return is_input_ ? source()->GetPrimaryActiveInputNode()
                     : source()->GetPrimaryActiveOutputNode();
  }

  const bool is_input_;
};

// Construct DeepQuery which pierces the shadowRoots required to access Audio
// settings page elements. `selector` param is the element within the settings
// page being accessed. Assumption is selector will exist within the
// "audio-settings" shadowRoot.
InteractiveAshTest::DeepQuery CreateAudioPageDeepQueryForSelector(
    const std::string& selector) {
  return InteractiveAshTest::DeepQuery{
      kOsSettingsUiSelector,
      kOsSettingsMainSelector,
      kOsSettingsMainPageContainerSelector,
      kOsSettingsDevicePageSelector,
      kOsSettingsDeviceAudioPageSelector,
      selector,
  };
}

// AudioSettingsInteractiveUiTest configures test environment and provides
// helper code for verifying interactive behavior of Audio settings page
// (chrome://os-settings/audio). InteractiveAshTest environment configures fake
// CrasAudioClient thus it does not need to be initialized or shutdown during
// setup and teardown functions.
class AudioSettingsInteractiveUiTest : public InteractiveAshTest {
 public:
  AudioSettingsInteractiveUiTest() = default;

  AudioSettingsInteractiveUiTest(const AudioSettingsInteractiveUiTest&) =
      delete;
  AudioSettingsInteractiveUiTest& operator=(
      const AudioSettingsInteractiveUiTest&) = delete;

  ~AudioSettingsInteractiveUiTest() override = default;

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    audio_handler_ = ash::CrasAudioHandler::Get();
  }

  void TearDownOnMainThread() override {
    audio_handler_ = nullptr;

    InteractiveAshTest::TearDownOnMainThread();
  }

  // Set active input or output device using CrasAudioHandler::SwitchToDevice.
  auto DoSetActiveDevice(uint64_t device_id) {
    return Steps(Do([this, device_id]() {
      DCHECK(audio_handler());
      const AudioDevice* audio_device =
          audio_handler()->GetDeviceFromId(device_id);
      DCHECK(audio_device);
      audio_handler()->SwitchToDevice(
          *audio_device, /*notify=*/true,
          CrasAudioHandler::DeviceActivateType::ACTIVATE_BY_USER);
    }));
  }

  // Ensure browser opened to Audio settings page.
  auto LoadAudioSettingsPage() {
    const InteractiveAshTest::DeepQuery kPathToAudioSettings = {
        kOsSettingsUiSelector,
        kOsSettingsMainSelector,
        kOsSettingsMainPageContainerSelector,
        kOsSettingsDevicePageSelector,
        kOsSettingsDeviceAudioPageSelector,
    };

    return Steps(
        Log("Open OS Settings to Audio Page"),
        InstrumentNextTab(kOsSettingsElementId, AnyBrowser()), Do([&]() {
          chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              GetActiveUserProfile(),
              chromeos::settings::mojom::kAudioSubpagePath);
        }),
        WaitForShow(kOsSettingsElementId),

        Log("Waiting for OS settings audio settings page to load"),
        WaitForWebContentsReady(
            kOsSettingsElementId,
            chrome::GetOSSettingsUrl(
                chromeos::settings::mojom::kAudioSubpagePath)),

        Log("Check for audio settings exists"),
        WaitForElementExists(kOsSettingsElementId, kPathToAudioSettings));
  }

  auto MaybeWaitForInputDevice(const uint64_t device_id) {
    DCHECK(audio_handler());
    const uint64_t primary_node = audio_handler()->GetPrimaryActiveInputNode();

    return Steps(
        If([primary_node, device_id]() { return primary_node != device_id; },
           Steps(Log(std::format(
                     "Waiting for primary input device to match node ID: {0}",
                     device_id)),
                 WaitForState(kActiveInputNodeState, device_id))));
  }

  auto MaybeWaitForOutputDevice(const uint64_t device_id) {
    DCHECK(audio_handler());
    const uint64_t primary_node = audio_handler()->GetPrimaryActiveOutputNode();

    return Steps(
        If([primary_node, device_id]() { return primary_node != device_id; },
           Steps(Log(std::format(
                     "Waiting for primary output device to match node ID: {0}",
                     device_id)),
                 WaitForState(kActiveOutputNodeState, device_id))));
  }

  // Wait for an element described by `selector` to exists. Valid selector
  // restrictions come from `CreateAudioPageDeepQueryForSelector`.
  auto WaitForAudioElementExists(const std::string& selector) {
    return Steps(
        Log(std::format("Wait for {0}", selector)),
        WaitForElementExists(kOsSettingsElementId,
                             CreateAudioPageDeepQueryForSelector(selector)));
  }

  CrasAudioHandler* audio_handler() const { return audio_handler_; }

 private:
  raw_ptr<CrasAudioHandler> audio_handler_ = nullptr;
};

// Verify audio settings page displays and renders expected layout given the
// chrome://os-settings/audio page is open and active output and input
// devices exist.
IN_PROC_BROWSER_TEST_F(AudioSettingsInteractiveUiTest, RenderAudioPage) {
  base::AddFeatureIdTagToTestResult(kAudioSettingsFeatureIdTag);
  SetupContextWidget();

  // Output device `output_1`'s ID configured here:
  // chromeos/ash/components/dbus/audio/fake_cras_audio_client.cc.
  const uint64_t expected_active_output_node = 0x100000001;
  EXPECT_TRUE(audio_handler()->GetDeviceFromId(expected_active_output_node));

  // Input device `input_1`'s ID configured here:
  // chromeos/ash/components/dbus/audio/fake_cras_audio_client.cc.
  const uint64_t expected_active_input_node = 0x100000002;
  EXPECT_TRUE(audio_handler()->GetDeviceFromId(expected_active_input_node));

  RunTestSequence(
      Log("Setup state observers"),
      // Setup state observers.
      ObserveState(kActiveOutputNodeState,
                   std::make_unique<ActiveAudioNodeStateObserver>(
                       audio_handler(), /*is_input=*/false)),
      ObserveState(kActiveInputNodeState,
                   std::make_unique<ActiveAudioNodeStateObserver>(
                       audio_handler(), /*is_input=*/true)),

      // Set fake internal speaker as active output device and wait for state
      // update to ensure output controls are displayed on audio settings page.
      DoSetActiveDevice(expected_active_output_node),
      MaybeWaitForOutputDevice(expected_active_output_node),
      Log("Expected primary output device configured"),

      // Set fake internal mic as active input device and wait for state update
      // to ensure input controls are displayed on audio settings page.
      DoSetActiveDevice(expected_active_input_node),
      MaybeWaitForInputDevice(expected_active_input_node),
      Log("Expected primary input device configured"),

      // Open audio settings page
      LoadAudioSettingsPage(),

      // Test that output controls exist.
      WaitForAudioElementExists(kOutputDeviceDropdownSelector),
      WaitForAudioElementExists(kOutputMuteSelector),
      WaitForAudioElementExists(kOutputSliderSelector),
      Log("Expected output controls exist"),

      // Test that input controls exist.
      WaitForAudioElementExists(kInputDeviceDropdownSelector),
      WaitForAudioElementExists(kInputMuteSelector),
      WaitForAudioElementExists(kInputSliderSelector),
      Log("Expected input controls exist"));
}

}  // namespace

}  // namespace ash
