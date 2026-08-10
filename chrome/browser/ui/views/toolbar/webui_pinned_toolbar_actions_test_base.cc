// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions_test_base.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/collaboration/public/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/data_sharing/public/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/actions.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"

WebUIPinnedToolbarActionsTestBase::WebUIPinnedToolbarActionsTestBase()
    : WebUIToolbarWebViewTestBase(
          {features::kInitialWebUI, features::kWebUIPinnedToolbarActions,
           // Enable another control to prevent WebView going to 0 width.
           features::kWebUIReloadButton,
           features::kSkipIPCChannelPausingForNonGuests,
           features::kWebUIInProcessResourceLoadingV2,
           // `WebUIPinnedToolbarActionsBrowserTest.LensOverlayResultsIcon`
           // depends on `kRoundedIcons`.
           features::kRoundedIcons,
           // Facilitate testing kActionSidePanelShowComments
           collaboration::features::kCollaborationComments,
           // Facilitate testing kActionsSidePanelShowContextualTasks
           contextual_tasks::kContextualTasks,
           contextual_tasks::kContextualTasksForceEntryPointEligibility,
           // Facilitate testing kActionSendSharedTabGroupFeedback
           data_sharing::features::kDataSharingFeature},
          {}),
      kActionMappings{
          {kActionNewIncognitoWindow,
           toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow},
          {kActionShowPasswordsBubbleOrPage,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kShowPasswordsBubbleOrPage},
          {kActionShowPaymentsBubbleOrPage,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kShowPaymentsBubbleOrPage},
          {kActionShowAddressesBubbleOrPage,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kShowAddressesBubbleOrPage},
          {kActionSidePanelShowBookmarks,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks},
          {kActionSidePanelShowReadingList,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowReadingList},
          {kActionSidePanelShowHistoryCluster,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowHistoryCluster},
// ChromeOS doesn't support download button.
#if !BUILDFLAG(IS_CHROMEOS)
          {kActionShowDownloads,
           toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads},
#endif  // !BUILDFLAG(IS_CHROMEOS)
          {kActionClearBrowsingData,
           toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData},
          {kActionPrint, toolbar_ui_api::mojom::PinnedToolbarAction::kPrint},
          {kActionSidePanelShowLensOverlayResults,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowLensOverlayResults},
          {kActionShowTranslate,
           toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate},
          {kActionQrCodeGenerator,
           toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator},
          {kActionRouteMedia,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia},
          {kActionSidePanelShowReadAnything,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowReadAnything},
          {kActionCopyUrl,
           toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl},
          {kActionSendTabToSelf,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf},
          {kActionTaskManager,
           toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager},
          {kActionDevTools,
           toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools},
          {kActionSidePanelShowContextualTasks,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowContextualTasks},
          {kActionSidePanelShowLens,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowLens},
          {kActionSidePanelShowAboutThisSite,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowAboutThisSite},
          {kActionSidePanelShowCustomizeChrome,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowCustomizeChrome},
          {kActionSidePanelShowShoppingInsights,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowShoppingInsights},
          {kActionSidePanelShowMerchantTrust,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowMerchantTrust},
          {kActionSendSharedTabGroupFeedback,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSendSharedTabGroupFeedback},
          {kActionSidePanelShowComments,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowComments},
      } {
}

WebUIPinnedToolbarActionsTestBase::~WebUIPinnedToolbarActionsTestBase() =
    default;

void WebUIPinnedToolbarActionsTestBase::SetUpOnMainThread() {
  WebUIToolbarWebViewTestBase::SetUpOnMainThread();
  // Make everything pinnable and visible by default to facilitate testing.
  for (const auto& mapping : kActionMappings) {
    SetPinnableProperty(mapping.first, true);
    if (actions::ActionItem* action_item =
            actions::ActionManager::Get().FindAction(
                mapping.first,
                BrowserActions::From(browser())->root_action_item())) {
      action_item->SetVisible(true);
    }
  }
  model_ = PinnedToolbarActionsModel::Get(browser()->GetProfile());
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  // cast to get to the non-const variant.
  static_cast<views::View*>(webui_toolbar_view)
      ->GetColorProvider()
      ->SetColorForTesting(ui::kColorIcon, SK_ColorYELLOW);
  static_cast<views::View*>(webui_toolbar_view)
      ->GetColorProvider()
      ->SetColorForTesting(ui::kColorMenuIcon, SK_ColorMAGENTA);
}

void WebUIPinnedToolbarActionsTestBase::TearDownOnMainThread() {
  model_ = nullptr;
  WebUIToolbarWebViewTestBase::TearDownOnMainThread();
}

content::WebContents* WebUIPinnedToolbarActionsTestBase::GetWebContents() {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  return webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
}

WebUIPinnedToolbarActions*
WebUIPinnedToolbarActionsTestBase::GetPinnedToolbarActions() {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  return static_cast<WebUIPinnedToolbarActions*>(
      webui_toolbar_view->GetPinnedToolbarActions());
}

content::EvalJsResult WebUIPinnedToolbarActionsTestBase::EvalJsOnPinnedButton(
    content::WebContents* web_contents,
    toolbar_ui_api::mojom::PinnedToolbarAction action,
    const std::string& script_body) {
  return content::EvalJs(
      web_contents,
      base::StringPrintf(R"(
    (() => {
      const container = %s?.shadowRoot;
      if (!container) return false;
      const actionEl = Array.from(container.querySelectorAll(
                               'pinned-toolbar-action'))
                      .find(el => el.state && el.state.action === %d);
      if (!actionEl) return false;
      const btn = actionEl.shadowRoot.querySelector('cr-icon-button');
      %s
    })();
  )",
                         GetButtonAppJS("#pinnedToolbarActions").c_str(),
                         static_cast<int>(action), script_body.c_str()));
}

