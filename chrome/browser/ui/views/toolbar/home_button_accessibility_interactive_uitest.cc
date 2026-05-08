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

class HomeButtonAccessibilityTest : public ToolbarAccessibilityTest {
 public:
  HomeButtonAccessibilityTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIHomeButton,
           features::kWebUIReloadButton},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIHomeButton,
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

// TODO(crbug.com/506481721): Flaky time out
IN_PROC_BROWSER_TEST_P(HomeButtonAccessibilityTest, DISABLED_LeftClickHome) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingElementStateObserver<bool>, kHomeButtonAXNodeExists);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL home_url = GURL("chrome://newtab/");

  const std::u16string home_name = l10n_util::GetStringUTF16(IDS_ACCNAME_HOME);

  RunTestSequence(
      // Show the home button first to ensure it's available.
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
      WaitForElementNonzeroSize(kToolbarHomeButtonElementId),
      // For WebUI, wait for the accessibility node to be ready to ensure it's
      // interactive.
      IfElement(
          kToolbarHomeButtonElementId,
          [](const ui::TrackedElement* el) {
            return !!el->AsA<ui::TrackedElementWebUI>();
          },
          Then(PollElement(kHomeButtonAXNodeExists, kToolbarHomeButtonElementId,
                           [home_name](const ui::TrackedElement* el) {
                             return GetAXNode(el, ax::mojom::Role::kButton,
                                              home_name) != nullptr;
                           }),
               WaitForState(kHomeButtonAXNodeExists, true))),
      MoveMouseToElement(kToolbarHomeButtonElementId), ClickMouse(),
      // Wait for navigation back to home
      WaitForWebContentsNavigation(kWebContentsElementId, home_url));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_MiddleClickHome DISABLED_MiddleClickHome
#else
#define MAYBE_MiddleClickHome MiddleClickHome
#endif
IN_PROC_BROWSER_TEST_P(HomeButtonAccessibilityTest, MAYBE_MiddleClickHome) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      // Show the home button first.
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url),
      WaitForElementNonzeroSize(kToolbarHomeButtonElementId),
      Check([&]() { return browser()->tab_strip_model()->count() == 1; }),
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),
      MoveMouseToElement(kToolbarHomeButtonElementId),
      ClickMouse(ui_controls::MIDDLE), WaitForState(kTabCountState, 2));
}

IN_PROC_BROWSER_TEST_P(HomeButtonAccessibilityTest, AccessibilityNode) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingElementStateObserver<bool>, kHomeButtonAXNodeExists);

  const std::u16string home_name = l10n_util::GetStringUTF16(IDS_ACCNAME_HOME);

  RunTestSequence(
      Do([this]() {
        browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                     true);
      }),
      WaitForShow(kToolbarHomeButtonElementId),
      // For WebUI, wait for the accessibility node to be ready.
      IfElement(
          kToolbarHomeButtonElementId,
          [](const ui::TrackedElement* el) {
            return !!el->AsA<ui::TrackedElementWebUI>();
          },
          Then(PollElement(kHomeButtonAXNodeExists, kToolbarHomeButtonElementId,
                           [home_name](const ui::TrackedElement* el) {
                             return GetAXNode(el, ax::mojom::Role::kButton,
                                              home_name) != nullptr;
                           }),
               WaitForState(kHomeButtonAXNodeExists, true))),
      CheckElement(
          kToolbarHomeButtonElementId,
          [home_name](ui::TrackedElement* el) {
            return GetAXNodeData(el, ax::mojom::Role::kButton, home_name,
                                 __FILE__, __LINE__)
                .role;
          },
          ax::mojom::Role::kButton),
      CheckElement(
          kToolbarHomeButtonElementId,
          [home_name](ui::TrackedElement* el) {
            return GetAXNodeData(el, ax::mojom::Role::kButton, home_name,
                                 __FILE__, __LINE__)
                .GetString16Attribute(ax::mojom::StringAttribute::kName);
          },
          home_name),
      CheckElement(
          kToolbarHomeButtonElementId,
          [home_name](ui::TrackedElement* el) {
            return GetAXNodeData(el, ax::mojom::Role::kButton, home_name,
                                 __FILE__, __LINE__)
                .GetString16Attribute(ax::mojom::StringAttribute::kDescription);
          },
          l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME)));
}

IN_PROC_BROWSER_TEST_P(HomeButtonAccessibilityTest,
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

INSTANTIATE_TEST_SUITE_P(All, HomeButtonAccessibilityTest, testing::Bool());
