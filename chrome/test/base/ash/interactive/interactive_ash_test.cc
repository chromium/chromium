// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/interactive_ash_test.h"

#include <optional>

#include "ash/ash_element_identifiers.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_switches.h"
#include "base/values.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chrome/test/base/chromeos/crosier/aura_window_title_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace {

using InteractiveMixinBasedBrowserTest =
    InteractiveBrowserTestT<MixinBasedInProcessBrowserTest>;

// This JavaScript is used to select an option from a dropdown menu. This
// JavaScript can be formatted with a single string to identify the desired
// option. The string can be a substring of the complete option name.
constexpr char kSelectDropdownElementOptionJs[] = R"(
  (el) => {
    const elements = el.querySelectorAll('option');
    for (let i = 0; i < elements.length; i++) {
      if (elements[i].label.indexOf('%s') == -1) {
        continue;
      }
      el.selectedIndex = elements[i].index;
      el.dispatchEvent(new Event('change'));
      return true;
    }
    return false;
  })";

// This JavaScript defines a function "action" that returns `true` if `el`
// contains the expected inner text.
constexpr char kFindElementWithTextActionJs[] = R"(
  function action(el) {
    return el && el.innerText.indexOf(%s) >= 0;
  })";

// This JavaScript defines a function "action" that returns `true` if `el`
// and a sibling of the element contains the expected inner texts.
constexpr char kFindMatchingTextsInElementAndSibling[] = R"(
  function action(el) {
    if (!el) {
      return false;
    }

    var textElement = el.shadowRoot.querySelector(%s);
    if (!textElement || textElement.innerText.indexOf(%s) == -1) {
      return false;
    }

    var siblingElement = el.shadowRoot.querySelector(%s);
    if (!siblingElement || siblingElement.innerText.indexOf(%s) == -1) {
      return false;
    }

    return true;
  }
)";

// This JavaScript defines a function "action" that returns `true` if `el`
// contains the expected inner text. Before returning `el` is clicked.
constexpr char kClickElementWithTextActionJs[] = R"(
  function action(el) {
    if (el && el.innerText.indexOf(%s) >= 0) {
      el.click();
      return true;
    }
    return false;
  })";

// This JavaScript defines a function "action" that returns `true` if `el`
// has a child element that contains the expected text and matches the
// provided selectors, and if the child element has a sibling that matches
// the provided selectors. The sibling element is clicked before returning.
constexpr char kClickChildOfElementWithTextActionJs[] = R"(
  function action(el) {
    if (!el) {
      return false;
    }
    var text = el.shadowRoot.querySelector(%s);
    if (!text || text.innerText.indexOf(%s) == -1) {
      return false;
    }
    var child = el.shadowRoot.querySelector(%s);
    if (child) {
      child.click();
      return true;
    }
    return false;
  })";

// This JavaScript is used to search for an element in the DOM. The element is
// described in terms of a root element and the relative path provided via an
// array of selectors, and the element will be considered "found" if it matches
// the entire array of selectors AND meets an arbitrary criteria.
//
// This JavaScript should be formatted with two strings: a JavaScript function
// and a list of selectors. The JavaScript function should be named "action",
// accept a single element parameter, and return `true` if the element parameter
// meets the desired criteria. The list of selectors should define the path from
// `el` to the desired element. The function will only be called on each element
// that matches the list of selectors.
constexpr char kFindElementAndDoActionJs[] = R"(
  (el) => {
    %s;

    function findElement(root, index, selectors) {
      const elements = root.shadowRoot.querySelectorAll(selectors[index]);
      for (let el of elements) {
        if (index < selectors.length - 1) {
          if (findElement(el, index + 1, selectors)) {
            return true;
          }
          continue;
        }
        if (action(el)) {
          return true;
        }
      }
      return false;
    }
    return findElement(el, 0, %s);
  })";

std::string DeepQueryToSelectors(
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  // Safely convert the selector list in `where` to a JSON/JS list.
  base::Value::List selector_list;
  for (const auto& selector : query) {
    selector_list.Append(selector);
  }
  std::string selectors;
  CHECK(base::JSONWriter::Write(selector_list, &selectors));
  return selectors;
}

}  // namespace

