// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_unittest.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/user_script.h"
#include "ui/base/l10n/l10n_util.h"

// Tests the icon appearance of extension actions with the toolbar redesign.
// Extensions that don't want to run should have their icons grayscaled.
// Overflowed extensions that want to run should have an additional decoration.
TEST_P(ToolbarActionsBarUnitTest, ExtensionActionWantsToRunAppearance) {
  CreateAndAddExtension("extension",
                        extensions::ExtensionBuilder::ActionType::PAGE_ACTION);
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(0u, overflow_bar()->GetIconCount());

  AddTab(browser(), GURL("chrome://newtab"));

  const gfx::Size size = toolbar_actions_bar()->GetViewSize();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionViewController* action =
      static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[0]);
  std::unique_ptr<IconWithBadgeImageSource> image_source =
      action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, true);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  toolbar_model()->SetVisibleIconCount(0u);
  EXPECT_EQ(0u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(1u, overflow_bar()->GetIconCount());

  action = static_cast<ExtensionActionViewController*>(
      overflow_bar()->GetActions()[0]);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_TRUE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, false);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
}

TEST_P(ToolbarActionsBarUnitTest, ExtensionActionBlockedActions) {
  // Blocked actions are only present with the runtime host permissions feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  scoped_refptr<const extensions::Extension> browser_action_ext =
      extensions::ExtensionBuilder("browser action")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .AddPermission("https://www.google.com/*")
          .Build();

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->GrantPermissions(browser_action_ext.get());
  service->AddExtension(browser_action_ext.get());
  extensions::ScriptingPermissionsModifier permissions_modifier_browser_ext(
      profile(), browser_action_ext);
  permissions_modifier_browser_ext.SetWithholdHostPermissions(true);

  ASSERT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  AddTab(browser(), GURL("https://www.google.com/"));

  ExtensionActionViewController* browser_action =
      static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[0]);
  EXPECT_EQ(browser_action_ext.get(), browser_action->extension());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  const gfx::Size size = toolbar_actions_bar()->GetViewSize();
  std::unique_ptr<IconWithBadgeImageSource> image_source =
      browser_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);
  action_runner->RequestScriptInjectionForTesting(
      browser_action_ext.get(), extensions::UserScript::DOCUMENT_IDLE,
      base::DoNothing());
  image_source =
      browser_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_TRUE(image_source->paint_blocked_actions_decoration());

  action_runner->RunForTesting(browser_action_ext.get());
  image_source =
      browser_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  scoped_refptr<const extensions::Extension> page_action_ext =
      extensions::ExtensionBuilder("page action")
          .SetAction(extensions::ExtensionBuilder::ActionType::PAGE_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .AddPermission("https://www.google.com/*")
          .Build();
  service->GrantPermissions(page_action_ext.get());
  service->AddExtension(page_action_ext.get());
  extensions::ScriptingPermissionsModifier permissions_modifier_page_action(
      profile(), page_action_ext);
  permissions_modifier_page_action.SetWithholdHostPermissions(true);

  ASSERT_EQ(2u, toolbar_actions_bar()->GetIconCount());
  ExtensionActionViewController* page_action =
      static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[1]);
  EXPECT_EQ(browser_action_ext.get(), browser_action->extension());

  image_source = page_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  action_runner->RequestScriptInjectionForTesting(
      page_action_ext.get(), extensions::UserScript::DOCUMENT_IDLE,
      base::DoNothing());
  image_source = page_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_TRUE(image_source->paint_blocked_actions_decoration());

  // Overflow the page action and set the page action as wanting to run. We
  // shouldn't show the page action decoration because we are showing the
  // blocked action decoration (and should only show one at a time).
  toolbar_model()->SetVisibleIconCount(0u);
  EXPECT_EQ(0u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(2u, overflow_bar()->GetIconCount());
  ExtensionActionViewController* overflow_page_action =
      static_cast<ExtensionActionViewController*>(
          overflow_bar()->GetActions()[1]);
  SetActionWantsToRunOnTab(overflow_page_action->extension_action(),
                           web_contents, true);
  image_source =
      overflow_page_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_TRUE(image_source->paint_blocked_actions_decoration());

  SetActionWantsToRunOnTab(overflow_page_action->extension_action(),
                           web_contents, false);
  toolbar_model()->SetVisibleIconCount(2u);

  action_runner->RunForTesting(page_action_ext.get());
  image_source = page_action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_page_action_decoration());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
}

