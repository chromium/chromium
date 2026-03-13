// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_features.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2ElementId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
    ui::test::PollingElementStateObserver<std::u16string>,
    kReloadButtonTooltipState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingElementStateObserver<bool>,
                                    kReloadButtonHasPopupState);

// Observer to capture the reload type of the next navigation.
class ReloadTypeObserver : public content::WebContentsObserver {
 public:
  explicit ReloadTypeObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInPrimaryMainFrame() &&
        navigation_handle->HasCommitted()) {
      last_reload_type_ = navigation_handle->GetReloadType();
    }
  }

  void Reset() { last_reload_type_ = content::ReloadType::NONE; }
  content::ReloadType last_reload_type() const { return last_reload_type_; }

 private:
  content::ReloadType last_reload_type_ = content::ReloadType::NONE;
};
}  // namespace

class ReloadButtonAccessibilityTest : public InteractiveBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  ReloadButtonAccessibilityTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIReloadButton});
    }
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Wait for the toolbar to load if WebUI is enabled.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->RequestDeferShow(base::DoNothing());
    }));

    // Enable accessibility for this process. For WebUI we also need to wait for
    // the accessibility information to be passed from the WebUI renderer.
    std::optional<content::AccessibilityNotificationWaiter> load_waiter;
    if (GetParam()) {
      content::WebContents* toolbar_webcontents =
          browser()
              ->GetBrowserView()
              .toolbar_button_provider()
              ->GetWebUIToolbarViewForTesting()
              ->GetWebViewForTesting()
              ->GetWebContents();
      ASSERT_TRUE(toolbar_webcontents);

      // Enable accessibility mode for this toolbar and wait for it to populate.
      load_waiter.emplace(toolbar_webcontents, ax::mojom::Event::kLoadComplete);
    }

    bool accessibility_initially_disabled =
        content::BrowserAccessibilityState::GetInstance()
            ->GetAccessibilityMode()
            .is_mode_off();
    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForProcess(ui::kAXModeComplete);
    if (load_waiter) {
      // The linux-blink-web-tests-force-accessibility-rel try bot already has
      // accessibility turned on. If we try to wait for a notification when it
      // is already on, then we will likely wait forever (since we already
      // missed the notification). We still need to wait long enough to get the
      // accessibility info.
      if (accessibility_initially_disabled) {
        ASSERT_TRUE(load_waiter->WaitForNotification());
      } else {
        ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
          BrowserView* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser);
          ToolbarButtonProvider* provider =
              browser_view->toolbar_button_provider();
          if (WebUIToolbarWebView* toolbar_web_view =
                  provider->GetWebUIToolbarViewForTesting()) {
            return toolbar_web_view->GetWebViewForTesting()
                       ->web_contents()
                       ->GetAccessibilityRootNode() != nullptr;
          }
          return true;
        }));
      }
    }
  }

  static ui::AXNode* FindReloadButtonNode(ui::AXNode* node) {
    if (node->data().role == ax::mojom::Role::kButton &&
        node->data().GetString16Attribute(ax::mojom::StringAttribute::kName) ==
            l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD)) {
      return node;
    }
    for (ui::AXNode* child : node->children()) {
      if (ui::AXNode* found = FindReloadButtonNode(child)) {
        return found;
      }
    }
    return nullptr;
  }

  static ui::AXNodeData GetReloadAXNodeData(const ui::TrackedElement* el,
                                            const char* file,
                                            int line) {
    if (auto* const view_el = el->AsA<views::TrackedElementViews>()) {
      ui::AXNodeData node_data;
      view_el->view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
      return node_data;
    } else if (auto* const webui_el = el->AsA<ui::TrackedElementWebUI>()) {
      ui::AXNode* root =
          webui_el->handler()->web_contents()->GetAccessibilityRootNode();
      if (!root) {
        ADD_FAILURE_AT(file, line) << "Could not get AXNode root";
        return {};
      }
      ui::AXNode* reload_button = FindReloadButtonNode(root);
      if (!reload_button) {
        ADD_FAILURE_AT(file, line) << "Could not find AXNode reload_button";
        return {};
      }
      return reload_button->data();
    } else {
      ADD_FAILURE_AT(file, line) << "Unsupported element type";
      return {};
    }
  }

  // Helper to check tooltip/accessible name.
  auto WaitForReloadButtonTooltip(int string_id) {
    return Steps(
        PollElement(kReloadButtonTooltipState, kReloadButtonElementId,
                    base::BindRepeating(&GetReloadButtonTooltip)),
        WaitForState(kReloadButtonTooltipState,
                     std::make_optional(l10n_util::GetStringUTF16(string_id))),
        StopObservingState(kReloadButtonTooltipState));
  }

  auto WaitForReloadButtonHasPopup(bool has_popup) {
    return Steps(
        PollElement(kReloadButtonHasPopupState, kReloadButtonElementId,
                    base::BindRepeating(&GetReloadButtonHasPopup)),
        WaitForState(kReloadButtonHasPopupState, std::make_optional(has_popup)),
        StopObservingState(kReloadButtonHasPopupState));
  }

  // Safe MoveMouseTo that works for both Views and WebUI.
  auto MoveMouseToElement(ui::ElementIdentifier id) {
    return MoveMouseTo(id, base::BindOnce([](ui::TrackedElement* el) {
                         return el->GetScreenBounds().CenterPoint();
                       }));
  }

  static std::u16string GetReloadButtonTooltip(const ui::TrackedElement* el) {
    return GetReloadAXNodeData(el, __FILE__, __LINE__)
        .GetString16Attribute(ax::mojom::StringAttribute::kDescription);
  }

  static bool GetReloadButtonHasPopup(const ui::TrackedElement* el) {
    return GetReloadAXNodeData(el, __FILE__, __LINE__)
               .GetIntAttribute(ax::mojom::IntAttribute::kHasPopup) !=
           std::to_underlying(ax::mojom::HasPopup::kFalse);
  }

 protected:
  std::unique_ptr<ReloadTypeObserver> reload_observer_;
  ui::AXNodeData node_data_;
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ReloadButtonAccessibilityTest, testing::Bool());

