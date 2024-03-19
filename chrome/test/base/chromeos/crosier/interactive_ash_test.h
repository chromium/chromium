// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_INTERACTIVE_ASH_TEST_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_INTERACTIVE_ASH_TEST_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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

  // Waits until the element or any of its children have the requested text.
  //
  // element_id
  //     The identifier of the WebContents to query.
  //
  // query
  //     The DeepQuery is a path to the element to start with, it can be {} to
  //     query the entire page.
  //
  // expected
  //     The text to search for.
  ui::test::internal::InteractiveTestPrivate::MultiStep
  WaitForElementTextContains(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query,
      const std::string& expected);

  // Waits for an element to render by using `getBoundingClientRect()` to verify
  // the element is visible and ready for interactions. Helps to prevent
  // `element_bounds.IsEmpty()` flakes.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForElementToRender(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& element);

  // Clicks on an element in the DOM. `element_id` is the identifier
  // of the WebContents to query. `query` is a DeepQuery path to the
  // element to start with, it can be {} to query the entire page.
  ui::test::internal::InteractiveTestPrivate::MultiStep ClickElement(
      const ui::ElementIdentifier& element_id,
      const DeepQuery& query);
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_INTERACTIVE_ASH_TEST_H_
