// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/native_theme/native_theme.h"

using extensions::mojom::ManifestLocation;
using SiteInteraction = extensions::SitePermissionsHelper::SiteInteraction;
using UserSiteSetting = extensions::PermissionsManager::UserSiteSetting;
using HoverCardState = ToolbarActionViewController::HoverCardState;

class ExtensionActionViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionActionViewControllerBrowserTest() = default;
  ExtensionActionViewControllerBrowserTest(
      const ExtensionActionViewControllerBrowserTest& other) = delete;
  ExtensionActionViewControllerBrowserTest& operator=(
      const ExtensionActionViewControllerBrowserTest& other) = delete;

  ~ExtensionActionViewControllerBrowserTest() override = default;

  void Init() { AddTab(browser(), GURL(u"https://example.com")); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void AddTab(Browser* browser, GURL gurl) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, gurl, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateAndCommitActiveTab(const GURL& gurl) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), gurl, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Sets whether the given |action| wants to run on the |web_contents|.
  void SetActionWantsToRunOnTab(extensions::ExtensionAction* action,
                                content::WebContents* web_contents,
                                bool wants_to_run) {
    action->SetIsVisible(
        sessions::SessionTabHelper::IdForTab(web_contents).id(), wants_to_run);
    extensions::ExtensionActionAPI::Get(browser()->profile())
        ->NotifyChange(action, web_contents, browser()->profile());
  }

  // Returns the active WebContents for the primary browser.
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ExtensionActionViewController* GetViewControllerForId(
      const std::string& action_id) {
    // It's safe to static cast here, because these tests only deal with
    // extensions.
    return static_cast<ExtensionActionViewController*>(
        container()->GetActionForId(action_id));
  }

  scoped_refptr<const extensions::Extension> CreateAndAddExtension(
      const std::string& name,
      extensions::ActionInfo::Type action_type) {
    return CreateAndAddExtensionWithGrantedHostPermissions(name, action_type,
                                                           {});
  }

  scoped_refptr<const extensions::Extension>
  CreateAndAddExtensionWithGrantedHostPermissions(
      const std::string& name,
      extensions::ActionInfo::Type action_type,
      const std::vector<std::string>& permissions) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetAction(action_type)
            .SetLocation(ManifestLocation::kInternal)
            .AddHostPermissions(permissions)
            .Build();

    if (!permissions.empty()) {
      extension_service()->GrantPermissions(extension.get());
    }

    extension_service()->AddExtension(extension.get());
    return extension;
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service();
  }
  ToolbarActionsModel* toolbar_model() {
    return ToolbarActionsModel::Get(browser()->profile());
  }

  ExtensionsToolbarContainer* container() {
    return browser()->GetBrowserView().toolbar()->extensions_container();
  }

  // The standard size associated with a toolbar action view.
  gfx::Size view_size() { return container()->GetToolbarActionSize(); }
};

// Tests the icon appearance of extension actions in the toolbar.
// Extensions that don't want to run should have their icons grayscaled.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       ExtensionActionWantsToRunAppearance) {
  Init();
  const std::string id =
      CreateAndAddExtension("extension", extensions::ActionInfo::Type::kPage)
          ->id();

  content::WebContents* web_contents = GetActiveWebContents();
  ExtensionActionViewController* const action = GetViewControllerForId(id);
  ASSERT_TRUE(action);
  std::unique_ptr<IconWithBadgeImageSource> image_source =
      action->GetIconImageSourceForTesting(web_contents, view_size());
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, true);
  image_source =
      action->GetIconImageSourceForTesting(web_contents, view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
}

// Tests the appearance of browser actions with blocked script actions.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       BrowserActionBlockedActions) {
  Init();
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "browser_action", extensions::ActionInfo::Type::kBrowser,
      {"https://www.google.com/*"});

  extensions::ScriptingPermissionsModifier permissions_modifier(
      browser()->profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);

  AddTab(browser(), GURL("https://www.google.com/"));

  ExtensionActionViewController* const action_controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(action_controller);
  EXPECT_EQ(extension.get(), action_controller->extension());

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  std::unique_ptr<IconWithBadgeImageSource> image_source =
      action_controller->GetIconImageSourceForTesting(web_contents,
                                                      view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);
  action_runner->RequestScriptInjectionForTesting(
      extension.get(), extensions::mojom::RunLocation::kDocumentIdle,
      base::DoNothing());
  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_TRUE(image_source->paint_blocked_actions_decoration());

  action_runner->RunForTesting(extension.get());
  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
}