// Tests that clicking the reload button correctly transitions to a stop button
// during loading and back to reload when loading is finished or stopped.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ReloadStateTransition DISABLED_ReloadStateTransition
#else
#define MAYBE_ReloadStateTransition ReloadStateTransition
#endif
// TODO(behamilton): On Windows this test is flaky.
IN_PROC_BROWSER_TEST_P(ReloadButtonAccessibilityTest,
                       MAYBE_ReloadStateTransition) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/slow");
  ASSERT_TRUE(embedded_test_server()->Start());

  RunTestSequence(
      InstrumentTab(kWebContentsElementId), WaitForShow(kReloadButtonElementId),
      // Initial state: Reload
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD),

      // Setup observer
      WithElement(kWebContentsElementId,
                  [this](ui::TrackedElement* el) {
                    reload_observer_ = std::make_unique<ReloadTypeObserver>(
                        AsInstrumentedWebContents(el)->web_contents());
                  }),

      // Start navigation to slow page
      InstrumentNextTab(kWebContents2ElementId), Do([&]() {
        browser()->OpenURL(
            content::OpenURLParams(embedded_test_server()->GetURL("/slow"),
                                   content::Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false),
            /*navigation_handle_callback=*/{});
      }),

      // Wait for navigation to start and button to turn into Stop.
      // We must explicitly wait for Stop to ensure we don't race the initial
      // state.
      WaitForReloadButtonTooltip(IDS_TOOLTIP_STOP),

      // Stop the navigation
      MoveMouseToElement(kReloadButtonElementId), ClickMouse(),

      // Button should turn back into Reload
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD));
}

// Tests that clicking the reload button performs a normal reload.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NormalReload DISABLED_NormalReload
#else
#define MAYBE_NormalReload NormalReload
#endif
IN_PROC_BROWSER_TEST_P(ReloadButtonAccessibilityTest, MAYBE_NormalReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url),
      WaitForShow(kReloadButtonElementId),
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD),

      // Setup observer BEFORE click
      WithElement(kWebContentsElementId,
                  [this](ui::TrackedElement* el) {
                    reload_observer_ = std::make_unique<ReloadTypeObserver>(
                        AsInstrumentedWebContents(el)->web_contents());
                  }),

      // Click the reload button
      MoveMouseToElement(kReloadButtonElementId), ClickMouse(),

      // Wait for reload to complete
      WaitForWebContentsNavigation(kWebContentsElementId),

      // Wait for reload button to be back in Reload state
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD),

      // Verify that the reload was normal
      CheckResult([this]() { return reload_observer_->last_reload_type(); },
                  content::ReloadType::NORMAL));
}

