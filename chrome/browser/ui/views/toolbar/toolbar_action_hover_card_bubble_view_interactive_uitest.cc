// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/widget_test.h"

namespace {

using SiteInteraction = extensions::SitePermissionsHelper::SiteInteraction;

// Similar to views::test::WidgetDestroyedWaiter but waiting after the widget
// has been closed is a no-op rather than an error.
class SafeWidgetDestroyedWaiter : public views::WidgetObserver {
 public:
  explicit SafeWidgetDestroyedWaiter(views::Widget* widget) {
    observation_.Observe(widget);
  }

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    observation_.Reset();
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  void Wait() {
    if (!observation_.IsObserving())
      return;
    DCHECK(quit_closure_.is_null());
    quit_closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  base::OnceClosure quit_closure_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

}  // namespace

class ToolbarActionHoverCardBubbleViewUITest : public ExtensionsToolbarUITest {
 public:
  ToolbarActionHoverCardBubbleViewUITest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    ToolbarActionHoverCardController::disable_animations_for_testing_ = true;
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ToolbarActionHoverCardBubbleViewUITest(
      const ToolbarActionHoverCardBubbleViewUITest&) = delete;
  ToolbarActionHoverCardBubbleViewUITest& operator=(
      const ToolbarActionHoverCardBubbleViewUITest&) = delete;
  ~ToolbarActionHoverCardBubbleViewUITest() override = default;

  ToolbarActionHoverCardBubbleView* hover_card() {
    return GetExtensionsToolbarContainer()
        ->action_hover_card_controller_->hover_card_;
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionsToolbarUITest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void HoverMouseOverActionView(ToolbarActionView* action_view) {
    // We don't use ToolbarActionView::OnMouseEntered here to invoke the hover
    // card because that path is disabled in browser tests. If we enabled it,
    // the real mouse might interfere with the test.
    GetExtensionsToolbarContainer()->UpdateToolbarActionHoverCard(
        action_view, ToolbarActionHoverCardUpdateType::kHover);
  }

  void ClickMouseOnActionView(ToolbarActionView* action_view) {
    ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), base::TimeTicks(), ui::EF_NONE, 0);
    action_view->OnMousePressed(mouse_event);
  }

  void MouseExitsFromExtensionsContainer() {
    ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(),
                               gfx::Point(), base::TimeTicks(), ui::EF_NONE, 0);
    GetExtensionsToolbarContainer()->OnMouseExited(mouse_event);
  }

  void MouseMovesInExtensionsContainer() {
    ui::MouseEvent mouse_event(ui::EventType::kMouseMoved, gfx::Point(),
                               gfx::Point(), base::TimeTicks(), ui::EF_NONE, 0);
    GetExtensionsToolbarContainer()->OnMouseMoved(mouse_event);
  }

  scoped_refptr<const extensions::Extension> LoadExtensionAndPinIt(
      const std::string& path) {
    scoped_refptr<const extensions::Extension> extension =
        LoadTestExtension(path);
    PinExtension(extension->id());
    return extension;
  }

  void PinExtension(const extensions::ExtensionId& extension_id) {
    ToolbarActionsModel* const toolbar_model =
        ToolbarActionsModel::Get(browser()->profile());
    toolbar_model->SetActionVisibility(extension_id, true);
    GetExtensionsToolbarContainer()->GetWidget()->LayoutRootViewIfNecessary();
  }

  // Make `extension_id` force-pinned, as if it was controlled by the
  // ExtensionSettings policy.
  void ForcePinExtension(const extensions::ExtensionId& extension_id) {
    std::string policy_item_key =
        base::StringPrintf("%s", extension_id.c_str());
    base::Value::Dict policy_item_value;
    policy_item_value.Set("toolbar_pin", "force_pinned");

    policy::PolicyMap policy_map =
        policy_provider_.policies()
            .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                         /*component_id=*/std::string()))
            .Clone();
    auto* existing_entry = policy_map.GetMutableValue(
        policy::key::kExtensionSettings, base::Value::Type::DICT);

    if (existing_entry) {
      // Append to the existing policy.
      existing_entry->GetDict().Set(policy_item_key,
                                    std::move(policy_item_value));
    } else {
      // Set the new policy value.
      base::Value::Dict policy_value;
      policy_value.Set(policy_item_key, std::move(policy_item_value));
      policy_map.Set(policy::key::kExtensionSettings,
                     policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                     policy::POLICY_SOURCE_CLOUD,
                     base::Value(std::move(policy_value)),
                     /*external_data_fetcher=*/nullptr);
    }

    policy_provider_.UpdateChromePolicy(policy_map);

    GetExtensionsToolbarContainer()->GetWidget()->LayoutRootViewIfNecessary();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    LoadExtensionAndPinIt("extensions/simple_with_popup");
    auto action_views = GetVisibleToolbarActionViews();
    ASSERT_EQ(action_views.size(), 1u);

    HoverMouseOverActionView(action_views[0]);
    views::test::WidgetVisibleWaiter(hover_card()->GetWidget()).Wait();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest, InvokeUi) {
  ShowAndVerifyUi();
}