// Tests the appearance of page actions with blocked script actions.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       PageActionBlockedActions) {
  Init();
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "page_action", extensions::ActionInfo::Type::kPage,
      {"https://www.google.com/*"});

  extensions::ScriptingPermissionsModifier permissions_modifier(
      browser()->profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  AddTab(browser(), GURL("https://www.google.com/"));

  ExtensionActionViewController* const action_controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(action_controller);
  EXPECT_EQ(extension.get(), action_controller->extension());

  content::WebContents* web_contents = GetActiveWebContents();
  std::unique_ptr<IconWithBadgeImageSource> image_source =
      action_controller->GetIconImageSourceForTesting(web_contents,
                                                      view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  action_runner->RequestScriptInjectionForTesting(
      extension.get(), extensions::mojom::RunLocation::kDocumentIdle,
      base::DoNothing());
  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_TRUE(image_source->paint_blocked_actions_decoration());

  // Simulate NativeTheme update after `image_source` is created.
  // `image_source` should paint fine without hitting use-after-free in such
  // case.  See http://crbug.com/1315967
  ui::NativeTheme* theme = ui::NativeTheme::GetInstanceForNativeUi();
  theme->NotifyOnNativeThemeUpdated();
  image_source->GetImageForScale(1.0f);
}

// Tests the appearance of extension actions for extensions without a browser or
// page action defined in their manifest, but with host permissions on a page.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       OnlyHostPermissionsAppearance) {
  Init();
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("just hosts")
          .SetLocation(ManifestLocation::kInternal)
          .AddHostPermission("https://www.google.com/*")
          .Build();

  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());
  extensions::ScriptingPermissionsModifier permissions_modifier(
      browser()->profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);

  ExtensionActionViewController* const action_controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(action_controller);
  EXPECT_EQ(extension.get(), action_controller->extension());

  // Initially load on a site that the extension doesn't have permissions to.
  AddTab(browser(), GURL("https://www.chromium.org/"));
  content::WebContents* web_contents = GetActiveWebContents();

  std::unique_ptr<IconWithBadgeImageSource> image_source =
      action_controller->GetIconImageSourceForTesting(web_contents,
                                                      view_size());
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(action_controller->IsEnabled(web_contents));
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
  EXPECT_EQ(u"just hosts", action_controller->GetTooltip(web_contents));

  // Navigate to a url the extension does have permissions to. The extension is
  // set to run on click and has the current URL withheld, so it should not be
  // grayscaled and should be clickable.
  NavigateAndCommitActiveTab(GURL("https://www.google.com/"));
  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_TRUE(action_controller->IsEnabled(web_contents));
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
  EXPECT_EQ(u"just hosts\nWants access to this site",
            action_controller->GetTooltip(web_contents));

  // After triggering the action it should have access, which is reflected in
  // the tooltip.
  action_controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kToolbarButton);
  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(action_controller->IsEnabled(web_contents));
  EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
  EXPECT_EQ(u"just hosts\nHas access to this site",
            action_controller->GetTooltip(web_contents));
}

IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       ExtensionActionContextMenuVisibility) {
  Init();
  std::string id =
      CreateAndAddExtension("extension", extensions::ActionInfo::Type::kBrowser)
          ->id();

  // Check that the context menu has the proper string for the action's pinned
  // state.
  auto check_visibility_string = [](ToolbarActionViewController* action,
                                    int expected_visibility_string) {
    ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
        action->GetContextMenu(extensions::ExtensionContextMenuModel::
                                   ContextMenuSource::kToolbarAction));
    std::optional<size_t> visibility_index = context_menu->GetIndexOfCommandId(
        extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
    ASSERT_TRUE(visibility_index.has_value());
    std::u16string visibility_label =
        context_menu->GetLabelAt(visibility_index.value());
    EXPECT_EQ(l10n_util::GetStringUTF16(expected_visibility_string),
              visibility_label);
  };

  ExtensionActionViewController* const action = GetViewControllerForId(id);
  ASSERT_TRUE(action);

  // Default state: unpinned.
  check_visibility_string(action, IDS_EXTENSIONS_PIN_TO_TOOLBAR);

  // Pin the extension; re-check.
  toolbar_model()->SetActionVisibility(id, true);
  check_visibility_string(action, IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR);

  // Unpin the extension and ephemerally pop it out.
  toolbar_model()->SetActionVisibility(id, false);
  EXPECT_FALSE(container()->IsActionVisibleOnToolbar(id));
  base::RunLoop run_loop;
  container()->PopOutAction(id, run_loop.QuitClosure());
  EXPECT_TRUE(container()->IsActionVisibleOnToolbar(id));
  // The string should still just be "pin".
  check_visibility_string(action, IDS_EXTENSIONS_PIN_TO_TOOLBAR);
}

enum class PermissionType {
  kScriptableHost,
  kExplicitHost,
};

