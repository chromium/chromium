// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_element_identifiers.h"
#include "ash/system/network/managed_sim_lock_notifier.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"
#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                    kMobileDataPoweredState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                    kCellularServiceConnected);

constexpr char kCorrectPin1[] = "1111";
constexpr char kCorrectPin2[] = "2222";
constexpr char kIncorrectPin1[] = "3333";
constexpr char kIncorrectPin2[] = "4444";
constexpr char kIncorrectPin3[] = "5555";

class SimLockInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    ConfigureEsimProfile(euicc_info(), *esim_info_, /*connected=*/true);

    auto* device_test = ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test);
    device_path_ = device_test->GetDevicePathForType(shill::kTypeCellular);

    SetSimLockState(/*type=*/"", FakeShillDeviceClient::kSimPinRetryCount,
                    /*lock_enabled=*/false);
  }

  void SetSimLockState(const std::string& type,
                       const int retries_left,
                       const bool lock_enabled) {
    auto* device_test = ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test);
    device_test->SetDeviceProperty(
        device_path_, shill::kSIMLockStatusProperty,
        base::Value(base::Value::Dict()
                        .Set(shill::kSIMLockTypeProperty, base::Value(type))
                        .Set(shill::kSIMLockRetriesLeftProperty,
                             base::Value(retries_left))
                        .Set(shill::kSIMLockEnabledProperty,
                             base::Value(lock_enabled))),
        /*notify_changed=*/true);
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep CheckSimLockState(
      const std::string& type,
      const int retries_left,
      const bool lock_enabled) {
    return Steps(CheckSimLockStateProperty(shill::kSIMLockTypeProperty,
                                           base::Value(type)),
                 CheckSimLockStateProperty(shill::kSIMLockRetriesLeftProperty,
                                           base::Value(retries_left)),
                 CheckSimLockStateProperty(shill::kSIMLockEnabledProperty,
                                           base::Value(lock_enabled)));
  }

  InteractiveTestApi::StepBuilder CheckSimLockStateProperty(
      const char* property,
      base::Value expected_value) {
    return Check([this, property,
                  expected_value = std::move(expected_value)]() {
      auto* const device_test = ShillDeviceClient::Get()->GetTestInterface();
      auto* const value =
          device_test ? device_test
                            ->GetDeviceProperty(device_path_,
                                                shill::kSIMLockStatusProperty)
                            ->GetDict()
                            .Find(property)
                      : nullptr;
      return value && *value == expected_value;
    });
  }

  void RestrictSimLockFromPolicy(bool allow_sim_lock) {
    base::Value::Dict global_config;
    global_config.Set(::onc::global_network_config::kAllowCellularSimLock,
                      allow_sim_lock);
    NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
        base::Value::List(), global_config);
  }

  // Navigates to the SIM lock settings on the detailed page of the cellular
  // network used for all SIM lock tests. This function does not do any checks
  // that we are in the correct state.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateToSimLockSettings() {
    return Steps(
        Log("Waiting for cellular to be enabled"),

        ObserveState(
            kMobileDataPoweredState,
            std::make_unique<ShillDevicePowerStateObserver>(
                ShillManagerClient::Get(), NetworkTypePattern::Mobile())),
        WaitForState(kMobileDataPoweredState, true),

        Log("Waiting for cellular network to be connected"),

        ObserveState(kCellularServiceConnected,
                     std::make_unique<WaitForServiceConnectedObserver>(
                         esim_info().iccid())),
        WaitForState(kCellularServiceConnected, true),

        Log("Navigating to the details page for the cellular network"),

        NavigateToInternetDetailsPage(kOSSettingsId,
                                      NetworkTypePattern::Cellular(),
                                      esim_info().nickname()),

        Log("Expanding the Advanced section"),

        WaitForElementExists(
            kOSSettingsId,
            settings::cellular::CellularDetailsAdvancedSection()),
        ClickElement(kOSSettingsId,
                     settings::cellular::CellularDetailsAdvancedSection()),
        WaitForElementExpanded(
            kOSSettingsId,
            settings::cellular::CellularDetailsAdvancedSection()));
  }

  // Enables SIM lock with the provided `pin`, checking that SIM lock is
  // initially off and checking that the subsequent state is correct.
  ui::test::internal::InteractiveTestPrivate::MultiStep EnableSimLockWithPin(
      const std::string& pin) {
    return Steps(
        Log("Checking that the SIM lock toggle exists and is off"),

        WaitForToggleState(kOSSettingsId,
                           settings::cellular::CellularSimLockToggle(),
                           /*is_checked=*/false),
        CheckSimLockState(
            /*type=*/"",
            /*retries_left=*/FakeShillDeviceClient::kSimPinRetryCount,
            /*lock_enabled=*/false),

        Log("Checking that the change SIM lock PIN button is hidden"),

        WaitForElementHasAttribute(
            kOSSettingsId, settings::cellular::CellularSimLockChangePinButton(),
            /*attribute=*/"hidden"),

        Log(base::StringPrintf("Enabling SIM lock with PIN %s", pin.c_str())),

        ClickElement(kOSSettingsId,
                     settings::cellular::CellularSimLockToggle()),
        ClearInputAndEnterText(
            kOSSettingsId,
            settings::cellular::CellularSimLockEnterPinDialogPin(), pin),
        ClickElement(kOSSettingsId,
                     settings::cellular::CellularSimLockEnterPinDialogButton()),
        WaitForElementDoesNotExist(
            kOSSettingsId,
            settings::cellular::CellularSimLockEnterPinDialogPin()),

        Log("Checking that the SIM lock toggle is now on"),

        WaitForToggleState(kOSSettingsId,
                           settings::cellular::CellularSimLockToggle(),
                           /*is_checked=*/true),
        CheckSimLockState(
            /*type=*/"",
            /*retries_left=*/FakeShillDeviceClient::kSimPinRetryCount,
            /*lock_enabled=*/true));
  }

  // Disables SIM lock with the provided `pin`, checking that SIM lock is
  // initially on and checking that the subsequent state is correct.
  ui::test::internal::InteractiveTestPrivate::MultiStep DisableSimLockWithPin(
      const std::string& pin) {
    return Steps(
        Log("Checking that the SIM lock toggle exists and is on"),

        WaitForToggleState(kOSSettingsId,
                           settings::cellular::CellularSimLockToggle(),
                           /*is_checked=*/true),

        Log(base::StringPrintf("Disabling SIM lock with PIN %s", pin.c_str())),

        ClickElement(kOSSettingsId,
                     settings::cellular::CellularSimLockToggle()),
        ClearInputAndEnterText(
            kOSSettingsId,
            settings::cellular::CellularSimLockEnterPinDialogPin(),
            pin.c_str()),
        ClickElement(kOSSettingsId,
                     settings::cellular::CellularSimLockEnterPinDialogButton()),
        WaitForElementDoesNotExist(
            kOSSettingsId,
            settings::cellular::CellularSimLockEnterPinDialogPin()),

        Log("Checking that the SIM lock toggle is now off"),

        WaitForToggleState(kOSSettingsId,
                           settings::cellular::CellularSimLockToggle(),
                           /*is_checked=*/false),
        CheckSimLockState(
            /*type=*/"",
            /*retries_left=*/FakeShillDeviceClient::kSimPinRetryCount,
            /*lock_enabled=*/false));
  }

  const SimInfo& esim_info() const { return *esim_info_; }

 private:
  std::string device_path_;
  std::unique_ptr<SimInfo> esim_info_;
};

