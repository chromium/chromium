// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"
#include "chrome/browser/ui/views/toolbar/webui_performance_intervention_control.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebContentsId);

using ::performance_manager::testing::ScopedSetAllPagesDiscardableForTesting;
}  // namespace

class WebUIPerformanceInterventionInteractiveTest
    : public InteractiveFeaturePromoTest {
 public:
  WebUIPerformanceInterventionInteractiveTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPerformanceInterventionDialogFeature})) {
    feature_list_.InitWithFeatures(
        {features::kWebUIPerformanceInterventionButton,
         features::kWebUIReloadButton, ::features::kInitialWebUI,
         performance_manager::features::kPerformanceInterventionDemoMode},
        {});
  }

  ~WebUIPerformanceInterventionInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    unconditionally_discard_pages_ =
        std::make_unique<ScopedSetAllPagesDiscardableForTesting>();
    WaitForInitialWebUIToolbar(browser());
    // Set a larger suppression threshold to account for IPC latency on bots.
    auto* webview = GetWebUIToolbarWebView(browser());
    CHECK(webview);
    webview->GetPerformanceInterventionControlForTesting()
        ->SetSuppressionThresholdForTesting(base::Seconds(1));
  }

  void TearDownOnMainThread() override {
    unconditionally_discard_pages_.reset();
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  GURL GetURL() {
    return embedded_test_server()->GetURL("example.com", "/title1.html");
  }

  std::vector<resource_attribution::PageContext> GetPageContextForTabs(
      const std::vector<int>& tab_indices,
      Browser* browser) {
    std::vector<resource_attribution::PageContext> page_contexts;
    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    for (int index : tab_indices) {
      std::optional<resource_attribution::PageContext> context =
          resource_attribution::PageContext::FromWebContents(
              tab_strip_model->GetWebContentsAt(index));
      CHECK(context.has_value());
      page_contexts.push_back(context.value());
    }
    return page_contexts;
  }

  void NotifyActionableTabListChange(const std::vector<int>& tab_indices,
                                     Browser* browser) {
    performance_manager::user_tuning::PerformanceDetectionManager::GetInstance()
        ->NotifyActionableTabObserversForTesting(
            PerformanceDetectionManager::ResourceType::kCpu,
            GetPageContextForTabs(tab_indices, browser));
  }

  auto TriggerOnActionableTabListChange(const std::vector<int>& tab_indices) {
    return Do([&]() { NotifyActionableTabListChange(tab_indices, browser()); });
  }

  WebContentsInteractionTestUtil::DeepQuery ButtonDeepQuery() {
    return WebContentsInteractionTestUtil::DeepQuery(
        {"toolbar-app", "performance-intervention-button"});
  }

  views::View* GetWebUIWebViewForInstrument() {
    auto* parent = GetWebUIToolbarWebView(this->browser());
    CHECK(parent) << "Parent WebUIToolbarWebView is null!";
    auto* web_view = parent->GetWebViewForTesting();
    CHECK(web_view) << "GetWebViewForTesting() is null!";
    CHECK(web_view->GetWebContents()) << "web_view->GetWebContents() is null!";
    return web_view;
  }

  auto InstrumentWebUIToolbar() {
    return InstrumentNonTabWebView(
        kWebUIToolbarWebContentsId,
        base::BindOnce(&WebUIPerformanceInterventionInteractiveTest::
                           GetWebUIWebViewForInstrument,
                       base::Unretained(this)),
        /*wait_for_ready=*/true);
  }

  auto WaitForButtonShown(bool shown) {
    return WaitForJsResultAt(kWebUIToolbarWebContentsId, ButtonDeepQuery(),
                             shown ? "el => (!el.hidden)" : "el => el.hidden",
                             true);
  }

  auto CheckButtonActive(bool active) {
    return CheckJsResultAt(kWebUIToolbarWebContentsId, ActualButtonDeepQuery(),
                           "el => el.hasAttribute('is-activated')", active);
  }

  auto WaitForButtonActive(bool active) {
    return WaitForJsResultAt(kWebUIToolbarWebContentsId,
                             ActualButtonDeepQuery(),
                             "el => el.hasAttribute('is-activated')", active);
  }

  auto ClickButton() {
    // Simulate clicking on the WebUI toolbar button while the performance
    // intervention bubble dialog is showing. Because injecting physical mouse
    // events via ui_controls is prone to timing out on headless Windows trybots
    // when a dialog bubble begins tearing down, we simulate the click by
    // closing the bubble natively due to focus loss (since JS events don't
    // natively trigger blur) and dispatching pointerdown and click JS events.
    return Steps(
        Do([this]() {
          auto* webview = GetWebUIToolbarWebView(browser());
          CHECK(webview);
          auto* control =
              webview->GetPerformanceInterventionControlForTesting();
          CHECK(control);
          auto* bubble_dialog = control->GetBubbleDialogModelHostForTesting();
          if (bubble_dialog && bubble_dialog->GetWidget()) {
            bubble_dialog->GetWidget()->CloseWithReason(
                views::Widget::ClosedReason::kLostFocus);
          }
        }),
        ExecuteJsAt(kWebUIToolbarWebContentsId, ActualButtonDeepQuery(),
                    "(el) => {"
                    "  el.dispatchEvent(new PointerEvent('pointerdown', "
                    "{bubbles: true, cancelable: true, view: window, "
                    "button: 0}));"
                    "  el.dispatchEvent(new PointerEvent('click', "
                    "{bubbles: true, cancelable: true, view: window, "
                    "button: 0, pointerType: 'mouse'}));"
                    "}"));
  }

  WebContentsInteractionTestUtil::DeepQuery ActualButtonDeepQuery() {
    return WebContentsInteractionTestUtil::DeepQuery(
        {"toolbar-app", "performance-intervention-button", "#button"});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedSetAllPagesDiscardableForTesting>
      unconditionally_discard_pages_;
};

IN_PROC_BROWSER_TEST_F(WebUIPerformanceInterventionInteractiveTest,
                       ShowAndHideButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}), Do([this]() {
        auto* webview = GetWebUIToolbarWebView(this->browser());
        CHECK(webview) << "GetWebUIToolbarWebView returned nullptr!";
        auto* widget = webview->GetWidget();
        CHECK(widget) << "WebUIToolbarWebView has no Widget!";
        widget->LayoutRootViewIfNecessary();
      }),
      InstrumentWebUIToolbar(), WaitForButtonShown(true),
      WaitForButtonActive(true),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      ClickButton(),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      WaitForButtonShown(true), WaitForButtonActive(false),
      TriggerOnActionableTabListChange({}), WaitForButtonShown(false));
}