class ExtensionActionViewControllerGrayscaleTest
    : public ExtensionActionViewControllerBrowserTest,
      public testing::WithParamInterface<PermissionType> {
 public:
  enum class ActionState {
    kEnabled,
    kDisabled,
  };

  enum class PageAccessStatus {
    // The extension has been granted permission to the host.
    kGranted,
    // The extension had the host withheld and it has not tried to access the
    // page.
    kWithheld,
    // The extension had the host withheld and it has been blocked when trying
    // to access the page.
    kBlocked,
    // The extension has not been granted permissions to the host, nor was it
    // withheld.
    kNone,
  };

  enum class Coloring {
    // The extension action color is grayscale.
    kGrayscale,
    // The extension action has full color.
    kFull,
  };

  enum class BlockedDecoration {
    // The extension is blocked, thus its action has painted decoration.
    kPainted,
    // The extension is not blocked, thus its action has no painted decoration.
    kNotPainted,
  };

  ExtensionActionViewControllerGrayscaleTest() = default;

  ExtensionActionViewControllerGrayscaleTest(
      const ExtensionActionViewControllerGrayscaleTest&) = delete;
  ExtensionActionViewControllerGrayscaleTest& operator=(
      const ExtensionActionViewControllerGrayscaleTest&) = delete;

  ~ExtensionActionViewControllerGrayscaleTest() override = default;

  scoped_refptr<const extensions::Extension> CreateExtension(
      PermissionType permission_type,
      const std::string& host_permission);

  extensions::PermissionsData::PageAccess GetPageAccess(
      content::WebContents* web_contents,
      scoped_refptr<const extensions::Extension> extensions,
      PermissionType permission_type);
};

scoped_refptr<const extensions::Extension>
ExtensionActionViewControllerGrayscaleTest::CreateExtension(
    PermissionType permission_type,
    const std::string& host_permission) {
  extensions::ExtensionBuilder builder("extension");
  builder.SetAction(extensions::ActionInfo::Type::kBrowser)
      .SetLocation(ManifestLocation::kInternal);
  switch (permission_type) {
    case PermissionType::kScriptableHost:
      builder.AddContentScript("script.js", {host_permission});
      break;
    case PermissionType::kExplicitHost:
      builder.AddHostPermission(host_permission);
      break;
  }

  return builder.Build();
}