bool WebUIPinnedToolbarActionsTestBase::IsPinnedButtonVisible(
    content::WebContents* web_contents,
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  return EvalJsOnPinnedButton(web_contents, action,
                              "return !!btn && btn.checkVisibility();")
      .ExtractBool();
}

bool WebUIPinnedToolbarActionsTestBase::ClickPinnedButton(
    content::WebContents* web_contents,
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  return EvalJsOnPinnedButton(
             web_contents, action,
             "if (!btn || !btn.checkVisibility()) return false; btn.click(); "
             "return true;")
      .ExtractBool();
}

void WebUIPinnedToolbarActionsTestBase::SetPinnableProperty(
    actions::ActionId id,
    bool pinnable) {
  actions::ActionManager::Get()
      .FindAction(id, BrowserActions::From(browser())->root_action_item())
      ->SetProperty(actions::kActionItemPinnableKey,
                    static_cast<int>(
                        pinnable ? actions::ActionPinnableState::kPinnable
                                 : actions::ActionPinnableState::kNotPinnable));
}

views::View* WebUIPinnedToolbarActionsTestBase::GetLocationBarView() {
  return BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->location_bar_view();
}

views::BubbleAnchor WebUIPinnedToolbarActionsTestBase::GetToolbarBubbleAnchor(
    actions::ActionId action_id) {
  return BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar_button_provider()
      ->GetBubbleAnchor(action_id);
}

void WebUIPinnedToolbarActionsTestBase::PinAction(
    actions::ActionId action_id,
    toolbar_ui_api::mojom::PinnedToolbarAction mojom_action) {
  auto* web_contents = GetWebContents();
  auto* pinned_actions = GetPinnedToolbarActions();
  ui::ElementIdentifier id =
      pinned_toolbar_actions::GetElementIdentifierForAction(action_id);

  // Verify it's not pinned initially.
  if (id) {
    CHECK_EQ(id, webui_toolbar::ActionIdToElementIdentifier(action_id));
    EXPECT_FALSE(BrowserElements::From(browser())->GetElement(id));
  }
  EXPECT_TRUE(pinned_actions->GetBubbleAnchor(action_id).IsNull());
  bool missing_anchor = false;
  pinned_actions->GetBubbleAnchorAsync(
      action_id, base::BindLambdaForTesting([&](BubbleAnchorResult anchor) {
        EXPECT_FALSE(anchor.has_value());
        EXPECT_EQ(anchor.error(), GetAnchorFailureReason::kAnchorNotFound);
        missing_anchor = true;
      }));
  EXPECT_TRUE(missing_anchor);
  EXPECT_EQ(GetToolbarBubbleAnchor(action_id).GetIfView(),
            GetLocationBarView());

  model_->UpdatePinnedState(action_id, true);
  // Test async anchor fetching.
  base::RunLoop run_loop;
  pinned_actions->GetBubbleAnchorAsync(
      action_id, base::BindLambdaForTesting([&](BubbleAnchorResult anchor) {
        EXPECT_TRUE(anchor.has_value());
        EXPECT_FALSE(anchor.value().IsNull());
        run_loop.Quit();
      }));
  run_loop.Run();
  // Test sync anchor fetching.
  EXPECT_FALSE(pinned_actions->GetBubbleAnchor(action_id).IsNull());
  EXPECT_TRUE(GetToolbarBubbleAnchor(action_id).GetIfElement());
  bool found_anchor = false;
  pinned_actions->GetBubbleAnchorAsync(
      action_id, base::BindLambdaForTesting([&](BubbleAnchorResult anchor) {
        EXPECT_TRUE(anchor.has_value());
        EXPECT_FALSE(anchor.value().IsNull());
        found_anchor = true;
      }));
  EXPECT_TRUE(found_anchor);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's not highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "!btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Verify it's trackable.
  if (id) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return BrowserElements::From(browser())->GetElement(id) != nullptr;
    }));
  }
}

void WebUIPinnedToolbarActionsTestBase::UnpinAction(
    actions::ActionId action_id,
    toolbar_ui_api::mojom::PinnedToolbarAction mojom_action) {
  auto* web_contents = GetWebContents();
  auto* pinned_actions = GetPinnedToolbarActions();
  ui::ElementIdentifier id =
      pinned_toolbar_actions::GetElementIdentifierForAction(action_id);

  model_->UpdatePinnedState(action_id, false);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));

  if (id) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return BrowserElements::From(browser())->GetElement(id) == nullptr;
    }));
  }
  EXPECT_TRUE(pinned_actions->GetBubbleAnchor(action_id).IsNull());
  EXPECT_EQ(GetToolbarBubbleAnchor(action_id).GetIfView(),
            GetLocationBarView());
}

void WebUIPinnedToolbarActionsTestBase::VerifyPinnedToolbarWidth() {
  auto* web_contents = GetWebContents();
  auto* pinned_actions = GetPinnedToolbarActions();

  // Verify HTML element width matches C++ calculated width.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents,
                           base::StringPrintf(
                               R"(
      (() => {
        const el = %s;
        return el ? Math.round(el.getBoundingClientRect().width) : -1;
      })();
    )",
                               GetButtonAppJS("#pinnedToolbarActions").c_str()))
               .ExtractInt() == pinned_actions->GetWidth();
  }));
}
