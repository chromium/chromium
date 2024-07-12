// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_INTERACTIVE_ASH_TEST_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_INTERACTIVE_ASH_TEST_H_

#include <memory>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interaction_sequence.h"

class GURL;
class Profile;

namespace content {
class NavigationHandle;
}

// Sets up Kombucha for ash testing:
// - Provides 1 Kombucha "context" per display, shared by all views::Widgets
// - Provides a default "context widget" so Kombucha can synthesize mouse events
// - Suppresses creating a browser window on startup, because most ash-chrome
//   tests don't need the window and creating it slows down the test
//
// Because this class derives from InProcessBrowserTest the source files must be
// added to a target that defines HAS_OUT_OF_PROC_TEST_RUNNER. The source files
// cannot be in a shared test support target that lacks that define.
//
// For tests that run on a DUT or in a VM, use the subclass AshIntegrationTest,
// which supports running on hardware.
class InteractiveAshTest
    : public InteractiveBrowserTestT<MixinBasedInProcessBrowserTest> {
 public:
  InteractiveAshTest();
  InteractiveAshTest(const InteractiveAshTest&) = delete;
  InteractiveAshTest& operator=(const InteractiveAshTest&) = delete;
  ~InteractiveAshTest() override;

  // Sets up a context widget for Kombucha. Call this at the start of each test
  // body. This is needed because InteractiveAshTest doesn't open a browser
  // window by default, but Kombucha needs a widget to simulate mouse events.
  void SetupContextWidget();

  // Installs system web apps (SWAs) like OS Settings, Files, etc. Can be called
  // in SetUpOnMainThread() or in your test body. SWAs are not installed by
  // default because this speeds up tests that don't need the apps.
  void InstallSystemApps();

  // Launches the system web app of type `type`. Associates `element_id` with
  // the app window and returns a Kombucha context for the app window.
  ui::ElementContext LaunchSystemWebApp(
      ash::SystemWebAppType type,
      const ui::ElementIdentifier& element_id);

  // Finds the system web app of type `type` and returns the Kombucha context
  // for the app window.
  ui::ElementContext FindSystemWebApp(ash::SystemWebAppType type);

  // Attempts to close the system web app of type `type`.
  void CloseSystemWebApp(ash::SystemWebAppType type);

  // Navigates the Settings app, which is expected to be associated with
  // `element_id`, to the top-level internet page.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateSettingsToInternetPage(const ui::ElementIdentifier& element_id);

  // Navigates the Settings app, which is expected to be associated with
  // `element_id`, to the top-level bluetooth page.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateSettingsToBluetoothPage(const ui::ElementIdentifier& element_id);

  //  Navigates the Settings app, which is expected to be associated with
  // `element_id`, to the details page for the network named `network_name`
  // with type `network_pattern`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateToInternetDetailsPage(const ui::ElementIdentifier& element_id,
                                const ash::NetworkTypePattern network_pattern,
                                const std::string& network_name);

  // This function expects the Settings to already be open and on the detailed
  // page of a cellular network.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateToApnRevampDetailsPage(const ui::ElementIdentifier& element_id);

  // This function expects the Settings to already be open and on the APN
  // subpage.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  OpenAddCustomApnDetailsDialog(const ui::ElementIdentifier& element_id);

  // Opens the Quick Settings bubble.
  ui::test::internal::InteractiveTestPrivate::MultiStep OpenQuickSettings();

  // Navigates to the internet page within Quick Settings. This function expects
  // the Quick Settings to already be open and on the root page.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateQuickSettingsToNetworkPage();

  // Navigates to the bluetooth page within Quick Settings. This function
  // expects the Quick Settings to already be open and on the root page.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateQuickSettingsToHotspotPage();

  // Navigates to the bluetooth page within Quick Settings. This function
  // expects the Quick Settings to already be open and on the root page.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateQuickSettingsToBluetoothPage();

  // Returns the active user profile.
  Profile* GetActiveUserProfile();

  // Convenience method to create a new browser window at `url` for the active
  // user profile. Returns the `NavigationHandle` for the started navigation,
  // which might be null if the navigation couldn't be started. Tests requiring
  // more complex browser setup should use `Navigate()` from
  // browser_navigator.h.
  base::WeakPtr<content::NavigationHandle> CreateBrowserWindow(const GURL& url);

  // MixinBasedInProcessBrowserTest:
  void TearDownOnMainThread() override;

  // Blocks until a window exists with the given title. If a matching window
  // already exists the test will resume immediately.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForWindowWithTitle(
      aura::Env* env,
      std::u16string title);

  // Waits for an element identified by `query` to exist in the DOM of an
  // instrumented WebUI identified by `element_id`.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& query);

  // Waits for an element identified by `query` to not exist in the DOM of an
  // instrumented WebUI identified by `element_id`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForElementDoesNotExist(const ui::ElementIdentifier& element_id,
                             const DeepQuery& query);

  // Waits for an element identified by `query` to both exist in the DOM of an
  // instrumented WebUI identified by `element_id` and be enabled.
  InteractiveTestApi::MultiStep WaitForElementEnabled(
      const ui::ElementIdentifier& element_id,
      WebContentsInteractionTestUtil::DeepQuery query);

  // Waits for an element identified by `query` to both exist in the DOM of an
  // instrumented WebUI identified by `element_id` and be disabled.
  InteractiveTestApi::MultiStep WaitForElementDisabled(
      const ui::ElementIdentifier& element_id,
      WebContentsInteractionTestUtil::DeepQuery query);

  // Waits for an element identified by `query` to both exist in the DOM of an
  // instrumented WebUI identified by `element_id` and be checked.
  InteractiveTestApi::MultiStep WaitForElementChecked(
      const ui::ElementIdentifier& element_id,
      WebContentsInteractionTestUtil::DeepQuery query);

  // Waits for an element identified by `query` to exist in the DOM of an
  // instrumented WebUI identified by `element_id` and be focused.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForElementFocused(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& query);

  // Waits for an element identified by `query` to both exist in the DOM of an
  // instrumented WebUI identified by `element_id` and have its text, or the
  // text of any of its children, match `expected`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForElementTextContains(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query,
      const std::string& expected);

  // This function is similar to `WaitForElementTextContains()` except it
  // supports non-unique elements. For more info see
  // `FindElementWithTextAndDoAction()`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForAnyElementTextContains(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& root,
      const WebContentsInteractionTestUtil::DeepQuery& selectors,
      const std::string& expected);

  // Waits for an element identified by `query` to both exist in the DOM of an
  // instrumented WebUI identified by `element_id` and have attribute
  // `attribute`.
  InteractiveTestApi::MultiStep WaitForElementHasAttribute(
      const ui::ElementIdentifier& element_id,
      WebContentsInteractionTestUtil::DeepQuery element,
      const std::string& attribute);

  // Waits for a toggle element identified by `query` to both exist in the DOM
  // of an instrumented WebUI identified by `element_id` and to be toggled .
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForToggleState(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query,
      bool is_checked);

  // Clears the text value of an input element identified by `query` in
  // the DOM of an instrumented WebUI identified by `element_id` .
  ui::test::internal::InteractiveTestPrivate::MultiStep ClearInputFieldValue(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query);

  // Waits for an element to render by using `getBoundingClientRect()` to verify
  // the element is visible and ready for interactions. Helps to prevent
  // `element_bounds.IsEmpty()` flakes.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForElementToRender(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& query);

  // Clicks on an element in the DOM. `element_id` is the identifier
  // of the WebContents to query. `query` is a DeepQuery path to the
  // element to start with, it can be {} to query the entire page.
  ui::test::internal::InteractiveTestPrivate::MultiStep ClickElement(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& query);

  // This function is similar to `ClickElement()` except it supports non-unique
  // elements. For more info see `FindElementWithTextAndDoAction()`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  ClickAnyElementTextContains(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& root,
      const WebContentsInteractionTestUtil::DeepQuery& selectors,
      const std::string& expected);

  // Waits for an element identified by `query` to exist in the DOM of an
  // instrumented WebUI identified by `element_id`. This function expects the
  // element to be a drop-down and will directly update the selected option
  // index to match the first option matching `option`.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  SelectDropdownElementOption(const ui::ElementIdentifier& element_id,
                              const DeepQuery& query,
                              const std::string& option);

  // Sends an instrumented WebUI identified by `element_id` the key presses
  // needed to input the provided text `text`. This function can handle ASCII
  // letters, numbers, the newline character, and a subset of symbols.
  // TODO(crbug.com/40286410) have a more supported way to do this and remove
  // this function.
  ui::test::internal::InteractiveTestPrivate::MultiStep SendTextAsKeyEvents(
      const ui::ElementIdentifier& element_id,
      const std::string& text);

 private:
  // Waits for an element identified by `root` to exist in the DOM of an
  // instrumented WebUI identified by `element_id`. When found, this function
  // will search for elements matching `selectors` and will execute `action` on
  // each element until `action` returns a truthy value.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  FindElementWithTextAndDoAction(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& root,
      const WebContentsInteractionTestUtil::DeepQuery& selectors,
      const std::string& action);

  // Helper function that navigates to a top-level page of the Settings app.
  // This function expects the Settings app to already be open. The `path`
  // parameter should correspond to a top-level menu item.
  ui::test::internal::InteractiveTestPrivate::MultiStep NavigateSettingsToPage(
      const ui::ElementIdentifier& element_id,
      const char* path);

  // Helper function that navigates to a detailed page within Quick Settings.
  // This function expects the Quick Settings to already be open and on the root
  // page. The `element_id` parameter should be the drill-in arrow for the
  // desired detailed page.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  NavigateQuickSettingsToPage(const ui::ElementIdentifier& element_id);
};

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_INTERACTIVE_ASH_TEST_H_