extensions::PermissionsData::PageAccess
ExtensionActionViewControllerGrayscaleTest::GetPageAccess(
    content::WebContents* web_contents,
    scoped_refptr<const extensions::Extension> extension,
    PermissionType permission_type) {
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  GURL url = web_contents->GetLastCommittedURL();
  switch (permission_type) {
    case PermissionType::kExplicitHost:
      return extension->permissions_data()->GetPageAccess(url, tab_id,
                                                          /*error=*/nullptr);
    case PermissionType::kScriptableHost:
      return extension->permissions_data()->GetContentScriptAccess(
          url, tab_id, /*error=*/nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ExtensionActionViewControllerGrayscaleTest,
    ::testing::Values(PermissionType::kScriptableHost,
                      PermissionType::kExplicitHost),
    [](const testing::TestParamInfo<PermissionType>& info) {
      switch (info.param) {
        case PermissionType::kScriptableHost:
          return "ScriptableHost";
        case PermissionType::kExplicitHost:
          return "ExplicitHost";
      }
    });

// Tests the behavior for icon grayscaling.
IN_PROC_BROWSER_TEST_P(ExtensionActionViewControllerGrayscaleTest,
                       GrayscaleIcon_ExplicitHosts) {
  Init();

  // Create an extension with google.com as either an explicit or scriptable
  // host permission.
  PermissionType permission_type = GetParam();
  std::string host_permission = "https://www.google.com/*";
  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(permission_type, host_permission);
  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());

  extensions::ScriptingPermissionsModifier permissions_modifier(
      browser()->profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  const GURL kHasPermissionUrl("https://www.google.com/");
  const GURL kNoPermissionsUrl("https://www.chromium.org/");

  // Make sure UserScriptListener doesn't hold up the navigation.
  extensions::ExtensionsBrowserClient::Get()
      ->GetUserScriptListener()
      ->TriggerUserScriptsReadyForTesting(browser()->profile());

  // Load up a page that we will navigate for the different test cases.
  AddTab(browser(), GURL("about:blank"));

  static constexpr struct {
    ActionState action_state;
    PageAccessStatus page_access;
    Coloring expected_coloring;
    BlockedDecoration expected_blocked_decoration;
  } test_cases[] = {
      {ActionState::kEnabled, PageAccessStatus::kNone, Coloring::kFull,
       BlockedDecoration::kNotPainted},
      {ActionState::kEnabled, PageAccessStatus::kWithheld, Coloring::kFull,
       BlockedDecoration::kNotPainted},
      {ActionState::kEnabled, PageAccessStatus::kBlocked, Coloring::kFull,
       BlockedDecoration::kPainted},
      {ActionState::kEnabled, PageAccessStatus::kGranted, Coloring::kFull,
       BlockedDecoration::kNotPainted},

      {ActionState::kDisabled, PageAccessStatus::kNone, Coloring::kGrayscale,
       BlockedDecoration::kNotPainted},
      {ActionState::kDisabled, PageAccessStatus::kWithheld, Coloring::kFull,
       BlockedDecoration::kNotPainted},
      {ActionState::kDisabled, PageAccessStatus::kBlocked, Coloring::kFull,
       BlockedDecoration::kPainted},
      {ActionState::kDisabled, PageAccessStatus::kGranted, Coloring::kFull,
       BlockedDecoration::kNotPainted},
  };

  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(controller);
  content::WebContents* web_contents = GetActiveWebContents();
  extensions::ExtensionAction* extension_action =
      extensions::ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("Running test case %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];

    // Set up the proper state for the test case.
    switch (test_case.page_access) {
      case PageAccessStatus::kNone: {
        NavigateAndCommitActiveTab(kNoPermissionsUrl);
        // Page access should be denied; verify.
        extensions::PermissionsData::PageAccess page_access =
            GetPageAccess(web_contents, extension, permission_type);
        EXPECT_EQ(extensions::PermissionsData::PageAccess::kDenied,
                  page_access);
        break;
      }
      case PageAccessStatus::kWithheld: {
        NavigateAndCommitActiveTab(kHasPermissionUrl);
        // Page access should already be withheld; verify.
        extensions::PermissionsData::PageAccess page_access =
            GetPageAccess(web_contents, extension, permission_type);
        EXPECT_EQ(extensions::PermissionsData::PageAccess::kWithheld,
                  page_access);
        break;
      }
      case PageAccessStatus::kBlocked:
        // Navigate to a page where the permission is currently withheld and
        // try to inject a script.
        NavigateAndCommitActiveTab(kHasPermissionUrl);
        action_runner->RequestScriptInjectionForTesting(
            extension.get(), extensions::mojom::RunLocation::kDocumentIdle,
            base::DoNothing());
        break;
      case PageAccessStatus::kGranted:
        // Grant the withheld requested permission and navigate there.
        permissions_modifier.GrantHostPermission(kHasPermissionUrl);
        NavigateAndCommitActiveTab(kHasPermissionUrl);
        break;
    }

    // Enable or disable the action based on the test case.
    extension_action->SetIsVisible(
        tab_id, test_case.action_state == ActionState::kEnabled);

    std::unique_ptr<IconWithBadgeImageSource> image_source =
        controller->GetIconImageSourceForTesting(web_contents, view_size());
    EXPECT_EQ(test_case.expected_coloring == Coloring::kGrayscale,
              image_source->grayscale());
    EXPECT_EQ(
        test_case.expected_blocked_decoration == BlockedDecoration::kPainted,
        image_source->paint_blocked_actions_decoration());

    // Clean up permissions state.
    if (test_case.page_access == PageAccessStatus::kGranted) {
      permissions_modifier.RemoveGrantedHostPermission(kHasPermissionUrl);
    }
    action_runner->ClearInjectionsForTesting(*extension);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       RuntimeHostsTooltip) {
  Init();
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "extension name", extensions::ActionInfo::Type::kBrowser,
      {"https://www.google.com/*"});

  extensions::ScriptingPermissionsModifier permissions_modifier(
      browser()->profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  const GURL kUrl("https://www.google.com/");
  AddTab(browser(), kUrl);

  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(controller);
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // Page access should already be withheld.
  EXPECT_EQ(extensions::PermissionsData::PageAccess::kWithheld,
            extension->permissions_data()->GetPageAccess(kUrl, tab_id,
                                                         /*error=*/nullptr));
  EXPECT_EQ(u"extension name\nWants access to this site",
            controller->GetTooltip(web_contents));

  // Request access.
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  action_runner->RequestScriptInjectionForTesting(
      extension.get(), extensions::mojom::RunLocation::kDocumentIdle,
      base::DoNothing());
  EXPECT_EQ(u"extension name\nWants access to this site",
            controller->GetTooltip(web_contents));

  // Grant access.
  action_runner->ClearInjectionsForTesting(*extension);
  permissions_modifier.GrantHostPermission(kUrl);
  EXPECT_EQ(u"extension name\nHas access to this site",
            controller->GetTooltip(web_contents));
}

// Tests the appearance of extension actions for an extension with the activeTab
// permission and no browser or page action defined in their manifest.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       ActiveTabIconAppearance) {
  Init();
  const GURL kUnlistedHost("https://www.example.com");
  const GURL kGrantedHost("https://www.google.com");
  const GURL kRestrictedHost("chrome://extensions");
  static constexpr char16_t kWantsAccessTooltip[] =
      u"active tab\nWants access to this site";
  static constexpr char16_t kHasAccessTooltip[] =
      u"active tab\nHas access to this site";
  static constexpr char16_t kNoAccessTooltip[] = u"active tab";
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("active tab")
          .AddAPIPermission("activeTab")
          .AddHostPermission(kGrantedHost.spec())
          .Build();
  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());

  // Navigate the browser to a site the extension doesn't have explicit access
  // to and verify the expected appearance.
  AddTab(browser(), kUnlistedHost);
  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(controller);
  content::WebContents* web_contents = GetActiveWebContents();

  {
    EXPECT_EQ(SiteInteraction::kActiveTab,
              controller->GetSiteInteraction(web_contents));
    EXPECT_TRUE(controller->IsEnabled(web_contents));
    std::unique_ptr<IconWithBadgeImageSource> image_source =
        controller->GetIconImageSourceForTesting(web_contents, view_size());
    EXPECT_FALSE(image_source->grayscale());
    EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
    EXPECT_EQ(kWantsAccessTooltip, controller->GetTooltip(web_contents));
  }

  // Navigate to a site which the extension does have explicit host access to
  // and verify the expected appearance.
  NavigateAndCommitActiveTab(kGrantedHost);
  {
    EXPECT_EQ(SiteInteraction::kGranted,
              controller->GetSiteInteraction(web_contents));
    // This is a little unintuitive, but if an extension is using a page action
    // and has not specified any declarative rules or manually changed it's
    // enabled state, it can have access to a page but be in the disabled state.
    // The icon will still be colored however.
    EXPECT_FALSE(controller->IsEnabled(web_contents));
    std::unique_ptr<IconWithBadgeImageSource> image_source =
        controller->GetIconImageSourceForTesting(web_contents, view_size());
    EXPECT_FALSE(image_source->grayscale());
    EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
    EXPECT_EQ(kHasAccessTooltip, controller->GetTooltip(web_contents));
  }

  // Navigate to a restricted URL and verify the expected appearance.
  NavigateAndCommitActiveTab(kRestrictedHost);
  {
    EXPECT_EQ(SiteInteraction::kNone,
              controller->GetSiteInteraction(web_contents));
    EXPECT_FALSE(controller->IsEnabled(web_contents));
    std::unique_ptr<IconWithBadgeImageSource> image_source =
        controller->GetIconImageSourceForTesting(web_contents, view_size());
    EXPECT_TRUE(image_source->grayscale());
    EXPECT_FALSE(image_source->paint_blocked_actions_decoration());
    EXPECT_EQ(kNoAccessTooltip, controller->GetTooltip(web_contents));
  }
}

// Tests that an extension with the activeTab permission has active tab site
// interaction except for restricted URLs.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       GetSiteInteractionWithActiveTab) {
  Init();
  // Note: Not using `CreateAndAddExtensionWithGrantedHostPermissions` because
  // this adds an API permission (activeTab).
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("active tab")
          .SetAction(extensions::ActionInfo::Type::kAction)
          .SetLocation(ManifestLocation::kInternal)
          .AddAPIPermission("activeTab")
          .Build();
  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());

  // Navigate the browser to google.com. Since clicking the extension would
  // grant access to the page, the page interaction status should show as
  // "pending".
  AddTab(browser(), GURL("https://www.google.com/"));
  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(controller);
  content::WebContents* web_contents = GetActiveWebContents();

  EXPECT_EQ(SiteInteraction::kActiveTab,
            controller->GetSiteInteraction(web_contents));

  // Click on the action, which grants activeTab and allows the extension to
  // access the page. This changes the page interaction status to "granted".
  controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kToolbarButton);
  EXPECT_EQ(SiteInteraction::kGranted,
            controller->GetSiteInteraction(web_contents));

  // Now navigate to a restricted URL. Clicking the extension won't give access
  // here, so the page interaction status should be "none".
  NavigateAndCommitActiveTab(GURL("chrome://extensions"));
  EXPECT_EQ(SiteInteraction::kNone,
            controller->GetSiteInteraction(web_contents));
  controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kToolbarButton);
  EXPECT_EQ(SiteInteraction::kNone,
            controller->GetSiteInteraction(web_contents));
}

