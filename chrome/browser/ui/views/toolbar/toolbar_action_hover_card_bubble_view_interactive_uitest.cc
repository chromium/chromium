// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_bubble_view.h"

#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/widget_test.h"

namespace {

using SiteInteraction = extensions::SitePermissionsHelper::SiteInteraction;

// Similar to views::test::WidgetDestroyedWaiter but waiting after the widget
// has been closed is a no-op rather than an error.
// TODO(crbug.com/1354661): Move SafeWidgetDestroyedWaiter to a shared file
// since it's used by multiple tests.
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

  void HoverMouseOverActionView(ToolbarActionView* action_view) {
    // We don't use ToolbarActionView::OnMouseEntered here to invoke the hover
    // card because that path is disabled in browser tests. If we enabled it,
    // the real mouse might interfere with the test.
    GetExtensionsToolbarContainer()->UpdateToolbarActionHoverCard(
        action_view, ToolbarActionHoverCardUpdateType::kHover);
  }

  void ClickMouseOnActionView(ToolbarActionView* action_view) {
    ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    action_view->OnMousePressed(mouse_event);
  }

  void MouseExitsFromExtensionsContainer() {
    ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    GetExtensionsToolbarContainer()->OnMouseExited(mouse_event);
  }

  void MouseMovesInExtensionsContainer() {
    ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
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
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;
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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install and pin one extension with host permissions and one without.
  auto extensionA = LoadExtensionAndPinIt("extensions/simple_with_popup");
  auto extensionB =
      InstallExtensionWithHostPermissions("All Urls Extension", "<all_urls>");
  PinExtension(extensionB->id());
  auto action_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(action_views.size(), 2u);

  // Navigate to a url extension B requests.
  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Hover over extension A and verify card anchors to its action.
  ToolbarActionView* actionA =
      GetExtensionsToolbarContainer()->GetViewForId(extensionA->id());
  HoverMouseOverActionView(actionA);
  views::Widget* const widget = hover_card()->GetWidget();
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), actionA);
  // Hover card should have the extension's name and no footnote since the
  // extension doesn't have access.
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(),
            actionA->view_controller()->GetActionName());
  EXPECT_EQ(hover_card()->GetFootnoteTitleTextForTesting(), u"");
  EXPECT_EQ(hover_card()->GetFootnoteDescriptionTextForTesting(), u"");

  // Hover over extension A and verify card anchors to its action. Note that the
  // widget is the same because it transitions from one action view to the
  // other.
  ToolbarActionView* actionB =
      GetExtensionsToolbarContainer()->GetViewForId(extensionB->id());
  HoverMouseOverActionView(actionB);
  views::test::WidgetVisibleWaiter(widget).Wait();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(hover_card()->GetAnchorView(), actionB);
  // Hover card should have the extension's name and footnote since the
  // extension has site access (by default).
  EXPECT_EQ(hover_card()->GetTitleTextForTesting(),
            actionB->view_controller()->GetActionName());
  EXPECT_EQ(
      hover_card()->GetFootnoteTitleTextForTesting(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_FOOTER_TITLE_HAS_ACCESS));
  EXPECT_EQ(
      hover_card()->GetFootnoteDescriptionTextForTesting(),
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_TOOLBAR_ACTION_HOVER_CARD_FOOTER_DESCRIPTION_EXTENSION_HAS_ACESSS,
          base::UTF8ToUTF16(net::GetHostAndPort(url))));
}

// Verify hover card is not visible when mouse moves inside the extensions
// container to a button that is not a toolbar icon view (which has its own 'on
// mouse moved' event listener).
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
