// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_features.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
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
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);
}  // namespace

// For now, BackButton is not implemented in WebUI, so we only test Views.
// TODO(crbug.com/470038385): Implement WebUI BackButton and re-enable WebUI
// tests.
class BackButtonAccessibilityTest : public InteractiveBrowserTest {
 public:
  BackButtonAccessibilityTest() {
    feature_list_.InitAndDisableFeature(::features::kInitialWebUI);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForProcess(ui::kAXModeComplete);
  }

  static ui::AXNode* FindBackButtonNode(ui::AXNode* node) {
    if (node->data().role == ax::mojom::Role::kButton &&
        node->data().GetString16Attribute(ax::mojom::StringAttribute::kName) ==
            l10n_util::GetStringUTF16(IDS_ACCNAME_BACK)) {
      return node;
    }
    for (ui::AXNode* child : node->children()) {
      if (ui::AXNode* found = FindBackButtonNode(child)) {
        return found;
      }
    }
    return nullptr;
  }

  static ui::AXNodeData GetBackAXNodeData(const ui::TrackedElement* el,
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
      ui::AXNode* back_button = FindBackButtonNode(root);
      if (!back_button) {
        ADD_FAILURE_AT(file, line) << "Could not find AXNode back_button";
        return {};
      }
      return back_button->data();
    } else {
      ADD_FAILURE_AT(file, line) << "Unsupported element type";
      return {};
    }
  }

  auto MoveMouseToElement(ui::ElementIdentifier id) {
    return MoveMouseTo(id, base::BindOnce([](ui::TrackedElement* el) {
                         return el->GetScreenBounds().CenterPoint();
                       }));
  }

 protected:
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BackButtonAccessibilityTest, LeftClickBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, url1),
                  NavigateWebContents(kWebContentsElementId, url2),
                  // Click back
                  MoveMouseToElement(kToolbarBackButtonElementId), ClickMouse(),
                  // Wait for navigation back to url1
                  WaitForWebContentsNavigation(kWebContentsElementId, url1));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_MiddleClickBack DISABLED_MiddleClickBack
#else
#define MAYBE_MiddleClickBack MiddleClickBack
#endif
IN_PROC_BROWSER_TEST_F(BackButtonAccessibilityTest, MAYBE_MiddleClickBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url1),
      NavigateWebContents(kWebContentsElementId, url2),
      Check([&]() { return browser()->tab_strip_model()->count() == 1; }),
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),
      MoveMouseToElement(kToolbarBackButtonElementId),
      ClickMouse(ui_controls::MIDDLE), WaitForState(kTabCountState, 2),
      Check([&]() {
        return browser()
                   ->tab_strip_model()
                   ->GetWebContentsAt(1)
                   ->GetVisibleURL() == url1;
      }));
}

IN_PROC_BROWSER_TEST_F(BackButtonAccessibilityTest, ContextMenu) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, url1),
                  NavigateWebContents(kWebContentsElementId, url2),
                  // Right-click to open history menu
                  MoveMouseToElement(kToolbarBackButtonElementId),
                  ClickMouse(ui_controls::RIGHT),
                  // Wait for history menu
                  WaitForShow(kToolbarBackButtonMenuElementId));
}

IN_PROC_BROWSER_TEST_F(BackButtonAccessibilityTest, AccessibilityNode) {
  RunTestSequence(
      WaitForShow(kToolbarBackButtonElementId),
      CheckElement(
          kToolbarBackButtonElementId,
          [](ui::TrackedElement* el) {
            return GetBackAXNodeData(el, __FILE__, __LINE__).role;
          },
          ax::mojom::Role::kButton),
      CheckElement(kToolbarBackButtonElementId, [](ui::TrackedElement* el) {
        return GetBackAXNodeData(el, __FILE__, __LINE__)
                   .GetString16Attribute(ax::mojom::StringAttribute::kName) ==
               l10n_util::GetStringUTF16(IDS_ACCNAME_BACK);
      }));
}