TEST_P(ToolbarActionsBarUnitTest, ExtensionActionContextMenu) {
  CreateAndAddExtension(
      "extension", extensions::ExtensionBuilder::ActionType::BROWSER_ACTION);
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // Check that the context menu has the proper string for the action's position
  // (in the main toolbar, in the overflow container, or temporarily popped
  // out).
  auto check_visibility_string = [](ToolbarActionViewController* action,
                                    int expected_visibility_string) {
    ui::SimpleMenuModel* context_menu =
        static_cast<ui::SimpleMenuModel*>(action->GetContextMenu());
    int visibility_index = context_menu->GetIndexOfCommandId(
        extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
    ASSERT_GE(visibility_index, 0);
    base::string16 visibility_label =
        context_menu->GetLabelAt(visibility_index);
    EXPECT_EQ(l10n_util::GetStringUTF16(expected_visibility_string),
              visibility_label);
  };

  check_visibility_string(toolbar_actions_bar()->GetActions()[0],
                          IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU);
  toolbar_model()->SetVisibleIconCount(0u);
  check_visibility_string(overflow_bar()->GetActions()[0],
                          IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR);
  base::RunLoop run_loop;
  toolbar_actions_bar()->PopOutAction(toolbar_actions_bar()->GetActions()[0],
                                      false,
                                      run_loop.QuitClosure());
  run_loop.Run();
  check_visibility_string(toolbar_actions_bar()->GetActions()[0],
                          IDS_EXTENSIONS_KEEP_BUTTON_IN_TOOLBAR);
}

class ExtensionActionViewControllerGrayscaleTest
    : public ToolbarActionsBarUnitTest {
 public:
  enum class PermissionType {
    kScriptableHost,
    kExplicitHost,
  };

  ExtensionActionViewControllerGrayscaleTest() {}
  ~ExtensionActionViewControllerGrayscaleTest() override = default;

  void RunGrayscaleTest(PermissionType permission_type);

 private:
  scoped_refptr<const extensions::Extension> CreateExtension(
      PermissionType permission_type);

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionViewControllerGrayscaleTest);
};

void ExtensionActionViewControllerGrayscaleTest::RunGrayscaleTest(
    PermissionType permission_type) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(permission_type);
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->GrantPermissions(extension.get());
  service->AddExtension(extension.get());

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  ASSERT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  const GURL kUrl("https://www.google.com/");
  AddTab(browser(), kUrl);

  enum class ActionState {
    kEnabled,
    kDisabled,
  };
  enum class PageAccess {
    kGranted,
    kPending,
    kNone,
  };
  enum class Opacity {
    kGrayscale,
    kFull,
  };
  enum class BlockedActions {
    kPainted,
    kNotPainted,
  };

  struct {
    ActionState action_state;
    PageAccess page_access;
    Opacity expected_opacity;
    BlockedActions expected_blocked_actions;
  } test_cases[] = {
      {ActionState::kEnabled, PageAccess::kNone, Opacity::kFull,
       BlockedActions::kNotPainted},
      {ActionState::kEnabled, PageAccess::kPending, Opacity::kFull,
       BlockedActions::kPainted},
      {ActionState::kEnabled, PageAccess::kGranted, Opacity::kFull,
       BlockedActions::kNotPainted},

      {ActionState::kDisabled, PageAccess::kNone, Opacity::kGrayscale,
       BlockedActions::kNotPainted},
      {ActionState::kDisabled, PageAccess::kPending, Opacity::kFull,
       BlockedActions::kPainted},
      {ActionState::kDisabled, PageAccess::kGranted, Opacity::kFull,
       BlockedActions::kNotPainted},
  };

  auto* controller = static_cast<ExtensionActionViewController*>(
      toolbar_actions_bar()->GetActions()[0]);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionAction* extension_action =
      extensions::ExtensionActionManager::Get(profile())->GetExtensionAction(
          *extension);
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();
  const gfx::Size kSize = toolbar_actions_bar()->GetViewSize();

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("Running test case %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];

    // Set up the proper state.
    extension_action->SetIsVisible(
        tab_id, test_case.action_state == ActionState::kEnabled);
    switch (test_case.page_access) {
      case PageAccess::kNone: {
        // Page access should already be withheld; verify.
        extensions::PermissionsData::PageAccess page_access =
            extensions::PermissionsData::PageAccess::kDenied;
        switch (permission_type) {
          case PermissionType::kExplicitHost:
            page_access = extension->permissions_data()->GetPageAccess(
                kUrl, tab_id, /*error=*/nullptr);
            break;
          case PermissionType::kScriptableHost:
            page_access = extension->permissions_data()->GetContentScriptAccess(
                kUrl, tab_id, /*error=*/nullptr);
            break;
        }
        EXPECT_EQ(extensions::PermissionsData::PageAccess::kWithheld,
                  page_access);
        break;
      }
      case PageAccess::kPending:
        action_runner->RequestScriptInjectionForTesting(
            extension.get(), extensions::UserScript::DOCUMENT_IDLE,
            base::DoNothing());
        break;
      case PageAccess::kGranted:
        permissions_modifier.GrantHostPermission(kUrl);
        break;
    }

    std::unique_ptr<IconWithBadgeImageSource> image_source =
        controller->GetIconImageSourceForTesting(web_contents, kSize);
    EXPECT_EQ(test_case.expected_opacity == Opacity::kGrayscale,
              image_source->grayscale());
    EXPECT_EQ(test_case.expected_blocked_actions == BlockedActions::kPainted,
              image_source->paint_blocked_actions_decoration());

    // Clean up permissions state.
    if (test_case.page_access == PageAccess::kGranted)
      permissions_modifier.RemoveGrantedHostPermission(kUrl);
    action_runner->ClearInjectionsForTesting(*extension);
  }
}

