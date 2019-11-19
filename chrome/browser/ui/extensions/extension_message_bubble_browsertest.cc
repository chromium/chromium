// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

ExtensionMessageBubbleBrowserTest::ExtensionMessageBubbleBrowserTest() {
}

ExtensionMessageBubbleBrowserTest::~ExtensionMessageBubbleBrowserTest() {
}

void ExtensionMessageBubbleBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  BrowserActionsBarBrowserTest::SetUpCommandLine(command_line);
  // The dev mode warning bubble is an easy one to trigger, so we use that for
  // our testing purposes.
  dev_mode_bubble_override_.reset(
      new extensions::FeatureSwitch::ScopedOverride(
          extensions::FeatureSwitch::force_dev_mode_highlighting(),
          true));
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::OVERRIDE_ENABLED);
  ToolbarActionsBar::set_extension_bubble_appearance_wait_time_for_testing(0);
}

void ExtensionMessageBubbleBrowserTest::TearDownOnMainThread() {
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::NO_OVERRIDE);
  BrowserActionsBarBrowserTest::TearDownOnMainThread();
}

void ExtensionMessageBubbleBrowserTest::AddSettingsOverrideExtension(
    const std::string& settings_override_value) {
  DCHECK(!custom_extension_dir_);
  custom_extension_dir_ = std::make_unique<extensions::TestExtensionDir>();
  std::string manifest = base::StringPrintf(
    "{\n"
    "  'name': 'settings override',\n"
    "  'version': '0.1',\n"
    "  'manifest_version': 2,\n"
    "  'description': 'controls settings',\n"
    "  'chrome_settings_overrides': {\n"
    "    %s\n"
    "  }\n"
    "}", settings_override_value.c_str());
  custom_extension_dir_->WriteManifestWithSingleQuotes(manifest);
  ASSERT_TRUE(LoadExtension(custom_extension_dir_->UnpackedPath()));
}

void ExtensionMessageBubbleBrowserTest::CheckBubble(
    Browser* browser,
    AnchorPosition position,
    bool should_be_highlighting) {
  EXPECT_EQ(should_be_highlighting, toolbar_model()->is_highlighting());
  EXPECT_TRUE(toolbar_model()->has_active_bubble());
  EXPECT_TRUE(browser->window()->GetToolbarActionsBar()->is_showing_bubble());
  CheckBubbleNative(browser, position);
}

void ExtensionMessageBubbleBrowserTest::CheckBubbleIsNotPresent(
    Browser* browser,
    bool should_profile_have_bubble,
    bool should_be_highlighting) {
  // We should never be highlighting without an active bubble.
  ASSERT_TRUE(!should_be_highlighting || should_profile_have_bubble);
  EXPECT_EQ(should_be_highlighting, toolbar_model()->is_highlighting());
  EXPECT_EQ(should_profile_have_bubble, toolbar_model()->has_active_bubble());
  EXPECT_FALSE(browser->window()->GetToolbarActionsBar()->is_showing_bubble());
  CheckBubbleIsNotPresentNative(browser);
}

void ExtensionMessageBubbleBrowserTest::CloseBubble(Browser* browser) {
  CloseBubbleNative(browser);
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(browser, false, false);
}

