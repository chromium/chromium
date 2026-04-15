// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_accessibility_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
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

class ForwardButtonAccessibilityTest : public ToolbarAccessibilityTest {
 public:
  ForwardButtonAccessibilityTest() {
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

// TODO(crbug.com/500598170): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_LeftClickForward DISABLED_LeftClickForward
#else
#define MAYBE_LeftClickForward LeftClickForward
#endif
IN_PROC_BROWSER_TEST_P(ForwardButtonAccessibilityTest, MAYBE_LeftClickForward) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  WaitForElementNonzeroSize(kToolbarForwardButtonElementId),
                  NavigateWebContents(kWebContentsElementId, url1),
                  NavigateWebContents(kWebContentsElementId, url2),
                  // Go back
                  MoveMouseToElement(kToolbarBackButtonElementId), ClickMouse(),
                  WaitForWebContentsNavigation(kWebContentsElementId, url1),
                  // Click forward
                  MoveMouseToElement(kToolbarForwardButtonElementId),
                  ClickMouse(),
                  // Wait for navigation forward to url2
                  WaitForWebContentsNavigation(kWebContentsElementId, url2));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_MiddleClickForward DISABLED_MiddleClickForward
#else
#define MAYBE_MiddleClickForward MiddleClickForward
#endif
IN_PROC_BROWSER_TEST_P(ForwardButtonAccessibilityTest,
                       MAYBE_MiddleClickForward) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      WaitForElementNonzeroSize(kToolbarForwardButtonElementId),
      NavigateWebContents(kWebContentsElementId, url1),
      NavigateWebContents(kWebContentsElementId, url2),
      // Go back
      MoveMouseToElement(kToolbarBackButtonElementId), ClickMouse(),
      WaitForWebContentsNavigation(kWebContentsElementId, url1),
      Check([&]() { return browser()->tab_strip_model()->count() == 1; }),
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),
      // Middle-click forward
      MoveMouseToElement(kToolbarForwardButtonElementId),
      ClickMouse(ui_controls::MIDDLE), WaitForState(kTabCountState, 2),
      Check([&]() {
        return browser()
                   ->tab_strip_model()
                   ->GetWebContentsAt(1)
                   ->GetVisibleURL() == url2;
      }));
}

IN_PROC_BROWSER_TEST_P(ForwardButtonAccessibilityTest, ContextMenu) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  WaitForElementNonzeroSize(kToolbarForwardButtonElementId),
                  NavigateWebContents(kWebContentsElementId, url1),
                  NavigateWebContents(kWebContentsElementId, url2),
                  // Go back
                  MoveMouseToElement(kToolbarBackButtonElementId), ClickMouse(),
                  WaitForWebContentsNavigation(kWebContentsElementId, url1),
                  // Right-click to open history menu
                  MoveMouseToElement(kToolbarForwardButtonElementId),
                  MayInvolveNativeContextMenu(
                      ClickMouse(ui_controls::RIGHT),
                      // Wait for history menu and close it.
                      DismissContextMenu(kToolbarForwardButtonElementId,
                                         kToolbarForwardButtonMenuElementId)));
}

IN_PROC_BROWSER_TEST_P(ForwardButtonAccessibilityTest, AccessibilityNode) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingElementStateObserver<bool>, kForwardButtonAXNodeExists);

  const std::u16string forward_name =
      l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD);

  RunTestSequence(
      WaitForShow(kToolbarForwardButtonElementId),
      WaitForElementNonzeroSize(kToolbarForwardButtonElementId),
      // For WebUI, wait for the accessibility node to be ready.
      IfElement(
          kToolbarForwardButtonElementId,
          [](const ui::TrackedElement* el) {
            return !!el->AsA<ui::TrackedElementWebUI>();
          },
          Then(PollElement(kForwardButtonAXNodeExists,
                           kToolbarForwardButtonElementId,
                           [forward_name](const ui::TrackedElement* el) {
                             return GetAXNode(el, ax::mojom::Role::kButton,
                                              forward_name) != nullptr;
                           }),
               WaitForState(kForwardButtonAXNodeExists, true))),
      CheckElement(
          kToolbarForwardButtonElementId,
          [forward_name](ui::TrackedElement* el) {
            return GetAXNodeData(el, ax::mojom::Role::kButton, forward_name,
                                 __FILE__, __LINE__)
                .role;
          },
          ax::mojom::Role::kButton),
      CheckElement(kToolbarForwardButtonElementId, [forward_name](
                                                       ui::TrackedElement* el) {
        return GetAXNodeData(el, ax::mojom::Role::kButton, forward_name,
                             __FILE__, __LINE__)
                   .GetString16Attribute(ax::mojom::StringAttribute::kName) ==
               forward_name;
      }));
}

IN_PROC_BROWSER_TEST_P(ForwardButtonAccessibilityTest,
                       ToggleForwardButtonVisibilityWithPref) {
#if BUILDFLAG(IS_LINUX)
  // TODO(https://crbug.com/500966638): Disabled on linux due to flakiness.
  if (GetParam()) {
    GTEST_SKIP() << "Skipping /1 version on Linux due to flakiness.";
  }
#endif
  RunTestSequence(
      // Start visible
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                     true);
      }),
      WaitForShow(kToolbarForwardButtonElementId),
      // Hide it
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                     false);
      }),
      WaitForHide(kToolbarForwardButtonElementId),
      // Show it again
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                     true);
      }),
      WaitForShow(kToolbarForwardButtonElementId));
}

INSTANTIATE_TEST_SUITE_P(All, ForwardButtonAccessibilityTest, testing::Bool());
