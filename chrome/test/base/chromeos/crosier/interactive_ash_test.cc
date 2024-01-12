// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/test/test_switches.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/chromeos/crosier/aura_window_title_observer.h"
#include "url/gurl.h"

namespace {

using InteractiveMixinBasedBrowserTest =
    InteractiveBrowserTestT<MixinBasedInProcessBrowserTest>;

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
InteractiveAshTest::ClickElement(const ui::ElementIdentifier& element_id,
                                 const DeepQuery& query) {
  return Steps(MoveMouseTo(element_id, query), ClickMouse());
}