void ExtensionMessageBubbleBrowserTest::TestBubbleAnchoredToExtensionAction() {
  scoped_refptr<const extensions::Extension> action_extension =
      extensions::ExtensionBuilder("action_extension")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::UNPACKED)
          .Build();
  extension_service()->AddExtension(action_extension.get());

  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();

  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION, true);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::TestBubbleAnchoredToAppMenu() {
  scoped_refptr<const extensions::Extension> no_action_extension =
      extensions::ExtensionBuilder("no_action_extension")
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(no_action_extension.get());
  // The 'suspicious extension' bubble warns the user about extensions that are
  // disabled for not being from the webstore. This is one of the few bubbles
  // that lets us test anchoring to the app menu, since we usually anchor to the
  // extension action now that every extension is given a permanent UI presence.
  extension_service()->DisableExtension(
      no_action_extension->id(),
      extensions::disable_reason::DISABLE_NOT_VERIFIED);
  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();
  CheckBubble(second_browser, ANCHOR_APP_MENU, false);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::
    TestBubbleAnchoredToAppMenuWithOtherAction() {
  scoped_refptr<const extensions::Extension> no_action_extension =
      extensions::ExtensionBuilder("no_action_extension")
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(no_action_extension.get());

  scoped_refptr<const extensions::Extension> action_extension =
      extensions::ExtensionBuilder("action_extension")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(action_extension.get());

  // The 'suspicious extension' bubble warns the user about extensions that are
  // disabled for not being from the webstore. This is one of the few bubbles
  // that lets us test anchoring to the app menu, since we usually anchor to the
  // extension action now that every extension is given a permanent UI presence.
  extension_service()->DisableExtension(
      no_action_extension->id(),
      extensions::disable_reason::DISABLE_NOT_VERIFIED);

  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();

  CheckBubble(second_browser, ANCHOR_APP_MENU, false);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::
    TestBubbleClosedAfterExtensionUninstall() {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("override")
                        .AppendASCII("newtab"));
  ASSERT_TRUE(extension);

  CheckBubbleIsNotPresent(browser(), false, false);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  base::RunLoop().RunUntilIdle();
  CheckBubble(browser(), ANCHOR_BROWSER_ACTION, false);

  extension_service()->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  base::RunLoop().RunUntilIdle();

  // If the only relevant extension was uninstalled, the bubble should
  // automatically close. See crbug.com/748952.
  CheckBubbleIsNotPresent(browser(), false, false);
}

void ExtensionMessageBubbleBrowserTest::TestUninstallDangerousExtension() {
  // Load an extension that overrides the proxy setting.
  ExtensionTestMessageListener listener("registered", false);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("proxy")
                        .AppendASCII("register"));
  // Wait for it to complete.
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Create a second browser with the extension installed - the bubble will be
  // set to show.
  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  // Uninstall the extension before the bubble is shown. This should not crash,
  // and the bubble shouldn't be shown.
  extension_service()->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(second_browser, false, false);
}

void ExtensionMessageBubbleBrowserTest::PreBubbleShowsOnStartup() {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));
}

void ExtensionMessageBubbleBrowserTest::TestBubbleShowsOnStartup() {
  base::RunLoop().RunUntilIdle();
  CheckBubble(browser(), ANCHOR_BROWSER_ACTION, true);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::TestDevModeBubbleIsntShownTwice() {
  scoped_refptr<const extensions::Extension> action_extension =
      extensions::ExtensionBuilder("action_extension")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::UNPACKED)
          .Build();
  extension_service()->AddExtension(action_extension.get());

  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();

  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION, true);
  CloseBubble(second_browser);
  base::RunLoop().RunUntilIdle();

  // The bubble was already shown, so it shouldn't be shown again.
  Browser* third_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(third_browser);
  third_browser->window()->Show();
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(third_browser, false, false);
}

void ExtensionMessageBubbleBrowserTest::TestControlledNewTabPageBubbleShown(
    bool click_learn_more) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("override")
                                          .AppendASCII("newtab")));
  CheckBubbleIsNotPresent(browser(), false, false);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  base::RunLoop().RunUntilIdle();
  CheckBubble(browser(), ANCHOR_BROWSER_ACTION, false);
  if (click_learn_more) {
    ClickLearnMoreButton(browser());
    EXPECT_EQ(3, browser()->tab_strip_model()->count());
  } else {
    CloseBubble(browser());
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
  }
}

