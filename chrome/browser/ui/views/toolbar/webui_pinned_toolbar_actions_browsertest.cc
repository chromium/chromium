// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions_test_base.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/ax_update_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_MAC)
#include "base/process/process_handle.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#endif

namespace {

// Observes accessibility events to capture announcement text.
class AXAnnouncementObserver : public views::AXUpdateObserver {
 public:
  explicit AXAnnouncementObserver(views::AXUpdateNotifier* notifier) {
    observation_.Observe(notifier);
#if BUILDFLAG(IS_MAC)
    recorder_ = content::AXInspectFactory::CreateRecorder(
        content::AXInspectFactory::DefaultPlatformRecorderType(),
        /*manager=*/nullptr, base::GetCurrentProcId());
    recorder_->ListenToEvents(base::BindRepeating(
        &AXAnnouncementObserver::OnMacEvent, base::Unretained(this)));
#endif
  }

  // Waits for the expected announcement to be received. Returns true on
  // success, false on timeout.
  // On macOS, this will only wait for any announcement to be received.
  bool verify_last_announcement(int message_id) {
    bool result = base::test::RunUntil([&]() {
#if BUILDFLAG(IS_MAC)
      return mac_announcement_received_;
#else
      return last_announcement_ == l10n_util::GetStringUTF16(message_id);
#endif
    });
    // Reset after each verification to allow subsequent announcements to be
    // verified correctly.
    last_announcement_.clear();
#if BUILDFLAG(IS_MAC)
    mac_announcement_received_ = false;
#endif

    return result;
  }

 private:
  // views::AXUpdateObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kAlert) {
      ui::AXNodeData node_data;
      view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
      last_announcement_ =
          node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
    }
  }

#if BUILDFLAG(IS_MAC)
  void OnMacEvent(const std::string& event) {
    if (event.find("AXAnnouncementRequested") != std::string::npos) {
      mac_announcement_received_ = true;
    }
  }

  std::unique_ptr<ui::AXEventRecorder> recorder_;
  bool mac_announcement_received_ = false;
#endif

  std::u16string last_announcement_;
  base::ScopedObservation<views::AXUpdateNotifier, views::AXUpdateObserver>
      observation_{this};
};

}  // namespace

