// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/native_theme/native_theme.h"

using extensions::mojom::ManifestLocation;
using SiteInteraction = extensions::SitePermissionsHelper::SiteInteraction;
using UserSiteSetting = extensions::PermissionsManager::UserSiteSetting;
using HoverCardState = ToolbarActionViewController::HoverCardState;

class ExtensionActionViewControllerUnitTest : public BrowserWithTestWindowTest {
 public:
  ExtensionActionViewControllerUnitTest() = default;
  ExtensionActionViewControllerUnitTest(
      const ExtensionActionViewControllerUnitTest& other) = delete;
  ExtensionActionViewControllerUnitTest& operator=(
      const ExtensionActionViewControllerUnitTest& other) = delete;

  ~ExtensionActionViewControllerUnitTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Initialize the various pieces of the extensions system.
    extensions::LoadErrorReporter::Init(false);
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    toolbar_model_ =
        extensions::extension_action_test_util::CreateToolbarModelForProfile(
            profile());
    extension_service_ =
        extensions::ExtensionSystem::Get(profile())->extension_service();

    test_util_ = ExtensionActionTestHelper::Create(browser(), false);

    view_size_ = test_util_->GetToolbarActionSize();
  }

  void TearDown() override {
    test_util_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  // Sets whether the given |action| wants to run on the |web_contents|.
  void SetActionWantsToRunOnTab(extensions::ExtensionAction* action,
                                content::WebContents* web_contents,
                                bool wants_to_run) {
    action->SetIsVisible(
        sessions::SessionTabHelper::IdForTab(web_contents).id(), wants_to_run);
    extensions::ExtensionActionAPI::Get(profile())->NotifyChange(
        action, web_contents, profile());
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
        test_util_->GetExtensionsContainer()->GetActionForId(action_id));
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
            .AddPermissions(permissions)
            .Build();

    if (!permissions.empty())
      extension_service()->GrantPermissions(extension.get());

    extension_service()->AddExtension(extension.get());
    return extension;
  }

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }
  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }
  ExtensionsContainer* container() {
    return test_util_->GetExtensionsContainer();
  }
  const gfx::Size& view_size() const { return view_size_; }

 private:
  // The ExtensionService associated with the primary profile.
  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;

  // ToolbarActionsModel associated with the main profile.
  raw_ptr<ToolbarActionsModel> toolbar_model_ = nullptr;

  std::unique_ptr<ExtensionActionTestHelper> test_util_;

  // The standard size associated with a toolbar action view.
  gfx::Size view_size_;
};

// Tests the icon appearance of extension actions in the toolbar.
// Extensions that don't want to run should have their icons grayscaled.
TEST_F(ExtensionActionViewControllerUnitTest,
       ExtensionActionWantsToRunAppearance) {
  const std::string id =
      CreateAndAddExtension("extension", extensions::ActionInfo::TYPE_PAGE)
          ->id();

  AddTab(browser(), GURL("chrome://newtab"));

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
TEST_F(ExtensionActionViewControllerUnitTest, BrowserActionBlockedActions) {
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "browser_action", extensions::ActionInfo::TYPE_BROWSER,
      {"https://www.google.com/*"});

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
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
TEST_F(ExtensionActionViewControllerUnitTest, PageActionBlockedActions) {
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "page_action", extensions::ActionInfo::TYPE_PAGE,
      {"https://www.google.com/*"});

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
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
TEST_F(ExtensionActionViewControllerUnitTest, OnlyHostPermissionsAppearance) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("just hosts")
          .SetLocation(ManifestLocation::kInternal)
          .AddPermission("https://www.google.com/*")
          .Build();

  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());
  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
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

TEST_F(ExtensionActionViewControllerUnitTest,
       ExtensionActionContextMenuVisibility) {
  std::string id =
      CreateAndAddExtension("extension", extensions::ActionInfo::TYPE_BROWSER)
          ->id();

  // Check that the context menu has the proper string for the action's pinned
  // state.
  auto check_visibility_string = [](ToolbarActionViewController* action,
                                    int expected_visibility_string) {
    ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
        action->GetContextMenu(extensions::ExtensionContextMenuModel::
                                   ContextMenuSource::kToolbarAction));
    absl::optional<size_t> visibility_index = context_menu->GetIndexOfCommandId(
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
  EXPECT_FALSE(container()->IsActionVisibleOnToolbar(action));
  base::RunLoop run_loop;
  container()->PopOutAction(action, run_loop.QuitClosure());
  EXPECT_TRUE(container()->IsActionVisibleOnToolbar(action));
  // The string should still just be "pin".
  check_visibility_string(action, IDS_EXTENSIONS_PIN_TO_TOOLBAR);
}

// TODO(devlin): Now that this is only parameterized in one way, it could be a
// TestWithParamInterface<PermissionType>.
class ExtensionActionViewControllerGrayscaleTest
    : public ExtensionActionViewControllerUnitTest {
 public:
  enum class PermissionType {
    kScriptableHost,
    kExplicitHost,
  };

  ExtensionActionViewControllerGrayscaleTest() {}

  ExtensionActionViewControllerGrayscaleTest(
      const ExtensionActionViewControllerGrayscaleTest&) = delete;
  ExtensionActionViewControllerGrayscaleTest& operator=(
      const ExtensionActionViewControllerGrayscaleTest&) = delete;

  ~ExtensionActionViewControllerGrayscaleTest() override = default;

  void RunGrayscaleTest(PermissionType permission_type);

 private:
  scoped_refptr<const extensions::Extension> CreateExtension(
      PermissionType permission_type);
  extensions::PermissionsData::PageAccess GetPageAccess(
      content::WebContents* web_contents,
      scoped_refptr<const extensions::Extension> extensions,
      PermissionType permission_type);
};

void ExtensionActionViewControllerGrayscaleTest::RunGrayscaleTest(
    PermissionType permission_type) {
  // Create an extension with google.com as either an explicit or scriptable
  // host permission.
  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(permission_type);
  extension_service()->GrantPermissions(extension.get());
  extension_service()->AddExtension(extension.get());

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  const GURL kHasPermissionUrl("https://www.google.com/");
  const GURL kNoPermissionsUrl("https://www.chromium.org/");

  // Make sure UserScriptListener doesn't hold up the navigation.
  extensions::ExtensionsBrowserClient::Get()
      ->GetUserScriptListener()
      ->TriggerUserScriptsReadyForTesting(browser()->profile());

  // Load up a page that we will navigate for the different test cases.
  AddTab(browser(), GURL("about:blank"));

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
    kGrayscale,
    kFull,
  };
  enum class BlockedDecoration {
    kPainted,
    kNotPainted,
  };

  struct {
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
      extensions::ExtensionActionManager::Get(profile())->GetExtensionAction(
          *extension);
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
        // Navigate to a page where the permission is currently withheld and try
        // to inject a script.
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
    if (test_case.page_access == PageAccessStatus::kGranted)
      permissions_modifier.RemoveGrantedHostPermission(kHasPermissionUrl);
    action_runner->ClearInjectionsForTesting(*extension);
  }
}