void ExtensionMessageBubbleBrowserTest::TestControlledHomeBubbleShown() {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

  const char kHomePage[] = "'homepage': 'https://www.google.com'\n";
  AddSettingsOverrideExtension(kHomePage);

  CheckBubbleIsNotPresent(browser(), false, false);

  chrome::ExecuteCommandWithDisposition(
      browser(), IDC_HOME, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  base::RunLoop().RunUntilIdle();

  CheckBubble(browser(), ANCHOR_BROWSER_ACTION, false);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::TestControlledSearchBubbleShown() {
  const char kSearchProvider[] =
      "'search_provider': {\n"
      "  'search_url': 'https://www.google.com/search?q={searchTerms}',\n"
      "  'is_default': true,\n"
      "  'favicon_url': 'https://www.google.com/favicon.icon',\n"
      "  'keyword': 'TheGoogs',\n"
      "  'name': 'Google',\n"
      "  'encoding': 'UTF-8'\n"
      "}\n";
  AddSettingsOverrideExtension(kSearchProvider);

  CheckBubbleIsNotPresent(browser(), false, false);

  OmniboxView* omnibox =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox->OnBeforePossibleChange();
  omnibox->SetUserText(base::ASCIIToUTF16("search for this"));
  omnibox->OnAfterPossibleChange(true);
  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);
  base::RunLoop().RunUntilIdle();

  CheckBubble(browser(), ANCHOR_BROWSER_ACTION, false);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::PreTestControlledStartupBubbleShown() {
  ASSERT_TRUE(InstallExtensionWithPermissionsGranted(
      test_data_dir_.AppendASCII("startup_pages"), 1));
}

void ExtensionMessageBubbleBrowserTest::TestControlledStartupBubbleShown() {
  base::RunLoop().RunUntilIdle();
  CheckBubble(browser(), ANCHOR_BROWSER_ACTION, true);
  CloseBubble(browser());
}

void ExtensionMessageBubbleBrowserTest::
    PreTestControlledStartupNotShownOnRestart() {
  ASSERT_TRUE(InstallExtensionWithPermissionsGranted(
      test_data_dir_.AppendASCII("startup_pages"), 1));
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
}

void ExtensionMessageBubbleBrowserTest::
    TestControlledStartupNotShownOnRestart() {
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
  CheckBubbleIsNotPresent(browser(), false, false);
}

void ExtensionMessageBubbleBrowserTest::TestBubbleWithMultipleWindows() {
  CheckBubbleIsNotPresent(browser(), false, false);
  LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));
  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  Browser* third_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(third_browser);
  third_browser->window()->Show();
  Browser* fourth_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(fourth_browser);
  fourth_browser->window()->Show();
  base::RunLoop().RunUntilIdle();
  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION, true);
  // Even though the bubble isn't present on these browser windows, highlighting
  // is per-profile.
  CheckBubbleIsNotPresent(browser(), true, true);
  CheckBubbleIsNotPresent(third_browser, true, true);
  CheckBubbleIsNotPresent(fourth_browser, true, true);
  CloseBubble(second_browser);
}

void ExtensionMessageBubbleBrowserTest::TestClickingLearnMoreButton() {
  CheckBubbleIsNotPresent(browser(), false, false);
  scoped_refptr<const extensions::Extension> no_action_extension =
      extensions::ExtensionBuilder("no_action_extension")
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extension_service()->AddExtension(no_action_extension.get());

  extension_service()->DisableExtension(
      no_action_extension->id(),
      extensions::disable_reason::DISABLE_NOT_VERIFIED);

  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();
  ClickLearnMoreButton(second_browser);
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(second_browser, false, false);
  // The learn more link goes to the information page about 'suspicious
  // extensions', so it should be opened in the active tab.
  content::WebContents* active_web_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(active_web_contents);
  EXPECT_EQ(GURL(chrome::kRemoveNonCWSExtensionURL),
            active_web_contents->GetLastCommittedURL());
}

void ExtensionMessageBubbleBrowserTest::TestClickingActionButton() {
  CheckBubbleIsNotPresent(browser(), false, false);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  std::string id = extension->id();
  EXPECT_TRUE(registry->enabled_extensions().GetByID(id));
  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();
  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION, true);
  ClickActionButton(second_browser);
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(browser(), false, false);
  // Clicking the action button disabled the extension.
  EXPECT_FALSE(registry->enabled_extensions().GetByID(id));
}

void ExtensionMessageBubbleBrowserTest::TestClickingDismissButton() {
  CheckBubbleIsNotPresent(browser(), false, false);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  std::string id = extension->id();
  EXPECT_TRUE(registry->enabled_extensions().GetByID(id));
  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  ASSERT_TRUE(second_browser);
  second_browser->window()->Show();
  base::RunLoop().RunUntilIdle();
  CheckBubble(second_browser, ANCHOR_BROWSER_ACTION, true);
  ClickDismissButton(second_browser);
  base::RunLoop().RunUntilIdle();
  CheckBubbleIsNotPresent(browser(), false, false);
  // Clicking dismiss should have no affect, so the extension should still be
  // active.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(id));
}