scoped_refptr<const extensions::Extension>
ExtensionActionViewControllerGrayscaleTest::CreateExtension(
    PermissionType permission_type) {
  extensions::ExtensionBuilder builder("extension");
  builder.SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
      .SetLocation(extensions::Manifest::INTERNAL);
  switch (permission_type) {
    case PermissionType::kScriptableHost: {
      std::unique_ptr<base::Value> content_scripts = base::JSONReader::Read(
          R"([{
                     "matches": ["https://www.google.com/*"],
                     "js": ["script.js"]
                 }])");
      builder.SetManifestKey("content_scripts", std::move(content_scripts));
      break;
    }
    case PermissionType::kExplicitHost:
      builder.AddPermission("https://www.google.com/*");
      break;
  }

  return builder.Build();
}

// Tests the behavior for icon grayscaling with the runtime host permissions
// feature enabled. Ideally, these would be a single parameterized test, but
// toolbar tests are already parameterized with the UI mode.
TEST_P(ExtensionActionViewControllerGrayscaleTest,
       GrayscaleIcon_ExplicitHosts) {
  RunGrayscaleTest(PermissionType::kExplicitHost);
}
TEST_P(ExtensionActionViewControllerGrayscaleTest,
       GrayscaleIcon_ScriptableHosts) {
  RunGrayscaleTest(PermissionType::kScriptableHost);
}

INSTANTIATE_TEST_CASE_P(,
                        ExtensionActionViewControllerGrayscaleTest,
                        testing::Values(false, true));

TEST_P(ToolbarActionsBarUnitTest, RuntimeHostsTooltip) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension name")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .AddPermission("https://www.google.com/*")
          .Build();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->GrantPermissions(extension.get());
  service->AddExtension(extension.get());

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  ASSERT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  const GURL kUrl("https://www.google.com/");
  AddTab(browser(), kUrl);

  auto* controller = static_cast<ExtensionActionViewController*>(
      toolbar_actions_bar()->GetActions()[0]);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();

  // Page access should already be withheld.
  EXPECT_EQ(extensions::PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetPageAccess(kUrl, tab_id,
                                                         /*error=*/nullptr));
  EXPECT_EQ("extension name",
            base::UTF16ToUTF8(controller->GetTooltip(web_contents)));

  // Request access.
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  action_runner->RequestScriptInjectionForTesting(
      extension.get(), extensions::UserScript::DOCUMENT_IDLE,
      base::DoNothing());
  EXPECT_EQ("extension name\nWants access to this site",
            base::UTF16ToUTF8(controller->GetTooltip(web_contents)));

  // Grant access.
  action_runner->ClearInjectionsForTesting(*extension);
  permissions_modifier.GrantHostPermission(kUrl);
  EXPECT_EQ("extension name\nHas access to this site",
            base::UTF16ToUTF8(controller->GetTooltip(web_contents)));
}

// ExtensionActionViewController::GetIcon() can potentially be called with a
// null web contents if the tab strip model doesn't know of an active tab
// (though it's a bit unclear when this is the case).
// See https://crbug.com/888121
TEST_P(ToolbarActionsBarUnitTest, TestGetIconWithNullWebContents) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension name")
          .SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION)
          .AddPermission("https://example.com/")
          .Build();

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->GrantPermissions(extension.get());
  service->AddExtension(extension.get());

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
  permissions_modifier.SetWithholdHostPermissions(true);

  // Try getting an icon with no active web contents. Nothing should crash, and
  // a non-empty icon should be returned.
  ToolbarActionViewController* controller =
      toolbar_actions_bar()->GetActions()[0];
  gfx::Image icon =
      controller->GetIcon(nullptr, toolbar_actions_bar()->GetViewSize());
  EXPECT_FALSE(icon.IsEmpty());
}