// Verify hover card is visible while hovering and not visible outside of the
// extensions container.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetVisibleOnHover) {
  ShowUi("");
  views::Widget* const widget = hover_card()->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  MouseExitsFromExtensionsContainer();
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card content and anchor is correctly updated when moving hover
// from one action view to another. Note that hover card content based on site
// access is tested more in depth in ExtensionActionViewController unittest,
// since such class computes the hover card state.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetUpdatedWhenHoveringBetweenActionViews) {
  // Bypass install verification to allow testing the behavior of
  // force-installed extensions.
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install four extensions with different policy and site access permissions
  // to test all the possible footnote combinations.
  auto simple_extension = InstallExtension("Simple extension");
  auto force_installed_extension =
      ForceInstallExtension("Force installed extension");
  auto extension_with_host_permissions = InstallExtensionWithHostPermissions(
      "Extension with host permissions", "<all_urls>");
  auto force_pinned_extension_with_host_permissions =
      InstallExtensionWithHostPermissions(
          "Force pinned extension with host permissions", "<all_urls>");

  PinExtension(simple_extension->id());
  PinExtension(force_installed_extension->id());
  PinExtension(extension_with_host_permissions->id());
  ForcePinExtension(force_pinned_extension_with_host_permissions->id());

  auto action_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(action_views.size(), 4u);

  // Navigate to a url that the extensions with host permissions request.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Hover over the simple extension pinned by the user.
  // Verify card anchors to its action, and it only contains the extension's
  // name.
  ToolbarActionView* simple_action =
      GetExtensionsToolbarContainer()->GetViewForId(simple_extension->id());
  HoverMouseOverActionView(simple_action);
  views::Widget* const widget = hover_card()->GetWidget();
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), simple_action);
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(), u"Simple extension");
  EXPECT_FALSE(hover_card()->IsSiteAccessSeparatorVisible());
  EXPECT_FALSE(hover_card()->IsSiteAccessTitleVisible());
  EXPECT_FALSE(hover_card()->IsSiteAccessDescriptionVisible());
  EXPECT_FALSE(hover_card()->IsPolicySeparatorVisible());
  EXPECT_FALSE(hover_card()->IsPolicyLabelVisible());

  // Hover over the extension installed by policy and pinned by the user.
  // Verify card anchors to its action using the same widget, because it
  // transitions from one action view to the other, and it contains the
  // extension's name and policy content.
  ToolbarActionView* force_installed_action =
      GetExtensionsToolbarContainer()->GetViewForId(
          force_installed_extension->id());
  HoverMouseOverActionView(force_installed_action);
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), force_installed_action);
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(),
            u"Force installed extension");
  EXPECT_FALSE(hover_card()->IsSiteAccessSeparatorVisible());
  EXPECT_FALSE(hover_card()->IsSiteAccessTitleVisible());
  EXPECT_FALSE(hover_card()->IsSiteAccessDescriptionVisible());
  EXPECT_TRUE(hover_card()->IsPolicySeparatorVisible());
  EXPECT_TRUE(hover_card()->IsPolicyLabelVisible());

  // Hover over the extension with host permissions pinned by the user.
  // Verify card anchors to its action using the same widget, and it contains
  // the extension's name and site access content.
  ToolbarActionView* action_with_host_permissions =
      GetExtensionsToolbarContainer()->GetViewForId(
          extension_with_host_permissions->id());
  HoverMouseOverActionView(action_with_host_permissions);
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), action_with_host_permissions);
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(),
            u"Extension with host permissions");
  EXPECT_TRUE(hover_card()->IsSiteAccessSeparatorVisible());
  EXPECT_TRUE(hover_card()->IsSiteAccessTitleVisible());
  EXPECT_TRUE(hover_card()->IsSiteAccessDescriptionVisible());
  EXPECT_FALSE(hover_card()->IsPolicySeparatorVisible());
  EXPECT_FALSE(hover_card()->IsPolicyLabelVisible());

  // Hover over the extension with host permission installed and pinned by
  // policy. Verify card anchors to its action using the same widget, and it
  // contains the extension's name, site access and policy content.
  ToolbarActionView* force_pinned_action_with_host_permissions =
      GetExtensionsToolbarContainer()->GetViewForId(
          force_pinned_extension_with_host_permissions->id());
  HoverMouseOverActionView(force_pinned_action_with_host_permissions);
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(),
            force_pinned_action_with_host_permissions);
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(),
            u"Force pinned extension with host permissions");
  EXPECT_TRUE(hover_card()->IsSiteAccessSeparatorVisible());
  EXPECT_TRUE(hover_card()->IsSiteAccessTitleVisible());
  EXPECT_TRUE(hover_card()->IsSiteAccessDescriptionVisible());
  EXPECT_TRUE(hover_card()->IsPolicySeparatorVisible());
  EXPECT_TRUE(hover_card()->IsPolicyLabelVisible());
}

