// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/interactive_ash_test.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_switches.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/chromeos/crosier/aura_window_title_observer.h"
#include "ui/base/interaction/element_identifier.h"
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
  Browser* browser = nullptr;
  RunTestSequence(InstrumentNextTab(element_id, AnyBrowser()),
                  Do([&]() { LaunchSystemWebAppAsync(profile, type); }),
                  InAnyContext(WaitForShow(element_id)), Do([&]() {
                    browser = FindSystemWebAppBrowser(profile, type);
                  }));
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
      WaitForState(kTitleObserver, true));
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementExists(
    const ui::ElementIdentifier& element_id,
    const DeepQuery& query) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
  StateChange element_exists;
  element_exists.event = kElementExists;
  element_exists.where = query;
  return WaitForStateChange(element_id, element_exists);
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::WaitForElementDoesNotExist(
    const ui::ElementIdentifier& element_id,
    const DeepQuery& query) {
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
InteractiveAshTest::WaitForElementFocused(
    const ui::ElementIdentifier& element_id,
    const DeepQuery& query) {
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
InteractiveAshTest::WaitForElementToRender(
    const ui::ElementIdentifier& element_id,
    const DeepQuery& query) {
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
InteractiveAshTest::ClickElement(const ui::ElementIdentifier& element_id,
                                 const DeepQuery& query) {
  return Steps(MoveMouseTo(element_id, query), ClickMouse());
}

ui::test::internal::InteractiveTestPrivate::MultiStep
InteractiveAshTest::SelectDropdownElementOption(
    const ui::ElementIdentifier& element_id,
    const DeepQuery& query,
    const std::string& option) {
  return Steps(
      WaitForElementExists(element_id, query),
      CheckJsResultAt(
          element_id, query,
          base::StringPrintf(kSelectDropdownElementOptionJs, option.c_str())));
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