scoped_refptr<const extensions::Extension>
ExtensionActionViewControllerGrayscaleTest::CreateExtension(
    PermissionType permission_type) {
  extensions::ExtensionBuilder builder("extension");
  builder.SetAction(extensions::ActionInfo::TYPE_BROWSER)
      .SetLocation(ManifestLocation::kInternal);
  constexpr char kHostGoogle[] = "https://www.google.com/*";
  switch (permission_type) {
    case PermissionType::kScriptableHost: {
      builder.AddContentScript("script.js", {kHostGoogle});
      break;
    }
    case PermissionType::kExplicitHost:
      builder.AddPermission(kHostGoogle);
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

// Tests the behavior for icon grayscaling. Ideally, these would be a single
// parameterized test, but toolbar tests are already parameterized with the UI
// mode.
TEST_F(ExtensionActionViewControllerGrayscaleTest,
       GrayscaleIcon_ExplicitHosts) {
  RunGrayscaleTest(PermissionType::kExplicitHost);
}
TEST_F(ExtensionActionViewControllerGrayscaleTest,
       GrayscaleIcon_ScriptableHosts) {
  RunGrayscaleTest(PermissionType::kScriptableHost);
}

TEST_F(ExtensionActionViewControllerUnitTest, RuntimeHostsTooltip) {
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "extension name", extensions::ActionInfo::TYPE_BROWSER,
      {"https://www.google.com/*"});

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
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
TEST_F(ExtensionActionViewControllerUnitTest, ActiveTabIconAppearance) {
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
          .AddPermission("activeTab")
          .AddPermission(kGrantedHost.spec())
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
TEST_F(ExtensionActionViewControllerUnitTest, GetSiteInteractionWithActiveTab) {
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "active tab", extensions::ActionInfo::TYPE_BROWSER, {"activeTab"});

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
TEST_F(ExtensionActionViewControllerUnitTest,
       GetSiteInteractionActiveTabWithFileURL) {
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
  extensions::ChromeTestExtensionLoader loader(profile());
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
      extensions::ExtensionRegistry::Get(profile()), extension->id());
  extensions::util::SetAllowFileAccess(extension->id(), profile(),
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
TEST_F(ExtensionActionViewControllerUnitTest, TestGetIconWithNullWebContents) {
  auto extension = CreateAndAddExtensionWithGrantedHostPermissions(
      "extension name", extensions::ActionInfo::TYPE_BROWSER,
      {"https://example.com/"});

  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension);
  permissions_modifier.SetWithholdHostPermissions(true);

  // Try getting an icon with no active web contents. Nothing should crash, and
  // a non-empty icon should be returned.
  ExtensionActionViewController* const controller =
      GetViewControllerForId(extension->id());
  gfx::Image icon = controller->GetIcon(nullptr, view_size());
  EXPECT_FALSE(icon.IsEmpty());
}

class ExtensionActionViewControllerFeatureUnitTest
    : public ExtensionActionViewControllerUnitTest {
 public:
  ExtensionActionViewControllerFeatureUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ~ExtensionActionViewControllerFeatureUnitTest() override = default;
  ExtensionActionViewControllerFeatureUnitTest(
      const ExtensionActionViewControllerFeatureUnitTest&) = delete;
  ExtensionActionViewControllerFeatureUnitTest& operator=(
      const ExtensionActionViewControllerFeatureUnitTest&) = delete;

  HoverCardState::SiteAccess GetHoverCardSiteAccessState(
      ExtensionActionViewController* controller,
      content::WebContents* web_contents) {
    return controller->GetHoverCardState(web_contents).site_access;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests hover card status after changing user site settings and site access.
TEST_F(ExtensionActionViewControllerFeatureUnitTest, GetHoverCardStatus) {
  std::string url_string = "https://example.com/";
  auto extensionA =
      CreateAndAddExtension("Extension A", extensions::ActionInfo::TYPE_ACTION);
  auto extensionB = CreateAndAddExtensionWithGrantedHostPermissions(
      "Extension B", extensions::ActionInfo::TYPE_ACTION, {url_string});
  auto extensionC = CreateAndAddExtensionWithGrantedHostPermissions(
      "Extension c", extensions::ActionInfo::TYPE_ACTION, {url_string});

  AddTab(browser(), GURL(url_string));
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
  extensions::ScriptingPermissionsModifier(profile(), extensionC)
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
