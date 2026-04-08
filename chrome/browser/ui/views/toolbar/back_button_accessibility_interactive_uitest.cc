// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_accessibility_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);
}  // namespace

class BackButtonAccessibilityTest : public ToolbarAccessibilityTest {
 public:
  BackButtonAccessibilityTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIBackForwardButton,
           features::kWebUIReloadButton},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIBackForwardButton,
               features::kWebUIReloadButton});
    }
  }

  void SetUpOnMainThread() override {
    ToolbarAccessibilityTest::SetUpOnMainThread();
    WaitForInitialWebUI();
    ConfigureAccessibilityForWebUITest(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(BackButtonAccessibilityTest, LeftClickBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  WaitForElementNonzeroSize(kToolbarBackButtonElementId),
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
IN_PROC_BROWSER_TEST_P(BackButtonAccessibilityTest, MAYBE_MiddleClickBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      WaitForElementNonzeroSize(kToolbarBackButtonElementId),
      NavigateWebContents(kWebContentsElementId, url1),
      NavigateWebContents(kWebContentsElementId, url2),
      Check([&]() { return browser()->tab_strip_model()->count() == 1; }),
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),
      // Middle-click back
      MoveMouseToElement(kToolbarBackButtonElementId),
      ClickMouse(ui_controls::MIDDLE), WaitForState(kTabCountState, 2),
      Check([&]() {
        return browser()
                   ->tab_strip_model()
                   ->GetWebContentsAt(1)
                   ->GetVisibleURL() == url1;
      }));
}

IN_PROC_BROWSER_TEST_P(BackButtonAccessibilityTest, ContextMenu) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  WaitForElementNonzeroSize(kToolbarBackButtonElementId),
                  NavigateWebContents(kWebContentsElementId, url1),
                  NavigateWebContents(kWebContentsElementId, url2),
                  // Right-click to open history menu
                  MoveMouseToElement(kToolbarBackButtonElementId),
                  MayInvolveNativeContextMenu(
                      ClickMouse(ui_controls::RIGHT),
                      // Wait for history menu and close it.
                      DismissContextMenu(kToolbarBackButtonElementId,
                                         kToolbarBackButtonMenuElementId)));
}

IN_PROC_BROWSER_TEST_P(BackButtonAccessibilityTest, AccessibilityNode) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingElementStateObserver<bool>, kBackButtonAXNodeExists);

  const std::u16string back_name = l10n_util::GetStringUTF16(IDS_ACCNAME_BACK);

  RunTestSequence(
      WaitForShow(kToolbarBackButtonElementId),
      // For WebUI, wait for the accessibility node to be ready.
      IfElement(
          kToolbarBackButtonElementId,
          [](const ui::TrackedElement* el) {
            return !!el->AsA<ui::TrackedElementWebUI>();
          },
          Then(PollElement(kBackButtonAXNodeExists, kToolbarBackButtonElementId,
                           [back_name](const ui::TrackedElement* el) {
                             return GetAXNode(el, ax::mojom::Role::kButton,
                                              back_name) != nullptr;
                           }),
               WaitForState(kBackButtonAXNodeExists, true))),
      CheckElement(
          kToolbarBackButtonElementId,
          [back_name](ui::TrackedElement* el) {
            return GetAXNodeData(el, ax::mojom::Role::kButton, back_name,
                                 __FILE__, __LINE__)
                .role;
          },
          ax::mojom::Role::kButton),
      CheckElement(
          kToolbarBackButtonElementId, [back_name](ui::TrackedElement* el) {
            return GetAXNodeData(el, ax::mojom::Role::kButton, back_name,
                                 __FILE__, __LINE__)
                       .GetString16Attribute(
                           ax::mojom::StringAttribute::kName) == back_name;
          }));
}

INSTANTIATE_TEST_SUITE_P(All, BackButtonAccessibilityTest, testing::Bool());
