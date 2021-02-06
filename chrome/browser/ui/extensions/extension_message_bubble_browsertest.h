// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BROWSERTEST_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BROWSERTEST_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"

namespace extensions {
class TestExtensionDir;
}

class ToolbarActionsModel;

class ExtensionMessageBubbleBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  enum AnchorPosition {
    ANCHOR_BROWSER_ACTION,
    ANCHOR_APP_MENU,
  };

  ExtensionMessageBubbleBrowserTest();
  ~ExtensionMessageBubbleBrowserTest() override;

  // extensions::ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Checks the position of the bubble present in the given |browser|, when the
  // bubble should be anchored at the given |anchor| and that the toolbar model
  // is correctly highlighting.
  void CheckBubble(Browser* browser,
                   AnchorPosition anchor,
                   bool should_be_highlighting);
  // Performs the platform-specific checks.
  virtual void CheckBubbleNative(Browser* browser, AnchorPosition anchor) = 0;

  // Closes the bubble present in the given |browser|.
  virtual void CloseBubble(Browser* browser);
  // Performs the platform-specific close.
  virtual void CloseBubbleNative(Browser* browser) = 0;

  // Checks that there is no active bubble for the given |browser|.
  // We specify whether or not the toolbar model should be highlighting or there
  // is a bubble active since another browser window may have an active bubble.
  void CheckBubbleIsNotPresent(Browser* browser,
                               bool should_profile_have_bubble,
                               bool should_be_highlighting);
  // Performs the platform-specific checks.
  virtual void CheckBubbleIsNotPresentNative(Browser* browser) = 0;

  // Clicks on the corresponding button in the bubble.
  virtual void ClickLearnMoreButton(Browser* browser) = 0;
  virtual void ClickActionButton(Browser* browser) = 0;
  virtual void ClickDismissButton(Browser* browser) = 0;

  // Adds a new extension that uses the chrome_settings_overrides api to
  // override a setting specified in |settings_override_value|.
  void AddSettingsOverrideExtension(const std::string& settings_override_value);

  // The following are essentially the different tests, but we can't define the
  // tests in this file, since it relies on platform-specific implementation
  // (the above virtual methods).

  // Tests that an extension bubble will be anchored to an extension action when
  // there are extensions with actions.
  void TestBubbleAnchoredToExtensionAction();

  // Tests that an extension bubble will be anchored to the app menu when there
  // aren't any extensions with actions.
  // This also tests that the crashes in crbug.com/476426 are fixed.
  void TestBubbleAnchoredToAppMenu();

  // Tests that an extension bubble will be anchored to the app menu if there
  // are no highlighted extensions, even if there's a benevolent extension with
  // an action.
  // Regression test for crbug.com/485614.
  void TestBubbleAnchoredToAppMenuWithOtherAction();

  // Tests that a displayed extension bubble will be closed after its associated
  // extension is uninstalled.
  void TestBubbleClosedAfterExtensionUninstall();

  // Tests that uninstalling the extension between when the bubble is originally
  // slated to show and when it does show is handled gracefully.
  // Regression test for crbug.com/531648.
  void TestUninstallDangerousExtension();

  // Tests that the extension bubble will show on startup.
  void PreBubbleShowsOnStartup();
  void TestBubbleShowsOnStartup();

  // Tests that the developer mode warning bubble is only shown once per
  // profile.
  // Regression test for crbug.com/607099.
  void TestDevModeBubbleIsntShownTwice();

  // Tests that the bubble indicating an extension is controlling a user's
  // new tab page is shown. When |click_learn_more| is true, the bubble is
  // closed by clicking the Learn More link, otherwise CloseBubble() is used.
  void TestControlledNewTabPageBubbleShown(bool click_learn_more);

  // Tests that the bubble indicating an extension is controlling a user's
  // home page is shown.
  void TestControlledHomeBubbleShown();

  // Tests that the bubble indicating an extension is controlling a user's
  // search engine is shown.
  void TestControlledSearchBubbleShown();

  // Tests that the bubble indicating an extension is controlling a user's
  // startup pages is shown.
  void PreTestControlledStartupBubbleShown();
  void TestControlledStartupBubbleShown();

  // Tests that the startup controlled bubble is *not* shown in the case of a
  // browser restart, since restarts always result in a session restore rather
  // than showing the normal startup pages.
  void PreTestControlledStartupNotShownOnRestart();
  void TestControlledStartupNotShownOnRestart();

  // Tests that having multiple windows, all of which could be vying to show a
  // warning bubble, behaves properly.
  void TestBubbleWithMultipleWindows();

  // Tests clicking on the corresponding button in the bubble view. The logic
  // for these is tested more thoroughly in the unit tests, but this ensures
  // that nothing goes wrong end-to-end.
  void TestClickingLearnMoreButton();
  void TestClickingActionButton();
  void TestClickingDismissButton();

  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }

 private:
  std::unique_ptr<extensions::FeatureSwitch::ScopedOverride>
      dev_mode_bubble_override_;

  // The backing directory for a custom extension loaded during a test. Null if
  // no custom extension is loaded.
  std::unique_ptr<extensions::TestExtensionDir> custom_extension_dir_;

  ToolbarActionsModel* toolbar_model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBrowserTest);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BROWSERTEST_H_