// Tests that file URLs only have active tab site interaction if the extension
// has active tab permission and file URL access.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       GetSiteInteractionActiveTabWithFileURL) {
  Init();
  // We need to use a TestExtensionDir here to allow for the reload when giving
  // an extension file URL access.
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
    {
      "name": "Active Tab Page Interaction with File URLs",
      "description": "Testing SiteInteraction and ActiveTab on file URLs",
      "version": "0.1",
      "manifest_version": 2,
      "browser_action": {},
      "permissions": ["activeTab"]
    })");
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  loader.set_allow_file_access(false);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());

  // Navigate to a file URL. The page interaction status should be "none", as
  // the extension doesn't have file URL access granted. Clicking it should
  // result in no change.
  AddTab(browser(), GURL("file://foo"));
  ExtensionActionViewController* controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(controller);
  content::WebContents* web_contents = GetActiveWebContents();

  EXPECT_EQ(SiteInteraction::kNone,
            controller->GetSiteInteraction(web_contents));
  controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kToolbarButton);
  EXPECT_EQ(SiteInteraction::kNone,
            controller->GetSiteInteraction(web_contents));

  // After being granted access to file URLs the page interaction status should
  // show as "pending". A click will grant activeTab, giving access to the page
  // and will change the page interaction status to "active".
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()),
      extension->id());
  extensions::util::SetAllowFileAccess(extension->id(), browser()->profile(),
                                       true /*allow*/);
  extension = observer.WaitForExtensionLoaded();
  ASSERT_TRUE(extension);
  // Refresh the controller as the extension has been reloaded.
  controller = GetViewControllerForId(extension->id());
  EXPECT_EQ(SiteInteraction::kActiveTab,
            controller->GetSiteInteraction(web_contents));
  controller->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kToolbarButton);
  EXPECT_EQ(SiteInteraction::kGranted,
            controller->GetSiteInteraction(web_contents));
}