InteractiveAshTest::InteractiveAshTest() {
  // See header file class comment.
  set_launch_browser_for_testing(nullptr);

  // Give all widgets the same Kombucha context.This is useful for ash system
  // UI because the UI uses a variety of small widgets. Note that if this test
  // used multiple displays we would need to provide a different context per
  // display (i.e. the widget's native window's root window). Elements like
  // the home button, shelf, etc. appear once per display.
  views::ElementTrackerViews::SetContextOverrideCallback(
      base::BindRepeating([](views::Widget* widget) {
        return ui::ElementContext(ash::Shell::GetPrimaryRootWindow());
      }));
}

InteractiveAshTest::~InteractiveAshTest() {
  views::ElementTrackerViews::SetContextOverrideCallback({});
}

void InteractiveAshTest::SetupContextWidget() {
  views::Widget* status_area_widget =
      ash::Shell::GetPrimaryRootWindowController()
          ->shelf()
          ->GetStatusAreaWidget();
  SetContextWidget(status_area_widget);
}

void InteractiveAshTest::InstallSystemApps() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  ash::SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
}

ui::ElementContext InteractiveAshTest::LaunchSystemWebApp(
    ash::SystemWebAppType type,
    const ui::ElementIdentifier& element_id) {
  Profile* profile = GetActiveUserProfile();
  CHECK(profile);
  RunTestSequence(InstrumentNextTab(element_id, AnyBrowser()),
                  Do([&]() { LaunchSystemWebAppAsync(profile, type); }),
                  InAnyContext(WaitForShow(element_id)));
  return FindSystemWebApp(type);
}

ui::ElementContext InteractiveAshTest::FindSystemWebApp(
    ash::SystemWebAppType type) {
  Profile* profile = GetActiveUserProfile();
  CHECK(profile);
  Browser* browser = FindSystemWebAppBrowser(profile, type);
  CHECK(browser);
  return browser->window()->GetElementContext();
}