IN_PROC_BROWSER_TEST_F(WebUIPerformanceInterventionInteractiveTest,
                       HideButtonClosesBubble) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}), Do([this]() {
        auto* webview = GetWebUIToolbarWebView(this->browser());
        CHECK(webview) << "GetWebUIToolbarWebView returned nullptr!";
        auto* widget = webview->GetWidget();
        CHECK(widget) << "WebUIToolbarWebView has no Widget!";
        widget->LayoutRootViewIfNecessary();
      }),
      InstrumentWebUIToolbar(), WaitForButtonShown(true),
      WaitForButtonActive(true),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      Do([this]() {
        browser()->tab_strip_model()->CloseWebContentsAt(0, CLOSE_NONE);
      }),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody));
}

IN_PROC_BROWSER_TEST_F(WebUIPerformanceInterventionInteractiveTest,
                       RapidTriggerIntervention) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}), Do([this]() {
        auto* webview = GetWebUIToolbarWebView(this->browser());
        CHECK(webview) << "GetWebUIToolbarWebView returned nullptr!";
        auto* widget = webview->GetWidget();
        CHECK(widget) << "WebUIToolbarWebView has no Widget!";
        widget->LayoutRootViewIfNecessary();
      }),
      InstrumentWebUIToolbar(), WaitForButtonShown(true),
      WaitForButtonActive(true),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      Do([this]() {
        browser()->tab_strip_model()->CloseWebContentsAt(0, CLOSE_NONE);
        NotifyActionableTabListChange({0}, browser());
      }),
      WaitForButtonShown(true));
}

IN_PROC_BROWSER_TEST_F(WebUIPerformanceInterventionInteractiveTest,
                       DialogClosedByUserClickOnButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}), Do([this]() {
        auto* webview = GetWebUIToolbarWebView(this->browser());
        CHECK(webview) << "GetWebUIToolbarWebView returned nullptr!";
        auto* widget = webview->GetWidget();
        CHECK(widget) << "WebUIToolbarWebView has no Widget!";
        widget->LayoutRootViewIfNecessary();
      }),
      InstrumentWebUIToolbar(), WaitForButtonShown(true),
      WaitForButtonActive(true),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      ClickButton(),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      WaitForButtonActive(false),
      EnsureNotPresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody));
}

class WebUIPerformanceInterventionDisabledInteractiveTest
    : public InteractiveFeaturePromoTest {
 public:
  WebUIPerformanceInterventionDisabledInteractiveTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPerformanceInterventionDialogFeature})) {
    feature_list_.InitWithFeatures(
        {::features::kInitialWebUI, features::kWebUIReloadButton},
        {features::kWebUIPerformanceInterventionButton});
  }

  ~WebUIPerformanceInterventionDisabledInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    WaitForInitialWebUIToolbar(browser());
  }

  WebUIPerformanceInterventionControl* GetControl() {
    auto* webview = GetWebUIToolbarWebView(browser());
    CHECK(webview);
    return webview->GetPerformanceInterventionControlForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIPerformanceInterventionDisabledInteractiveTest,
                       OnClickedDoesNotCrash) {
  RunTestSequence(Do([this]() {
    WebUIPerformanceInterventionControl* control = GetControl();
    // This should not crash.
    control->OnClicked(true);
  }));
}