// ExtensionActionViewController::GetIcon() can potentially be called with a
// null web contents if the tab strip model doesn't know of an active tab
// (though it's a bit unclear when this is the case).
// See https://crbug.com/888121
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerBrowserTest,
                       TestGetIconWithNullWebContents) {
  Init();
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "extension name", extensions::ActionInfo::Type::kAction,
      {"https://example.com/"});

  extensions::ScriptingPermissionsModifier permissions_modifier(
      browser()->profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);

  // Try getting an icon with no active web contents. Nothing should crash, and
  // a non-empty icon should be returned.
  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  ui::ImageModel icon = controller->GetIcon(nullptr, view_size());
  EXPECT_FALSE(icon.IsEmpty());
}

class ExtensionActionViewControllerFeatureBrowserTest
    : public ExtensionActionViewControllerBrowserTest {
 public:
  ExtensionActionViewControllerFeatureBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ~ExtensionActionViewControllerFeatureBrowserTest() override = default;
  ExtensionActionViewControllerFeatureBrowserTest(
      const ExtensionActionViewControllerFeatureBrowserTest&) = delete;
  ExtensionActionViewControllerFeatureBrowserTest& operator=(
      const ExtensionActionViewControllerFeatureBrowserTest&) = delete;

  HoverCardState::SiteAccess GetHoverCardSiteAccessState(
      ExtensionActionViewController* controller,
      content::WebContents* web_contents) {
    return controller->GetHoverCardState(web_contents).site_access;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests hover card status after changing user site settings and site access.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerFeatureBrowserTest,
                       GetHoverCardStatus) {
  Init();
  std::string url_string = "https://example.com/";

  const GURL gurl =
      embedded_https_test_server().GetURL("example.com", "/simple.html");

  auto extensionA = CreateAndAddExtension(
      "Extension A", extensions::ActionInfo::Type::kAction);
  auto extensionB = CreateAndAddExtensionWithGrantedHostPermissions(
      "Extension B", extensions::ActionInfo::Type::kAction, {url_string});
  auto extensionC = CreateAndAddExtensionWithGrantedHostPermissions(
      "Extension c", extensions::ActionInfo::Type::kAction, {url_string});

  AddTab(browser(), gurl);

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto url = url::Origin::Create(web_contents->GetLastCommittedURL());

  ExtensionActionViewController* const controllerA =
      GetViewControllerForId(extensionA->id());
  ASSERT_TRUE(controllerA);
  ExtensionActionViewController* const controllerB =
      GetViewControllerForId(extensionB->id());
  ASSERT_TRUE(controllerB);
  ExtensionActionViewController* const controllerC =
      GetViewControllerForId(extensionC->id());
  ASSERT_TRUE(controllerC);

  // By default, user site setting is "customize by extension" and site access
  // is granted to every extension that requests them. Thus, verify extension A
  // hover card state is "does not want access" and the rest is "have access".
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser()->profile());
  ASSERT_EQ(permissions_manager->GetUserSiteSetting(url),
            UserSiteSetting::kCustomizeByExtension);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kExtensionDoesNotWantAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);

  // Withhold extension C host permissions. Verify only extension C changed
  // hover card state to "requests access".
  extensions::ScriptingPermissionsModifier(browser()->profile(), extensionC)
      .SetWithholdHostPermissions(true);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kExtensionDoesNotWantAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kExtensionRequestsAccess);

  // Block all extensions site access. Verify all extensions appear as "all
  // extensions blocked" (even though extension A never requested access).
  permissions_manager->UpdateUserSiteSetting(
      url, UserSiteSetting::kBlockAllExtensions);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kAllExtensionsBlocked);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kAllExtensionsBlocked);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kAllExtensionsBlocked);

  // Change back to customize site access by extension. Verify extension A
  // hover card state is "does not want access", extension B is "has access" and
  // extension C is "requests access".
  permissions_manager->UpdateUserSiteSetting(
      url, UserSiteSetting::kCustomizeByExtension);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kExtensionDoesNotWantAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kExtensionRequestsAccess);
}