class WebUIPinnedToolbarActionsBrowserTest
    : public WebUIPinnedToolbarActionsTestBase {};

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PinUnpinIndividually) {
  for (const auto& [action_id, mojom_action] : kActionMappings) {
    PinAction(action_id, mojom_action);
    UnpinAction(action_id, mojom_action);
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, PinAllTogether) {
  for (const auto& [action_id, mojom_action] : kActionMappings) {
    PinAction(action_id, mojom_action);
    EXPECT_NO_FATAL_FAILURE(VerifyPinnedToolbarWidth());
  }

  for (const auto& [action_id, mojom_action] : kActionMappings) {
    UnpinAction(action_id, mojom_action);
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, RouteMediaIcons) {
  auto* web_contents = GetWebContents();
  auto* action_item = static_cast<actions::StatefulImageActionItem*>(
      actions::ActionManager::Get().FindAction(
          kActionRouteMedia,
          BrowserActions::From(browser())->root_action_item()));

  struct Test {
    base::raw_ref<const gfx::VectorIcon> icon;
    std::string_view expected_icon;
  };

  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia;

  const auto kRouteMediaIcons =
      features::IsRoundedIconsEnabled()
          ? std::vector<Test>(
                {{base::raw_ref(vector_icons::kCastIcon),
                  std::string_view("webui-toolbar:cast")},
                 {base::raw_ref(vector_icons::kCastWarningIcon),
                  std::string_view("webui-toolbar:cast_warning")},
                 {base::raw_ref(vector_icons::kCastPauseIcon),
                  std::string_view("webui-toolbar:cast_pause")},
                 {base::raw_ref(vector_icons::kCastConnectedIcon),
                  std::string_view("webui-toolbar:cast_connected")}})
          : std::vector<Test>(
                {{base::raw_ref(
                      vector_icons::kMediaRouterIdleChromeRefreshOldIcon),
                  std::string_view(
                      "webui-toolbar:media_router_idle_chrome_refresh_old")},
                 {base::raw_ref(
                      vector_icons::kMediaRouterWarningChromeRefreshOldIcon),
                  std::string_view(
                      "webui-toolbar:media_router_warning_chrome_refresh_old")},
                 {base::raw_ref(vector_icons::kMediaRouterPausedOldIcon),
                  std::string_view("webui-toolbar:media_router_paused_old")},
                 {base::raw_ref(
                      vector_icons::kMediaRouterActiveChromeRefreshOldIcon),
                  std::string_view(
                      "webui-toolbar:media_router_active_chrome_refresh_old")},
                 {base::raw_ref(kCastChromeRefreshOldIcon),
                  std::string_view("webui-toolbar:cast_chrome_refresh_old")}});

  for (const auto& test : kRouteMediaIcons) {
    SCOPED_TRACE(test.expected_icon);
    action_item->SetStatefulImage(ui::ImageModel::FromVectorIcon(*test.icon));
    PinAction(kActionRouteMedia, mojom_action);

    // Make sure the icon got wired through.
    EXPECT_EQ(test.expected_icon,
              EvalJsOnPinnedButton(
                  web_contents, mojom_action,
                  "return btn?.getAttribute('iron-icon') || '(null)'"));

    // And the color.
    EXPECT_EQ(
        "--cr-icon-button-fill-color: rgba(255, 0, 255, 1.00);",
        EvalJsOnPinnedButton(web_contents, mojom_action,
                             "return btn?.getAttribute('style') || '(null)'"));

    UnpinAction(kActionRouteMedia, mojom_action);
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PasswordManagerIcon) {
  auto* web_contents = GetWebContents();
  auto* action_item = static_cast<actions::StatefulImageActionItem*>(
      actions::ActionManager::Get().FindAction(
          kActionRouteMedia,
          BrowserActions::From(browser())->root_action_item()));
  action_item->SetStatefulImage(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled()
          ? vector_icons::kPasswordManagerIcon
          : vector_icons::kPasswordManagerOldIcon));
  PinAction(
      kActionShowPasswordsBubbleOrPage,
      toolbar_ui_api::mojom::PinnedToolbarAction::kShowPasswordsBubbleOrPage);
  EXPECT_EQ(features::IsRoundedIconsEnabled()
                ? "webui-toolbar:password_manager"
                : "webui-toolbar:password_manager_old",
            EvalJsOnPinnedButton(
                web_contents,
                toolbar_ui_api::mojom::PinnedToolbarAction::
                    kShowPasswordsBubbleOrPage,
                "return btn?.getAttribute('iron-icon') || '(null)'"));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, SidePanelToggle) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionSidePanelShowCustomizeChrome;
  auto mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowCustomizeChrome;

  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  auto is_any_side_panel_showing = [&]() {
    return side_panel_ui->IsSidePanelShowing();
  };

  // Show side panel.
  EXPECT_TRUE(ClickPinnedButton(web_contents, mojom_action));
  ASSERT_TRUE(base::test::RunUntil(is_any_side_panel_showing));

  // Dismiss side panel.
  EXPECT_TRUE(ClickPinnedButton(web_contents, mojom_action));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !is_any_side_panel_showing(); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, InvokeActions) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  // QR code generator and translate actions only work with a legitimate
  // non-chrome:// URL
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/href_translate_test.html")));

  for (const auto& [action_id, mojom_action] : kActionMappings) {
    auto* action_item = actions::ActionManager::Get().FindAction(
        action_id, BrowserActions::From(browser())->root_action_item());
    ASSERT_TRUE(action_item);
    action_item->SetEnabled(true);
    bool invoked = false;
    action_item->SetInvokeActionCallback(base::BindLambdaForTesting(
        [&](actions::ActionItem* item,
            actions::ActionInvocationContext context) { invoked = true; }));

    model_->UpdatePinnedState(action_id, true);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

    EXPECT_TRUE(ClickPinnedButton(web_contents, mojom_action));
    ASSERT_TRUE(base::test::RunUntil([&]() { return invoked; }));
    model_->UpdatePinnedState(action_id, false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, EphemeralActions) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  // Initially not pinned and not visible.
  ASSERT_FALSE(model_->Contains(action_id));
  ASSERT_FALSE(IsPinnedButtonVisible(web_contents, mojom_action));

  // Show ephemerally.
  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action_id, true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Hide ephemerally.
  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action_id, false);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       UpdateActionState) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  // Initially not pinned and not visible.
  ASSERT_FALSE(model_->Contains(action_id));
  ASSERT_FALSE(IsPinnedButtonVisible(web_contents, mojom_action));

  // Activate action.
  webui_toolbar_view->GetPinnedToolbarActions()->UpdateActionState(action_id,
                                                                   true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Deactivate action.
  webui_toolbar_view->GetPinnedToolbarActions()->UpdateActionState(action_id,
                                                                   false);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));

  model_->UpdatePinnedState(action_id, true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's not highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "!btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Activate action.
  webui_toolbar_view->GetPinnedToolbarActions()->UpdateActionState(action_id,
                                                                   true);

  // Verify it's highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_contents, mojom_action,
                                "return !!btn && "
                                "btn.hasAttribute('is-menu-open');")
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       ButtonEnabledState) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  auto* action_item = actions::ActionManager::Get().FindAction(
      action_id, BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action_item);

  // Disable the action.
  action_item->SetEnabled(false);

  // Verify button is disabled in WebUI.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_contents, mojom_action,
                                "return !!btn && btn.disabled;")
        .ExtractBool();
  }));

  // Re-enable.
  action_item->SetEnabled(true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !EvalJsOnPinnedButton(web_contents, mojom_action,
                                 "return !!btn && btn.disabled;")
                .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, PinUnpinnable) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Make unpinnable.
  SetPinnableProperty(action_id, false);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));
  // Make sure it's still pinned.
  ASSERT_TRUE(model_->Contains(action_id));

  // Make pinnable.
  SetPinnableProperty(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       ActivatedRendering) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  PinAction(action_id, mojom_action);

  auto verify_activated = [&](bool expected) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return EvalJsOnPinnedButton(
                 web_contents, mojom_action,
                 base::StringPrintf(
                     "const indicator = "
                     "actionEl.shadowRoot.querySelector('.status-indicator'); "
                     "return btn.hasAttribute('is-activated') === %s && "
                     "!!indicator && indicator.checkVisibility() === %s;",
                     expected ? "true" : "false", expected ? "true" : "false"))
          .ExtractBool();
    }));
  };

  // Initially not activated.
  verify_activated(false);

  // Set activated.
  actions::ActionManager::Get()
      .FindAction(action_id,
                  BrowserActions::From(browser())->root_action_item())
      ->SetProperty(kActionItemUnderlineIndicatorKey, true);
  verify_activated(true);

  // Set not activated.
  actions::ActionManager::Get()
      .FindAction(action_id,
                  BrowserActions::From(browser())->root_action_item())
      ->SetProperty(kActionItemUnderlineIndicatorKey, false);
  verify_activated(false);
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, StateAccessors) {
  PinnedToolbarActions* view =
      GetWebUIToolbarWebView(browser())->GetPinnedToolbarActions();

  // Pin then pop out.
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, true);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  view->ShowActionEphemerallyInToolbar(kActionPrint, true);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, false);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_TRUE(view->IsActionPoppedOut(kActionPrint));
  view->ShowActionEphemerallyInToolbar(kActionPrint, false);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));

  // Pop out then pin.
  view->ShowActionEphemerallyInToolbar(kActionPrint, true);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_TRUE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, true);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  view->ShowActionEphemerallyInToolbar(kActionPrint, false);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, false);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       TextAndAriaLabelAttributes) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  auto* action_item = actions::ActionManager::Get().FindAction(
      action_id, BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action_item);

  // Pin it so it renders.
  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Test the default appropriate values are set for the tooltip and ax text.
  std::string default_name =
      base::UTF16ToUTF8(action_item->GetAccessibleName().empty()
                            ? action_item->GetTooltipText()
                            : action_item->GetAccessibleName());
  std::string default_description =
      base::UTF16ToUTF8(action_item->GetTooltipText());

  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         default_name);
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kButton;
  find_criteria.name = default_name;
  ui::AXPlatformNodeDelegate* print_node =
      content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);

  EXPECT_EQ(default_name,
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(default_description, print_node->GetStringAttribute(
                                     ax::mojom::StringAttribute::kDescription));

  // Test all values are provided.
  action_item->SetTooltipText(u"tooltip");
  action_item->SetAccessibleName(u"accessible_name");

  content::WaitForAccessibilityTreeToChange(web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "accessible_name");
  find_criteria.name = "accessible_name";
  print_node = content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);
  EXPECT_EQ("accessible_name",
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("tooltip", print_node->GetStringAttribute(
                           ax::mojom::StringAttribute::kDescription));

  // Test accessible_name is empty (Fallback to Tooltip).
  action_item->SetAccessibleName(u"");

  content::WaitForAccessibilityTreeToChange(web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "tooltip");
  find_criteria.name = "tooltip";
  print_node = content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);
  EXPECT_EQ("tooltip",
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("tooltip", print_node->GetStringAttribute(
                           ax::mojom::StringAttribute::kDescription));

  // Test tooltip and accessible_name are empty.
  action_item->SetTooltipText(u"");

  content::WaitForAccessibilityTreeToChange(web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents, "");
  find_criteria.name = "";
  print_node = content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);
  EXPECT_EQ("",
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("", print_node->GetStringAttribute(
                    ax::mojom::StringAttribute::kDescription));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, ToolbarDivider) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  // Clear any default pinned actions.
  std::vector<actions::ActionId> pinned_ids = model_->PinnedActionIds();
  for (actions::ActionId id : pinned_ids) {
    model_->UpdatePinnedState(id, false);
  }

  auto is_divider_visible = [&]() {
    return content::EvalJs(
               web_contents,
               base::StrCat({GetButtonAppJS("#pinnedToolbarActions"),
                             "?.shadowRoot?.querySelector('toolbar-"
                             "divider') !== null"}))
        .ExtractBool();
  };

  // Helper to check if divider is at expected position.
  // returns index of divider or -1 if not found.
  auto find_divider_index = [&]() {
    return content::EvalJs(web_contents,
                           base::StringPrintf(
                               R"(
      (() => {
        const children = Array.from(%s?.shadowRoot?.children || [])
                            .filter(el => ['pinned-toolbar-action',
                                           'toolbar-divider'].includes(
                                              el.tagName.toLowerCase()));
        return children.findIndex(
            el => el.tagName.toLowerCase() === 'toolbar-divider');
      })();
    )",
                               GetButtonAppJS("#pinnedToolbarActions").c_str()))
        .ExtractInt();
  };

  auto find_action_index =
      [&](toolbar_ui_api::mojom::PinnedToolbarAction action) {
        return content::EvalJs(
                   web_contents,
                   base::StringPrintf(
                       R"(
      (() => {
        const shadowRoot = %s?.shadowRoot;
        if (!shadowRoot) return -1;
        const children = Array.from(shadowRoot.children)
                            .filter(el => ['pinned-toolbar-action',
                                           'toolbar-divider'].includes(
                                              el.tagName.toLowerCase()));
        return children.findIndex(el => el.state && el.state.action === %d);
      })();
    )",
                       GetButtonAppJS("#pinnedToolbarActions").c_str(),
                       static_cast<int>(action)))
            .ExtractInt();
      };

  // 1) Initially no actions, no divider.
  ASSERT_TRUE(base::test::RunUntil([&]() { return !is_divider_visible(); }));

  // 2) Pin one action, divider should appear after it.
  actions::ActionId action1 = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action1 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  model_->UpdatePinnedState(action1, true);
  ASSERT_TRUE(base::test::RunUntil([&]() { return is_divider_visible(); }));

  int action1_index = find_action_index(mojom_action1);
  int divider_index = find_divider_index();
  EXPECT_EQ(divider_index, action1_index + 1);

  // 3) Pop out another action, divider should be between them.
  actions::ActionId action2 = kActionShowTranslate;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action2 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;

  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action2, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return find_action_index(mojom_action2) != -1; }));

  action1_index = find_action_index(mojom_action1);
  divider_index = find_divider_index();
  int action2_index = find_action_index(mojom_action2);

  EXPECT_LT(action1_index, divider_index);
  EXPECT_LT(divider_index, action2_index);
  EXPECT_EQ(divider_index, action1_index + 1);
  EXPECT_EQ(action2_index, divider_index + 1);

  // 4) Unpin action1 and hide action2 ephemerally, divider should disappear.
  model_->UpdatePinnedState(action1, false);
  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action2, false);
  ASSERT_TRUE(base::test::RunUntil([&]() { return !is_divider_visible(); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       A11yAnnouncements) {
  AXAnnouncementObserver announcement_observer(views::AXUpdateNotifier::Get());

  actions::ActionId action_id = kActionSendSharedTabGroupFeedback;

  // Initial State: Unpinned.
  ASSERT_FALSE(model_->Contains(action_id));

  auto invoke_pin_unpin = [&](actions::ActionId pin_unpin_action) {
    actions::ActionManager::Get()
        .FindAction(pin_unpin_action,
                    BrowserActions::From(browser())->root_action_item())
        ->InvokeAction(actions::ActionInvocationContext::Builder()
                           .SetProperty(kActionIdKey, action_id)
                           .Build());
  };

  // Pin via Action Invocation.
  invoke_pin_unpin(kActionPinActionToToolbar);

  // Expect an announcement that the action was pinned.
  EXPECT_TRUE(announcement_observer.verify_last_announcement(
      IDS_TOOLBAR_BUTTON_PINNED));
  ASSERT_TRUE(model_->Contains(action_id));

  // Unpin via Action Invocation.
  invoke_pin_unpin(kActionUnpinActionFromToolbar);

  // Expect an announcement that the action was unpinned.
  EXPECT_TRUE(announcement_observer.verify_last_announcement(
      IDS_TOOLBAR_BUTTON_UNPINNED));
  ASSERT_FALSE(model_->Contains(action_id));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       AboutThisSiteIcon) {
  auto* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  // Pin "About This Site" action.
  PinAction(
      kActionSidePanelShowAboutThisSite,
      toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowAboutThisSite);

  // Define the expected icon name.
  std::string expected_icon;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_icon = "internal-icons:page_insights";
#else
  expected_icon = features::IsRoundedIconsEnabled()
                      ? "webui-toolbar:info"
                      : "webui-toolbar:info_chrome_refresh_old";
#endif

  // Verify iron-icon attribute in WebUI.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_contents,
                                toolbar_ui_api::mojom::PinnedToolbarAction::
                                    kSidePanelShowAboutThisSite,
                                "return btn?.getAttribute('iron-icon') || '';")
               .ExtractString() == expected_icon;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       LensOverlayResultsIcon) {
  auto* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  // Pin "Lens Overlay Results" action.
  PinAction(kActionSidePanelShowLensOverlayResults,
            toolbar_ui_api::mojom::PinnedToolbarAction::
                kSidePanelShowLensOverlayResults);

  // Define the expected icon name.
  std::string expected_icon;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_icon = "internal-icons:google_lens_monochrome_logo";
#else
  expected_icon = features::IsRoundedIconsEnabled()
                      ? "webui-toolbar:search"
                      : "webui-toolbar:search_chrome_refresh_old_icon";
#endif

  // Verify iron-icon attribute in WebUI.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_contents,
                                toolbar_ui_api::mojom::PinnedToolbarAction::
                                    kSidePanelShowLensOverlayResults,
                                "return btn?.getAttribute('iron-icon') || '';")
               .ExtractString() == expected_icon;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PostOrQueueActionAfterAnimation_SingleButton) {
  auto* pinned_actions = GetPinnedToolbarActions();
  actions::ActionId action_id = kActionSidePanelShowBookmarks;

  ASSERT_FALSE(pinned_actions->IsActionPinned(action_id));

  bool callback_called = false;
  base::RunLoop run_loop;

  model_->UpdatePinnedState(action_id, true);

  pinned_actions->PostOrQueueActionAfterAnimation(
      base::BindLambdaForTesting([&]() {
        callback_called = true;
        views::BubbleAnchor anchor = pinned_actions->GetBubbleAnchor(action_id);
        EXPECT_FALSE(anchor.IsNull());
        run_loop.Quit();
      }));

  EXPECT_FALSE(callback_called);
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Cleanup.
  model_->UpdatePinnedState(action_id, false);
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PostOrQueueActionAfterAnimation_MultipleButtons) {
  auto* pinned_actions = GetPinnedToolbarActions();
  actions::ActionId action_id_1 = kActionSidePanelShowBookmarks;
  actions::ActionId action_id_2 = kActionSidePanelShowReadingList;

  ASSERT_FALSE(pinned_actions->IsActionPinned(action_id_1));
  ASSERT_FALSE(pinned_actions->IsActionPinned(action_id_2));

  bool callback_called = false;
  base::RunLoop run_loop;

  model_->UpdatePinnedState(action_id_1, true);
  model_->UpdatePinnedState(action_id_2, true);

  pinned_actions->PostOrQueueActionAfterAnimation(
      base::BindLambdaForTesting([&]() {
        callback_called = true;
        EXPECT_FALSE(pinned_actions->GetBubbleAnchor(action_id_1).IsNull());
        EXPECT_FALSE(pinned_actions->GetBubbleAnchor(action_id_2).IsNull());
        run_loop.Quit();
      }));

  EXPECT_FALSE(callback_called);
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Cleanup.
  model_->UpdatePinnedState(action_id_1, false);
  model_->UpdatePinnedState(action_id_2, false);
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PostOrQueueActionAfterAnimation_ImmediateEmpty) {
  auto* pinned_actions = GetPinnedToolbarActions();

  // Ensure nothing is pinned.
  for (const auto& [action_id, mojom_action] : kActionMappings) {
    if (pinned_actions->IsActionPinned(action_id)) {
      model_->UpdatePinnedState(action_id, false);
    }
  }

  bool callback_called = false;
  pinned_actions->PostOrQueueActionAfterAnimation(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  EXPECT_TRUE(callback_called);
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PostOrQueueActionAfterAnimation_ImmediateWithPinned) {
  auto* pinned_actions = GetPinnedToolbarActions();
  actions::ActionId action_id = kActionSidePanelShowBookmarks;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks;

  PinAction(action_id, mojom_action);

  bool callback_called = false;
  pinned_actions->PostOrQueueActionAfterAnimation(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  EXPECT_TRUE(callback_called);

  UnpinAction(action_id, mojom_action);
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       MovePinnedAction_UpdatesModelAndOrder) {
  auto* pinned_actions = GetPinnedToolbarActions();

  // Ensure nothing is pinned before testing.
  while (!pinned_actions->PinnedActionIds().empty()) {
    model_->UpdatePinnedState(pinned_actions->PinnedActionIds().front(), false);
  }

  actions::ActionId action1 = kActionClearBrowsingData;
  actions::ActionId action2 = kActionPrint;

  model_->UpdatePinnedState(action1, true);
  model_->UpdatePinnedState(action2, true);

  const auto& ids_before = pinned_actions->PinnedActionIds();
  ASSERT_GE(ids_before.size(), 2u);

  pinned_actions->MovePinnedAction(action2, 0);

  const auto& ids_after = pinned_actions->PinnedActionIds();
  EXPECT_EQ(ids_after[0], action2);

  // Cleanup.
  model_->UpdatePinnedState(action1, false);
  model_->UpdatePinnedState(action2, false);
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       MovePinnedActionBy_RespectsBounds) {
  auto* pinned_actions = GetPinnedToolbarActions();

  // Ensure nothing is pinned before testing.
  while (!pinned_actions->PinnedActionIds().empty()) {
    model_->UpdatePinnedState(pinned_actions->PinnedActionIds().front(), false);
  }

  actions::ActionId action1 = kActionClearBrowsingData;
  actions::ActionId action2 = kActionPrint;

  model_->UpdatePinnedState(action1, true);
  model_->UpdatePinnedState(action2, true);

  pinned_actions->MovePinnedAction(action2, 0);
  EXPECT_EQ(pinned_actions->PinnedActionIds()[0], action2);

  pinned_actions->MovePinnedActionBy(action2, -1);
  EXPECT_EQ(pinned_actions->PinnedActionIds()[0], action2);

  pinned_actions->MovePinnedActionBy(action2, 1);
  EXPECT_NE(pinned_actions->PinnedActionIds()[0], action2);
  EXPECT_EQ(pinned_actions->PinnedActionIds()[1], action2);

  // Cleanup.
  model_->UpdatePinnedState(action1, false);
  model_->UpdatePinnedState(action2, false);
}