// Tests that clicking the reload button with a modifier (Shift) performs a
// hard reload (bypassing cache).
// TODO(behamilton): On Windows this test is flaky.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_HardReloadModifier DISABLED_HardReloadModifier
#else
#define MAYBE_HardReloadModifier HardReloadModifier
#endif
IN_PROC_BROWSER_TEST_P(ReloadButtonAccessibilityTest,
                       MAYBE_HardReloadModifier) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url),
      WaitForShow(kReloadButtonElementId),
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD),

      // Setup observer BEFORE click
      WithElement(kWebContentsElementId,
                  [this](ui::TrackedElement* el) {
                    reload_observer_ = std::make_unique<ReloadTypeObserver>(
                        AsInstrumentedWebContents(el)->web_contents());
                  }),

      // Shift-Click the reload button
      MoveMouseToElement(kReloadButtonElementId),
      ClickMouse(ui_controls::LEFT, true, ui_controls::kShift),

      // Wait for reload to complete
      WaitForWebContentsNavigation(kWebContentsElementId),

      // Wait for reload button to be back in Reload state
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD),

      // Verify that the reload was indeed bypassing cache
      Check([this]() {
        return reload_observer_->last_reload_type() ==
               content::ReloadType::BYPASSING_CACHE;
      }));
}

// Middle-clicking is not supported by ClickMouse on Macs
// TODO(behamilton): On Windows this test is flaky.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_MiddleClickReload DISABLED_MiddleClickReload
#else
#define MAYBE_MiddleClickReload MiddleClickReload
#endif
// Tests that middle-clicking the reload button opens the current page in a new
// background tab.
IN_PROC_BROWSER_TEST_P(ReloadButtonAccessibilityTest, MAYBE_MiddleClickReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url),
      WaitForShow(kReloadButtonElementId),

      // Ensure we have only 1 tab
      Check([&]() { return browser()->tab_strip_model()->count() == 1; }),

      // Setup state observer for tab count
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),

      // Middle-click the reload button
      MoveMouseToElement(kReloadButtonElementId),
      ClickMouse(ui_controls::MIDDLE),

      // Wait for the second tab to be opened
      WaitForState(kTabCountState, 2),

      // Verify a new tab was opened with the same URL
      Check([&]() {
        return browser()
                   ->tab_strip_model()
                   ->GetWebContentsAt(1)
                   ->GetVisibleURL() == url;
      }));
}

// Tests that the accessibility tree is correctly populated for the reload
// button.
IN_PROC_BROWSER_TEST_P(ReloadButtonAccessibilityTest, AccessibilityNode) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId), WaitForShow(kReloadButtonElementId),
      WaitForReloadButtonTooltip(IDS_TOOLTIP_RELOAD),
      CheckElement(
          kReloadButtonElementId,
          [](ui::TrackedElement* el) {
            return GetReloadAXNodeData(el, __FILE__, __LINE__).role;
          },
          ax::mojom::Role::kButton),
      CheckElement(kReloadButtonElementId,
                   [](ui::TrackedElement* el) {
                     return GetReloadAXNodeData(el, __FILE__, __LINE__)
                         .HasState(ax::mojom::State::kFocusable);
                   }),
      CheckElement(
          kReloadButtonElementId,
          [](ui::TrackedElement* el) {
            return GetReloadAXNodeData(el, __FILE__, __LINE__)
                .GetString16Attribute(ax::mojom::StringAttribute::kName);
          },
          l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD)),

      CheckElement(
          kReloadButtonElementId,
          [](ui::TrackedElement* el) {
            return GetReloadAXNodeData(el, __FILE__, __LINE__)
                .GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb);
          },
          std::to_underlying(ax::mojom::DefaultActionVerb::kPress)),
      CheckElement(
          kReloadButtonElementId,
          [](ui::TrackedElement* el) {
            return GetReloadAXNodeData(el, __FILE__, __LINE__)
                .GetString16Attribute(ax::mojom::StringAttribute::kDescription);
          },
          l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD)));
}