// Tests correct tooltip text after changing user site settings and site access.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerFeatureBrowserTest,
                       GetTooltip) {
  Init();
  std::u16string extension_name = u"Extension";
  std::string requested_url_string = "https://a.com/";
  const GURL requested_gurl =
      embedded_https_test_server().GetURL("a.com", "/simple.html");
  const GURL not_requested_gurl =
      embedded_https_test_server().GetURL("b.com", "/simple.html");
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      base::UTF16ToUTF8(extension_name), extensions::ActionInfo::Type::kAction,
      {requested_url_string});

  // Navigate to a site the extension requests access to.
  AddTab(browser(), requested_gurl);
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto requested_url = url::Origin::Create(web_contents->GetLastCommittedURL());

  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(controller);

  // By default, user site setting is "customize by extension" and site access
  // is granted to every extension that requests them. Verify extension tooltip
  // is "has access".
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser()->profile());
  ASSERT_EQ(permissions_manager->GetUserSiteSetting(requested_url),
            UserSiteSetting::kCustomizeByExtension);
  EXPECT_EQ(
      controller->GetTooltip(web_contents),
      base::JoinString(
          {extension_name,
           l10n_util::GetStringUTF16(
               IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_HAS_ACCESS_TOOLTIP)},
          u"\n"));

  // Withhold extension host permissions. Verify extension tooltip is "requests
  // access".
  extensions::ScriptingPermissionsModifier(browser()->profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_EQ(
      controller->GetTooltip(web_contents),
      base::JoinString(
          {extension_name,
           l10n_util::GetStringUTF16(
               IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_REQUESTS_TOOLTIP)},
          u"\n"));

  // Block all extensions access to requested.com. Verify extension tooltip is
  // "blocked access".
  permissions_manager->UpdateUserSiteSetting(
      requested_url, UserSiteSetting::kBlockAllExtensions);
  EXPECT_EQ(
      controller->GetTooltip(web_contents),
      base::JoinString(
          {extension_name,
           l10n_util::GetStringUTF16(
               IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_BLOCKED_ACCESS_TOOLTIP)},
          u"\n"));

  // Navigate to a site that the extension didn't request access to.
  AddTab(browser(), not_requested_gurl);
  web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto non_requested_url =
      url::Origin::Create(web_contents->GetLastCommittedURL());

  // By default, user site setting is "customize by extension". Verify extension
  // tooltip is just the extension name since extension didn't request access
  // to this site.
  ASSERT_EQ(permissions_manager->GetUserSiteSetting(non_requested_url),
            UserSiteSetting::kCustomizeByExtension);
  EXPECT_EQ(controller->GetTooltip(web_contents), extension_name);

  // Block all extensions access to non-requested.com. Verify extension tooltip
  // is "blocked access" regardless of extension not requesting access to this
  // site.
  permissions_manager->UpdateUserSiteSetting(
      non_requested_url, UserSiteSetting::kBlockAllExtensions);
  EXPECT_EQ(
      controller->GetTooltip(web_contents),
      base::JoinString(
          {extension_name,
           l10n_util::GetStringUTF16(
               IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_BLOCKED_ACCESS_TOOLTIP)},
          u"\n"));
}

class ExtensionActionViewControllerFeatureWithPermittedSitesBrowserTest
    : public ExtensionActionViewControllerFeatureBrowserTest {
 public:
  ExtensionActionViewControllerFeatureWithPermittedSitesBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites);
  }
  ~ExtensionActionViewControllerFeatureWithPermittedSitesBrowserTest()
      override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests hover card status after changing user site settings and site access.