// Verify hover card content is dynamically updated when toolbar action title is
// updated.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetContentDynamicallyUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto extension = InstallExtension("Extension name");
  PinExtension(extension->id());

  // Verify extension is pinned
  ToolbarActionView* action_view =
      GetExtensionsToolbarContainer()->GetViewForId(extension->id());
  ASSERT_TRUE(action_view);

  // Hover over the extension and verify card anchors to its action.
  HoverMouseOverActionView(action_view);
  views::Widget* const widget = hover_card()->GetWidget();
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), action_view);

  // Verify card title contains the extension name and action title is not
  // visible.
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(), u"Extension name");
  EXPECT_FALSE(hover_card()->IsActionTitleVisible());

  // Update the extension action's title for the current tab.
  extensions::ExtensionAction* action =
      extensions::ExtensionActionManager::Get(profile())->GetExtensionAction(
          *extension);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(action);
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  action->SetTitle(tab_id, "Action title");
  extensions::ExtensionActionAPI::Get(profile())->NotifyChange(
      action, web_contents, profile());

  // Verify hover card is still visible.
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), action_view);

  // Verify card contains the extension name and action title.
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(), u"Extension name");
  EXPECT_TRUE(hover_card()->IsActionTitleVisible());
  EXPECT_EQ(hover_card()->GetActionTitleTextForTesting(), u"Action title");
}

// Verify site access content in hover card is dynamically updated when the
// extension site access is updated.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetContentDynamicallyUpdated_SiteAccessUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install an extension and withhold its host permissions.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", "*://example.com/*");
  auto permissions_modifier =
      extensions::ScriptingPermissionsModifier(profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);

  PinExtension(extension->id());
  ToolbarActionView* action_view =
      GetExtensionsToolbarContainer()->GetViewForId(extension->id());
  ASSERT_TRUE(action_view);

  // Navigate to a example.com
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Hover over the extension and verify card anchors to its action.
  HoverMouseOverActionView(action_view);
  views::Widget* const widget = hover_card()->GetWidget();
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), action_view);

  // Verify site access title has "requests access" text.
  EXPECT_EQ(
      hover_card()->GetSiteAccessTitleTextForTesting(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_TITLE_REQUESTS_ACCESS));

  // Grant host permissions to example.com.
  extensions::PermissionsManagerWaiter waiter(
      extensions::PermissionsManager::Get(profile()));
  permissions_modifier.GrantHostPermission(url);
  waiter.WaitForExtensionPermissionsUpdate();

  // Verify site access title has "has access" text.
  EXPECT_EQ(hover_card()->GetSiteAccessTitleTextForTesting(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_TITLE_HAS_ACCESS));
}

// Verify hover card is not visible when mouse moves inside the extensions
// container to a button that is not a toolbar icon view (which has its own
// 'on mouse moved' event listener).
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetNotVisibleOnExtensionsControl) {
  ShowUi("");
  views::Widget* const widget = hover_card()->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  MouseMovesInExtensionsContainer();
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is not visible after clicking on a toolbar action view.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetNotVisibleOnToolbarActionViewClick) {
  ShowUi("");
  views::Widget* const widget = hover_card()->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  auto action_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(action_views.size(), 1u);

  ClickMouseOnActionView(action_views[0]);
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is not visible on focus, similar to tooltip behavior.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetNotVisibleOnFocus) {
  LoadExtensionAndPinIt("extensions/simple_with_popup");
  auto action_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(action_views.size(), 1u);

  GetExtensionsToolbarContainer()->GetFocusManager()->SetFocusedView(
      action_views[0]);
  EXPECT_EQ(hover_card(), nullptr);
}

// Verify that the hover card is not visible when any key is pressed.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewUITest,
                       WidgetNotVisibleOnAnyKeyPressInSameWindow) {
  ShowUi("");
  views::Widget* const widget = hover_card()->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Verify that the hover card widget is destroyed sometime between now and
  // when we check afterwards. Depending on platform, the destruction could be
  // synchronous or asynchronous.
  SafeWidgetDestroyedWaiter widget_destroyed_waiter(widget);
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));

  // Note, fade in/out animations are disabled for testing so this should be
  // relatively quick.
  widget_destroyed_waiter.Wait();
  EXPECT_EQ(hover_card(), nullptr);
}

class ToolbarActionHoverCardBubbleViewDisabledFeatureUITest
    : public ToolbarActionHoverCardBubbleViewUITest {
 public:
  ToolbarActionHoverCardBubbleViewDisabledFeatureUITest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
};

// Verify hover card is not visible on toolbar action view hover when the
// feature is disabled.
IN_PROC_BROWSER_TEST_F(ToolbarActionHoverCardBubbleViewDisabledFeatureUITest,
                       WidgetNotVisibleWhenDisabledFeature) {
  LoadExtensionAndPinIt("extensions/simple_with_popup");
  auto action_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(action_views.size(), 1u);

  HoverMouseOverActionView(action_views[0]);
  EXPECT_EQ(hover_card(), nullptr);
}
