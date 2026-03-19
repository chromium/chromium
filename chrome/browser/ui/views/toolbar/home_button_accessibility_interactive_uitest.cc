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
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
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

// For now, HomeButton is not implemented in WebUI, so we only test Views.
// TODO(crbug.com/470039765): Implement WebUI HomeButton and re-enable WebUI
// tests.
class HomeButtonAccessibilityTest : public InteractiveBrowserTest {
 public:
  HomeButtonAccessibilityTest() {
    feature_list_.InitAndDisableFeature(::features::kInitialWebUI);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForProcess(ui::kAXModeComplete);
  }

  static ui::AXNode* FindHomeButtonNode(ui::AXNode* node) {
    if (node->data().role == ax::mojom::Role::kButton &&
        node->data().GetString16Attribute(ax::mojom::StringAttribute::kName) ==
            l10n_util::GetStringUTF16(IDS_ACCNAME_HOME)) {
      return node;
    }
    for (ui::AXNode* child : node->children()) {
      if (ui::AXNode* found = FindHomeButtonNode(child)) {
        return found;
      }
    }
    return nullptr;
  }

  static ui::AXNodeData GetHomeAXNodeData(const ui::TrackedElement* el,
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
      ui::AXNode* home_button = FindHomeButtonNode(root);
      if (!home_button) {
        ADD_FAILURE_AT(file, line) << "Could not find AXNode home_button";
        return {};
      }
      return home_button->data();
    } else {
      ADD_FAILURE_AT(file, line) << "Unsupported element type";
      return {};
    }
  }

 protected:
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HomeButtonAccessibilityTest, LeftClickHome) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL home_url = GURL("chrome://newtab/");
  RunTestSequence(
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      InstrumentTab(kWebContentsElementId),
      // Navigate away from home
      NavigateWebContents(kWebContentsElementId,
                          embedded_test_server()->GetURL("/title1.html")),
      // Click home
      WaitForShow(kToolbarHomeButtonElementId),
      WaitForViewProperty(kToolbarHomeButtonElementId, views::View, Enabled,
                          true),
      MoveMouseTo(kToolbarHomeButtonElementId), ClickMouse(),
      // Wait for navigation back to home
      WaitForWebContentsNavigation(kWebContentsElementId, home_url));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_MiddleClickHome DISABLED_MiddleClickHome
#else
#define MAYBE_MiddleClickHome MiddleClickHome
#endif
IN_PROC_BROWSER_TEST_F(HomeButtonAccessibilityTest, MAYBE_MiddleClickHome) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url),
      Check([&]() { return browser()->tab_strip_model()->count() == 1; }),
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),
      WaitForShow(kToolbarHomeButtonElementId),
      // Ensure the button is enabled before clicking.
      WaitForViewProperty(kToolbarHomeButtonElementId, views::View, Enabled,
                          true),
      MoveMouseTo(kToolbarHomeButtonElementId), ClickMouse(ui_controls::MIDDLE),
      WaitForState(kTabCountState, 2));
}

IN_PROC_BROWSER_TEST_F(HomeButtonAccessibilityTest, AccessibilityNode) {
  RunTestSequence(
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      WaitForShow(kToolbarHomeButtonElementId),
      CheckElement(
          kToolbarHomeButtonElementId,
          [](ui::TrackedElement* el) {
            return GetHomeAXNodeData(el, __FILE__, __LINE__).role;
          },
          ax::mojom::Role::kButton),
      CheckElement(
          kToolbarHomeButtonElementId,
          [](ui::TrackedElement* el) {
            return GetHomeAXNodeData(el, __FILE__, __LINE__)
                .GetString16Attribute(ax::mojom::StringAttribute::kName);
          },
          l10n_util::GetStringUTF16(IDS_ACCNAME_HOME)),
      CheckElement(
          kToolbarHomeButtonElementId,
          [](ui::TrackedElement* el) {
            return GetHomeAXNodeData(el, __FILE__, __LINE__)
                .GetString16Attribute(ax::mojom::StringAttribute::kDescription);
          },
          l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME)));
}

IN_PROC_BROWSER_TEST_F(HomeButtonAccessibilityTest,
                       ToggleHomeButtonVisibilityWithPref) {
  RunTestSequence(
      // Start visible
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      WaitForShow(kToolbarHomeButtonElementId),
      // Hide it
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     false);
      }),
      WaitForHide(kToolbarHomeButtonElementId),
      // Show it again
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      WaitForShow(kToolbarHomeButtonElementId));
}