IN_PROC_BROWSER_TEST_F(
    ExtensionActionViewControllerFeatureWithPermittedSitesBrowserTest,
    GetHoverCardStatus) {
  Init();
  std::string url_string = "https://example.com/";
  const GURL gurl =
      embedded_https_test_server().GetURL("example.com", "/simple.html");
  auto extensionA = CreateAndAddExtension(
      "Extension A", extensions::ActionInfo::Type::kAction);
  auto extensionB = CreateAndAddExtensionWithGrantedHostPermissions(
      "Extension B", extensions::ActionInfo::Type::kAction, {url_string});
  auto extensionC = CreateAndAddExtensionWithGrantedHostPermissions(
      "Extension c", extensions::ActionInfo::Type::kAction, {url_string});

  AddTab(browser(), gurl);
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto url = url::Origin::Create(web_contents->GetLastCommittedURL());

  ExtensionActionViewController* const controllerA =
      GetViewControllerForId(extensionA->id());
  ASSERT_TRUE(controllerA);
  ExtensionActionViewController* const controllerB =
      GetViewControllerForId(extensionB->id());
  ASSERT_TRUE(controllerB);
  ExtensionActionViewController* const controllerC =
      GetViewControllerForId(extensionC->id());
  ASSERT_TRUE(controllerC);

  // By default, user site setting is "customize by extension" and site access
  // is granted to every extension that requests them. Thus, verify extension A
  // hover card state is "does not want access" and the rest is "have access".
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser()->profile());
  ASSERT_EQ(permissions_manager->GetUserSiteSetting(url),
            UserSiteSetting::kCustomizeByExtension);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kExtensionDoesNotWantAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);

  // Withhold extension C host permissions. Verify only extension C changed
  // hover card state to "requests access".
  extensions::ScriptingPermissionsModifier(browser()->profile(), extensionC)
      .SetWithholdHostPermissions(true);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kExtensionDoesNotWantAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kExtensionHasAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kExtensionRequestsAccess);

  // Grant all extensions site access. Verify extension A hover card state is
  // "does not want access" and extensions B and C is "all extensions allowed".
  permissions_manager->UpdateUserSiteSetting(
      url, UserSiteSetting::kGrantAllExtensions);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerA, web_contents),
            HoverCardState::SiteAccess::kExtensionDoesNotWantAccess);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerB, web_contents),
            HoverCardState::SiteAccess::kAllExtensionsAllowed);
  EXPECT_EQ(GetHoverCardSiteAccessState(controllerC, web_contents),
            HoverCardState::SiteAccess::kAllExtensionsAllowed);
}

class ExtensionActionViewControllerWithSidePanelBrowserTest
    : public ExtensionActionViewControllerBrowserTest {
 protected:
  extensions::SidePanelService* side_panel_service() {
    return extensions::SidePanelService::Get(browser()->profile());
  }
};

// Test that the extension action is enabled if opening the side panel on icon
// click is enabled and the extension has a side panel for the current tab.
IN_PROC_BROWSER_TEST_F(ExtensionActionViewControllerWithSidePanelBrowserTest,
                       ActionEnabledIfSidePanelPresent) {
  Init();
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("just side panel")
          .SetLocation(ManifestLocation::kInternal)
          .SetManifestVersion(3)
          .AddAPIPermission("sidePanel")
          .Build();

  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());
  side_panel_service()->SetOpenSidePanelOnIconClick(extension->id(), true);

  ExtensionActionViewController* const action_controller =
      GetViewControllerForId(extension->id());
  ASSERT_TRUE(action_controller);
  EXPECT_EQ(extension.get(), action_controller->extension());

  AddTab(browser(), GURL("https://www.chromium.org/"));
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // If the preference is true but there is no side panel for the current tab,
  // the action should be disabled.
  std::unique_ptr<IconWithBadgeImageSource> image_source =
      action_controller->GetIconImageSourceForTesting(web_contents,
                                                      view_size());
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(action_controller->IsEnabled(web_contents));

  // Set a side panel for the current tab. This should enable the extension's
  // action.
  extensions::api::side_panel::PanelOptions options;
  options.enabled = true;
  options.path = "panel.html";
  options.tab_id = tab_id;
  side_panel_service()->SetOptions(*extension, std::move(options));

  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_TRUE(action_controller->IsEnabled(web_contents));

  // Setting the preference to false should disable the extension's action.
  side_panel_service()->SetOpenSidePanelOnIconClick(extension->id(), false);
  image_source = action_controller->GetIconImageSourceForTesting(web_contents,
                                                                 view_size());
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(action_controller->IsEnabled(web_contents));
}