void InteractiveAshTest::CloseSystemWebApp(ash::SystemWebAppType type) {
  if (Profile* const profile = GetActiveUserProfile()) {
    if (Browser* const browser = FindSystemWebAppBrowser(profile, type)) {
      chrome::CloseWindow(browser);
    }
  }
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateSettingsToInternetPage(
    const ui::ElementIdentifier& element_id) {
  return NavigateSettingsToPage(element_id, /*path=*/"/internet");
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateSettingsToBluetoothPage(
    const ui::ElementIdentifier& element_id) {
  return NavigateSettingsToPage(element_id, /*path=*/"/bluetooth");
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::OpenQuickSettings() {
  return Steps(PressButton(ash::kUnifiedSystemTrayElementId),
               WaitForShow(ash::kQuickSettingsViewElementId));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateQuickSettingsToNetworkPage() {
  return NavigateQuickSettingsToPage(
      ash::kNetworkFeatureTileDrillInArrowElementId);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateQuickSettingsToBluetoothPage() {
  return NavigateQuickSettingsToPage(
      ash::kBluetoothFeatureTileDrillInArrowElementId);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateToApnRevampDetailsPage(
    const ui::ElementIdentifier& element_id) {
  return Steps(
      WaitForElementExists(element_id,
                           ash::settings::cellular::CellularSubpageApnRow()),
      ScrollIntoView(element_id,
                     ash::settings::cellular::CellularSubpageApnRow()),
      MoveMouseTo(element_id, ash::settings::cellular::CellularSubpageApnRow()),
      ClickMouse(),
      WaitForElementExists(
          element_id, ash::settings::cellular::ApnSubpageActionMenuButton()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::OpenAddCustomApnDetailsDialog(
    const ui::ElementIdentifier& element_id) {
  return Steps(
      WaitForElementEnabled(
          element_id, ash::settings::cellular::ApnSubpageActionMenuButton()),
      ClickElement(element_id,
                   ash::settings::cellular::ApnSubpageActionMenuButton()),
      WaitForElementEnabled(
          element_id, ash::settings::cellular::ApnSubpageCreateApnButton()),
      ClickElement(element_id,
                   ash::settings::cellular::ApnSubpageCreateApnButton()),
      WaitForElementExists(element_id, ash::settings::cellular::ApnDialog()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::OpenApnSelectionDialog(
    const ui::ElementIdentifier& element_id) {
  return Steps(
      WaitForElementEnabled(
          element_id, ash::settings::cellular::ApnSubpageActionMenuButton()),
      ClickElement(element_id,
                   ash::settings::cellular::ApnSubpageActionMenuButton()),
      WaitForElementEnabled(
          element_id, ash::settings::cellular::ApnSubpageShowKnownApnsButton()),
      ClickElement(element_id,
                   ash::settings::cellular::ApnSubpageShowKnownApnsButton()),
      WaitForElementExists(element_id,
                           ash::settings::cellular::ApnSelectionDialog()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::OpenAddBuiltInVpnDialog(
    const ui::ElementIdentifier& element_id) {
  return Steps(
      WaitForElementEnabled(element_id,
                            ash::settings::AddConnectionsExpandButton()),
      ClickElement(element_id, ash::settings::AddConnectionsExpandButton()),
      WaitForElementExpanded(element_id,
                             ash::settings::AddConnectionsExpandButton()),
      WaitForElementEnabled(element_id, ash::settings::AddBuiltInVpnRow()),
      ClickElement(element_id, ash::settings::AddBuiltInVpnRow()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::OpenAddWifiDialog(const ui::ElementIdentifier& element_id) {
  return Steps(
      NavigateSettingsToNetworkSubpage(element_id,
                                       ash::NetworkTypePattern::WiFi()),
      WaitForElementExists(element_id, ash::settings::wifi::AddWifiButton()),
      ClickElement(element_id, ash::settings::wifi::AddWifiButton()),
      WaitForElementExists(element_id,
                           ash::settings::wifi::ConfigureWifiDialog()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::CompleteAddWifiDialog(
    const ui::ElementIdentifier& element_id,
    const WifiDialogConfig& config) {
  CHECK(!config.ssid.empty());
  ui::test::internal::InteractiveTestPrivate::MultiStep steps = Steps(
      Log(base::StringPrintf("Entering SSID \"%s\" for the network",
                             config.ssid.c_str())),
      WaitForElementExists(element_id,
                           ash::settings::wifi::ConfigureWifiDialogSsidInput()),
      ClearInputAndEnterText(
          element_id, ash::settings::wifi::ConfigureWifiDialogSsidInput(),
          config.ssid.c_str()),
      Log("Ensuring the network has the correct security"));

  if (config.security_type !=
      ::chromeos::network_config::mojom::SecurityType::kNone) {
    // TODO(cros-device-enablement@google.com): Add logic for selecting security
    // type and filling out the relevant fields.
    NOTREACHED();
  }

  AddStep(steps,
          Log(base::StringPrintf("Configuring the network as %s",
                                 config.is_shared ? "shared" : "not shared")));
  if (config.security_type ==
      ::chromeos::network_config::mojom::SecurityType::kNone) {
    AddStep(steps, WaitForElementChecked(
                       element_id,
                       ash::settings::wifi::ConfigureWifiDialogShareToggle()));
    if (!config.is_shared) {
      AddStep(
          steps,
          Steps(ClickElement(
                    element_id,
                    ash::settings::wifi::ConfigureWifiDialogShareToggle()),
                WaitForElementUnchecked(
                    element_id,
                    ash::settings::wifi::ConfigureWifiDialogShareToggle())));
    }
  } else {
    // TODO(cros-device-enablement@google.com): Add logic for marking the
    // network as that it is shared.
    NOTREACHED();
  }

  AddStep(steps,
          Steps(Log("Clicking the connect button and waiting for the dialog "
                    "to disappear"),
                WaitForElementEnabled(
                    element_id,
                    ash::settings::wifi::ConfigureWifiDialogConnectButton()),
                ClickElement(
                    element_id,
                    ash::settings::wifi::ConfigureWifiDialogConnectButton()),
                WaitForElementDoesNotExist(
                    element_id, ash::settings::wifi::ConfigureWifiDialog())));
  return steps;
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateQuickSettingsToHotspotPage() {
  return NavigateQuickSettingsToPage(
      ash::kHotspotFeatureTileDrillInArrowElementId);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateSettingsToNetworkSubpage(
    const ui::ElementIdentifier& element_id,
    const ash::NetworkTypePattern network_pattern) {
  WebContentsInteractionTestUtil::DeepQuery internet_summary_row;

  if (network_pattern.MatchesPattern(ash::NetworkTypePattern::Mobile())) {
    internet_summary_row = ash::settings::cellular::CellularSummaryItem();
  } else if (network_pattern.MatchesPattern(ash::NetworkTypePattern::VPN())) {
    internet_summary_row = ash::settings::vpn::VpnSummaryItem();
  } else if (network_pattern.MatchesPattern(ash::NetworkTypePattern::WiFi())) {
    internet_summary_row = ash::settings::wifi::WifiSummaryItem();
  } else {
    // Unsupported Network pattern.
    NOTREACHED();
  }

  return Steps(NavigateSettingsToInternetPage(element_id),
               WaitForElementExists(element_id, internet_summary_row),
               ScrollIntoView(element_id, internet_summary_row),
               MoveMouseTo(element_id, internet_summary_row), ClickMouse());
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateToInternetDetailsPage(
    const ui::ElementIdentifier& element_id,
    const ash::NetworkTypePattern network_pattern,
    const std::string& network_name) {
  WebContentsInteractionTestUtil::DeepQuery network_list;
  WebContentsInteractionTestUtil::DeepQuery network_list_item(
      {"network-list-item"});
  WebContentsInteractionTestUtil::DeepQuery network_list_item_title(
      {"div#divText"});
  WebContentsInteractionTestUtil::DeepQuery network_list_item_subpage_arrow(
      {"cr-icon-button#subpageButton"});
  std::string element_selector;

  // TODO: Add other network types.
  if (network_pattern.MatchesPattern(ash::NetworkTypePattern::Mobile())) {
    network_list = ash::settings::cellular::CellularNetworksList();
    network_list_item = WebContentsInteractionTestUtil::DeepQuery(
        {"network-list", "network-list-item"});
  } else if (network_pattern.MatchesPattern(ash::NetworkTypePattern::VPN())) {
    network_list = ash::settings::vpn::VpnNetworksList();
  } else {
    // Unsupported Network pattern.
    NOTREACHED();
  }

  return Steps(NavigateSettingsToNetworkSubpage(element_id, network_pattern),
               FindElementAndDoActionOnChildren(
                   element_id, network_list, network_list_item,
                   ClickElementWithSiblingContainsText(
                       network_list_item_title, network_name,
                       network_list_item_subpage_arrow)),
               WaitForElementTextContains(
                   element_id, ash::settings::InternetSettingsSubpageTitle(),
                   /*text=*/network_name.c_str()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateToBluetoothDeviceDetailsPage(
    const ui::ElementIdentifier& element_id,
    const std::string& device_name) {
  const WebContentsInteractionTestUtil::DeepQuery bluetooth_device_item_title(
      {"os-settings-paired-bluetooth-list-item", "div#deviceName"});

  return Steps(NavigateSettingsToBluetoothPage(element_id),
               WaitForAnyElementTextContains(
                   element_id, ash::settings::bluetooth::BluetoothDeviceList(),
                   bluetooth_device_item_title, device_name),
               ClickAnyElementTextContains(
                   element_id, ash::settings::bluetooth::BluetoothDeviceList(),
                   bluetooth_device_item_title, device_name),
               WaitForElementTextContains(
                   element_id, ash::settings::bluetooth::BluetoothDeviceName(),
                   device_name));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateToKnownNetworksPage(
    const ui::ElementIdentifier& element_id) {
  return Steps(
      NavigateSettingsToInternetPage(element_id),
      WaitForElementEnabled(element_id, ash::settings::wifi::WifiSummaryItem()),
      ClickElement(element_id, ash::settings::wifi::WifiSummaryItem()),
      WaitForElementEnabled(
          element_id, ash::settings::wifi::WifiKnownNetworksSubpageButton()),
      ClickElement(element_id,
                   ash::settings::wifi::WifiKnownNetworksSubpageButton()),
      WaitForElementExists(element_id,
                           ash::settings::wifi::KnownNetworksSubpage()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateToPasspointSubscriptionSubpage(
    const ui::ElementIdentifier& element_id,
    const std::string& passpoint_name) {
  const WebContentsInteractionTestUtil::DeepQuery
      passpoint_subscription_item_name(
          {"cr-link-row#subscriptionItem", "div#label"});

  return Steps(
      NavigateToKnownNetworksPage(element_id),
      WaitForElementExists(
          element_id,
          ash::settings::wifi::KnownNetworksSubpagePasspointSubsciptions()),
      ClickAnyElementTextContains(
          element_id, ash::settings::wifi::KnownNetworksSubpage(),
          passpoint_subscription_item_name, passpoint_name),
      WaitForElementTextContains(element_id,
                                 ash::settings::InternetSettingsSubpageTitle(),
                                 /*text=*/passpoint_name.c_str()));
}

Profile* InteractiveAshTest::GetActiveUserProfile() {
  return ProfileManager::GetActiveUserProfile();
}

base::WeakPtr<content::NavigationHandle>
InteractiveAshTest::CreateBrowserWindow(const GURL& url) {
  Profile* profile = GetActiveUserProfile();
  CHECK(profile);
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.window_action = NavigateParams::SHOW_WINDOW;
  return Navigate(&params);
}

void InteractiveAshTest::TearDownOnMainThread() {
  // Passing --test-launcher-interactive leaves the browser running after the
  // end of the test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherInteractive)) {
    base::RunLoop loop;
    loop.Run();
  }
  InteractiveBrowserTestT<
      MixinBasedInProcessBrowserTest>::TearDownOnMainThread();
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForWindowWithTitle(aura::Env* env,
                                           std::u16string title) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(AuraWindowTitleObserver, kTitleObserver);
  return Steps(
      ObserveState(kTitleObserver,
                   std::make_unique<AuraWindowTitleObserver>(env, title)),
      WaitForState(kTitleObserver, true), StopObservingState(kTitleObserver));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementExists(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
  StateChange element_exists;
  element_exists.event = kElementExists;
  element_exists.where = query;
  return WaitForStateChange(element_id, element_exists);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementDoesNotExist(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDoesNotExist);
  StateChange does_not_exist;
  does_not_exist.type = StateChange::Type::kDoesNotExist;
  does_not_exist.event = kElementDoesNotExist;
  does_not_exist.where = query;
  return WaitForStateChange(element_id, does_not_exist);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementEnabled(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementEnabled;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return !el.disabled; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementWithManagedPropertyBoolean(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query,
    const std::string& property,
    bool expected_value) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kManagedBooleanChange);
  StateChange managed_boolean_change;
  managed_boolean_change.event = kManagedBooleanChange;
  managed_boolean_change.where = query;
  managed_boolean_change.type = StateChange::Type::kExistsAndConditionTrue;
  managed_boolean_change.test_function =
      base::StringPrintf(R"(
    (el) => {
      return el &&
             el.managedProperties_ &&
             el.managedProperties_.%s.activeValue === %s;
    }
  )",
                         property.c_str(), expected_value ? "true" : "false");
  return WaitForStateChange(element_id, managed_boolean_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementDisabled(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDisabled);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementDisabled;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return el.disabled; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementChecked(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementChecked);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementChecked;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return el.checked; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementUnchecked(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementUnchecked);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementUnchecked;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return !el.checked; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementExpanded(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExpanded);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementExpanded;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return el.expanded; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementOpened(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementOpened);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementOpened;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return el.opened || el.open; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementUnopened(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementUnopened);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementUnopened;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = "(el) => { return !el.opened && !el.open; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementFocused(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementFocused);
  StateChange element_focused;
  element_focused.event = kElementFocused;
  element_focused.where = query;
  element_focused.test_function =
      "(el) => { return el === document.activeElement; }";
  return WaitForStateChange(element_id, element_focused);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementTextContains(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query,
    const std::string& expected) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextFound);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.type = WebContentsInteractionTestUtil::StateChange::Type::
      kExistsAndConditionTrue;
  state_change.where = query;
  state_change.test_function = "function(el) { return el.innerText.indexOf(" +
                               base::GetQuotedJSONString(expected) +
                               ") >= 0; }";
  state_change.event = kTextFound;
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForAnyElementTextContains(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& root,
    const WebContentsInteractionTestUtil::DeepQuery& selectors,
    const std::string& expected) {
  return FindElementAndDoActionOnChildren(
      element_id, root, selectors,
      base::StringPrintf(kFindElementWithTextActionJs,
                         base::GetQuotedJSONString(expected).c_str()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForAnyElementAndSiblingTextContains(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& root,
    const WebContentsInteractionTestUtil::DeepQuery& selectors,
    const WebContentsInteractionTestUtil::DeepQuery& element_with_text,
    const std::string& expected_text,
    const WebContentsInteractionTestUtil::DeepQuery& sibling_element,
    const std::string& sibling_expected_text) {
  return FindElementAndDoActionOnChildren(
      element_id, root, selectors,
      FindMatchingTextsInElementAndSibling(element_with_text, expected_text,
                                           sibling_element,
                                           sibling_expected_text));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementHasAttribute(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element,
    const std::string& attribute) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHasAttribute);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementHasAttribute;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = base::StringPrintf(
      "(el) => { return el.hasAttribute('%s'); }", attribute.c_str());
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementDoesNotHaveAttribute(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element,
    const std::string& attribute) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHasAttribute);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementHasAttribute;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function = base::StringPrintf(
      "(el) => { return !el.hasAttribute('%s'); }", attribute.c_str());
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementDisplayNone(
    const ui::ElementIdentifier& element_id,
    WebContentsInteractionTestUtil::DeepQuery element) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHasDisplayNone);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.event = kElementHasDisplayNone;
  state_change.where = element;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.test_function =
      "(el) => { return el.style.display === 'none'; }";
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForToggleState(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query,
    bool is_checked) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kToggleState);
  StateChange toggle_state_change;
  toggle_state_change.event = kToggleState;
  toggle_state_change.where = query;
  toggle_state_change.type = StateChange::Type::kExistsAndConditionTrue;
  toggle_state_change.test_function = is_checked
                                          ? "(el) => { return el.checked; }"
                                          : "(el) => { return !el.checked; }";
  return WaitForStateChange(element_id, toggle_state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::ClearInputFieldValue(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputElementText);
  StateChange input_element_change;
  input_element_change.event = kInputElementText;
  input_element_change.where = query;
  input_element_change.type = StateChange::Type::kExistsAndConditionTrue;
  input_element_change.test_function = "(el) => { return el.value === ''; }";

  return Steps(MoveMouseTo(element_id, query), ClickMouse(),
               CheckJsResultAt(element_id, query,
                               "(el) => {el.value = ''; return true;}"),
               WaitForStateChange(element_id, input_element_change));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementToRender(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementRenders);
  StateChange element_renders;
  element_renders.event = kElementRenders;
  element_renders.where = query;
  element_renders.test_function =
      "(el) => { if (el !== null) { let rect = el.getBoundingClientRect(); "
      "return rect.width > 0 && rect.height > 0; } return false; }";
  return WaitForStateChange(element_id, element_renders);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::ClickElement(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query) {
  return Steps(MoveMouseTo(element_id, query), ClickMouse());
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::ClickAnyElementTextContains(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& root,
    const WebContentsInteractionTestUtil::DeepQuery& selectors,
    const std::string& expected) {
  return FindElementAndDoActionOnChildren(
      element_id, root, selectors,
      base::StringPrintf(kClickElementWithTextActionJs,
                         base::GetQuotedJSONString(expected).c_str()));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::SelectDropdownElementOption(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query,
    const std::string& option) {
  return Steps(
      WaitForElementExists(element_id, query),
      CheckJsResultAt(
          element_id, query,
          base::StringPrintf(kSelectDropdownElementOptionJs, option.c_str())));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::SendTextAsKeyEvents(const ui::ElementIdentifier& element_id,
                                        const std::string& text) {
  MultiStep steps;

  // The text that should be entered as key events is provided as a std::string,
  // but key events require a key code. The loop below converts each character
  // of the string into a single key event; this loop avoids code duplication be
  // identifying when a character exists within a known contiguous range, e.g. A
  // through Z, and computing the desired key code by calculating the offset of
  // the character into the known contiguous range.
  for (char c : text) {
    std::optional<ui::KeyboardCode> key_code;
    unsigned short offset = 0;
    int modifiers = 0;

    if (c >= 'a' && c <= 'z') {
      key_code = ui::VKEY_A;
      offset = c - 'a';
    } else if (c >= 'A' && c <= 'Z') {
      key_code = ui::VKEY_A;
      offset = c - 'A';
      modifiers = ui::EF_SHIFT_DOWN;
    } else if (c >= '0' && c <= '9') {
      key_code = ui::VKEY_0;
      offset = c - '0';
    } else if (c == '$') {
      key_code = ui::VKEY_4;
      modifiers = ui::EF_SHIFT_DOWN;
    } else if (c == '-') {
      key_code = ui::VKEY_OEM_MINUS;
    } else if (c == '.') {
      key_code = ui::VKEY_OEM_PERIOD;
    } else if (c == ';') {
      key_code = ui::VKEY_OEM_1;
    } else if (c == ':') {
      key_code = ui::VKEY_OEM_1;
      modifiers = ui::EF_SHIFT_DOWN;
    } else if (c == '_') {
      key_code = ui::VKEY_OEM_MINUS;
      modifiers = ui::EF_SHIFT_DOWN;
    } else if (c == '\n') {
      key_code = ui::VKEY_RETURN;
    } else if (c == ' ') {
      key_code = ui::VKEY_SPACE;
    }

    if (!key_code.has_value()) {
      // Unsupported input.
      NOTREACHED_IN_MIGRATION();
    }

    AddStep(steps, SendAccelerator(
                       element_id,
                       ui::Accelerator(
                           static_cast<ui::KeyboardCode>(*key_code + offset),
                           modifiers, ui::Accelerator::KeyState::PRESSED)));
  }
  return steps;
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::ClearInputAndEnterText(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& query,
    const std::string& text) {
  return Steps(WaitForElementExists(element_id, query),
               ClearInputFieldValue(element_id, query),
               ClickElement(element_id, query),
               SendTextAsKeyEvents(element_id, text));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::FindElementAndDoActionOnChildren(
    const ui::ElementIdentifier& element_id,
    const WebContentsInteractionTestUtil::DeepQuery& root,
    const WebContentsInteractionTestUtil::DeepQuery& selectors,
    const std::string& action) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementWithActionReturnTrue);

  WebContentsInteractionTestUtil::StateChange state_change;
  state_change.type = WebContentsInteractionTestUtil::StateChange::Type::
      kExistsAndConditionTrue;
  state_change.where = root;
  state_change.test_function =
      base::StringPrintf(kFindElementAndDoActionJs, action.c_str(),
                         DeepQueryToSelectors(selectors).c_str());
  state_change.event = kElementWithActionReturnTrue;
  return WaitForStateChange(element_id, state_change);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateSettingsToPage(
    const ui::ElementIdentifier& element_id,
    const char* path) {
  CHECK(path);
  const WebContentsInteractionTestUtil::DeepQuery menu_item(
      {{"os-settings-ui", "os-settings-menu",
        base::StringPrintf("os-settings-menu-item[path=\"%s\"]", path)}});
  return Steps(ScrollIntoView(element_id, menu_item),
               MoveMouseTo(element_id, menu_item), ClickMouse());
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::NavigateQuickSettingsToPage(
    const ui::ElementIdentifier& element_id) {
  // This function assumes that the drill-in arrow is or will become visible
  // without any action.
  return Steps(WaitForShow(ash::kQuickSettingsViewElementId),
               WaitForShow(element_id), MoveMouseTo(element_id), ClickMouse());
}

const std::string InteractiveAshTest::ClickElementWithSiblingContainsText(
    const WebContentsInteractionTestUtil::DeepQuery& element_with_text,
    const std::string& expected,
    const WebContentsInteractionTestUtil::DeepQuery& element_to_click) {
  return base::StringPrintf(kClickChildOfElementWithTextActionJs,
                            DeepQueryToSelectors(element_with_text).c_str(),
                            base::GetQuotedJSONString(expected).c_str(),
                            DeepQueryToSelectors(element_to_click).c_str());
}

const std::string InteractiveAshTest::FindMatchingTextsInElementAndSibling(
    const WebContentsInteractionTestUtil::DeepQuery& element_with_text,
    const std::string& expected_text,
    const WebContentsInteractionTestUtil::DeepQuery& sibling_element,
    const std::string& sibling_expected_text) {
  return base::StringPrintf(
      kFindMatchingTextsInElementAndSibling,
      DeepQueryToSelectors(element_with_text).c_str(),
      base::GetQuotedJSONString(expected_text).c_str(),
      DeepQueryToSelectors(sibling_element).c_str(),
      base::GetQuotedJSONString(sibling_expected_text).c_str());
}