IN_PROC_BROWSER_TEST_F(SimLockInteractiveUiTest, LockUnlockPin) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      NavigateToSimLockSettings(),

      EnableSimLockWithPin(kCorrectPin1),

      DisableSimLockWithPin(kCorrectPin1),

      Log("Closing the Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(SimLockInteractiveUiTest, ChangePin) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      NavigateToSimLockSettings(),

      EnableSimLockWithPin(kCorrectPin1),

      Log("Checking that the SIM lock PIN can be changed"),

      WaitForElementExists(
          kOSSettingsId, settings::cellular::CellularSimLockChangePinButton()),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockChangePinButton()),
      ClearInputAndEnterText(
          kOSSettingsId,
          settings::cellular::CellularSimLockChangePinDialogOld(),
          kCorrectPin1),
      ClearInputAndEnterText(
          kOSSettingsId,
          settings::cellular::CellularSimLockChangePinDialogNew(),
          kCorrectPin2),
      ClearInputAndEnterText(
          kOSSettingsId,
          settings::cellular::CellularSimLockChangePinDialogNewConfirm(),
          kCorrectPin2),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockChangePinDialogButton()),
      WaitForElementDoesNotExist(
          kOSSettingsId,
          settings::cellular::CellularSimLockChangePinDialogButton()),
      CheckSimLockState(
          /*type=*/"",
          /*retries_left=*/FakeShillDeviceClient::kSimPinRetryCount,
          /*lock_enabled=*/true),

      Log("Closing the Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(SimLockInteractiveUiTest, LockUnlockPuk) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      NavigateToSimLockSettings(),

      EnableSimLockWithPin(kCorrectPin1),

      Log("Checking that an incorrect PIN will result in the PUK being "
          "required"),

      WaitForToggleState(kOSSettingsId,
                         settings::cellular::CellularSimLockToggle(),
                         /*is_checked=*/true),
      ClickElement(kOSSettingsId, settings::cellular::CellularSimLockToggle()),
      WaitForElementExists(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogPin()),
      WaitForElementTextContains(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogSubtext(),
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_INTERNET_NETWORK_SIM_ENTER_PIN_DESCRIPTION_TEXT_LABEL)),

      Log("Entering incorrect PIN #1"),

      ClearInputAndEnterText(
          kOSSettingsId, settings::cellular::CellularSimLockEnterPinDialogPin(),
          kIncorrectPin1),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockEnterPinDialogButton()),
      WaitForElementTextContains(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogSubtext(),
          l10n_util::GetStringFUTF8(
              IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INCORRECT_PIN_PLURAL,
              base::NumberToString16(2))),
      CheckSimLockState(/*type=*/"", /*retries_left=*/2,
                        /*lock_enabled=*/true),

      Log("Entering incorrect PIN #2"),

      ClearInputAndEnterText(
          kOSSettingsId, settings::cellular::CellularSimLockEnterPinDialogPin(),
          kIncorrectPin2),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockEnterPinDialogButton()),
      WaitForElementTextContains(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogSubtext(),
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INCORRECT_PIN)),
      CheckSimLockState(/*type=*/"", /*retries_left=*/1,
                        /*lock_enabled=*/true),

      Log("Entering incorrect PIN #3"),

      ClearInputAndEnterText(
          kOSSettingsId, settings::cellular::CellularSimLockEnterPinDialogPin(),
          kIncorrectPin3),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockEnterPinDialogButton()),

      Log("Checking that we are prompted to input the PUK"),

      WaitForElementExists(
          kOSSettingsId,
          settings::cellular::CellularSimLockUnlockPukDialogPuk()),
      CheckSimLockState(/*type=*/shill::kSIMLockPuk, /*retries_left=*/10,
                        /*lock_enabled=*/true),

      Log("Entering PUK information"),

      ClearInputAndEnterText(
          kOSSettingsId,
          settings::cellular::CellularSimLockUnlockPukDialogPuk(),
          FakeShillDeviceClient::kSimPuk),
      ClearInputAndEnterText(
          kOSSettingsId,
          settings::cellular::CellularSimLockUnlockPukDialogPin(),
          kCorrectPin2),
      ClearInputAndEnterText(
          kOSSettingsId,
          settings::cellular::CellularSimLockUnlockPukDialogPinConfirm(),
          kCorrectPin2),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockUnlockPukDialogButton()),
      WaitForElementDoesNotExist(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogPin()),

      Log("Checking that the SIM lock toggle is on again"),

      WaitForToggleState(kOSSettingsId,
                         settings::cellular::CellularSimLockToggle(),
                         /*is_checked=*/true),
      CheckSimLockState(
          /*type=*/"",
          /*retries_left=*/FakeShillDeviceClient::kSimPinRetryCount,
          /*lock_enabled=*/true),

      Log("Closing the Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(SimLockInteractiveUiTest, ProhibitWithPolicy) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      NavigateToSimLockSettings(),

      EnableSimLockWithPin(kCorrectPin1),

      Log("Checking that SIM PIN lock toggle does not have an enterprise icon"),

      WaitForElementDoesNotExist(
          kOSSettingsId, settings::cellular::CellularSimLockTogglePolicyIcon()),

      Log("Prohibiting SIM PIN lock with policy"),

      Do([this]() { RestrictSimLockFromPolicy(/*allow_sim_lock=*/false); }),

      Log("Checking that SIM PIN lock toggle has an enterprise icon"),

      WaitForElementExists(
          kOSSettingsId, settings::cellular::CellularSimLockTogglePolicyIcon()),

      Log("Waiting for the unlock SIM notification to be shown"),

      WaitForShow(kCellularManagedSimLockNotificationElementId),

      // We remove the notification since clicking it will cause OS Settings to
      // refresh and re-navigate to the SIM lock settings which takes a
      // significant amount of time (would need to extend the step timeout to
      // account for this), and leaving the notification open could cause issues
      // with clicking the SIM lock toggle.
      Log("Removing the unlock SIM notification and waiting for it to be "
          "hidden before continuing"),

      Do([]() {
        message_center::MessageCenter::Get()->RemoveNotification(
            /*id=*/ManagedSimLockNotifier::kManagedSimLockNotificationId,
            /*by_user=*/false);
      }),
      WaitForHide(kCellularManagedSimLockNotificationElementId),

      Log(base::StringPrintf("Unlocking SIM lock with PIN %s", kCorrectPin1)),

      ClickElement(kOSSettingsId, settings::cellular::CellularSimLockToggle()),
      WaitForElementTextContains(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogPolicySubtitle(),
          /*expected=*/
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCK_POLICY_ADMIN_SUBTITLE)),
      ClearInputAndEnterText(
          kOSSettingsId, settings::cellular::CellularSimLockEnterPinDialogPin(),
          kCorrectPin1),
      ClickElement(kOSSettingsId,
                   settings::cellular::CellularSimLockEnterPinDialogButton()),
      WaitForElementDoesNotExist(
          kOSSettingsId,
          settings::cellular::CellularSimLockEnterPinDialogPin()),

      Log("Checking that the SIM lock toggle is now off and disabled"),

      WaitForToggleState(kOSSettingsId,
                         settings::cellular::CellularSimLockToggle(),
                         /*is_checked=*/false),
      WaitForElementDisabled(kOSSettingsId,
                             settings::cellular::CellularSimLockToggle()),
      CheckSimLockState(
          /*type=*/"",
          /*retries_left=*/FakeShillDeviceClient::kSimPinRetryCount,
          /*lock_enabled=*/false),

      Log("Closing the Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
