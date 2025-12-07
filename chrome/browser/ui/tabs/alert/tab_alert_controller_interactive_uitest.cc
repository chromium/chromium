// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace {
class TabAlertControllerObserver
    : public ui::test::StateObserver<std::optional<tabs::TabAlert>> {
 public:
  TabAlertControllerObserver(Browser* browser, int tab_index) {
    callback_subscription_ =
        tabs::TabAlertController::From(
            browser->tab_strip_model()->GetTabAtIndex(tab_index))
            ->AddAlertToShowChangedCallback(base::BindRepeating(
                &TabAlertControllerObserver::OnAlertToShowChanged,
                base::Unretained(this)));
  }

  void OnAlertToShowChanged(std::optional<tabs::TabAlert> alert) {
    OnStateObserverStateChanged(alert);
  }

  ~TabAlertControllerObserver() override = default;

 private:
  base::CallbackListSubscription callback_subscription_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertControllerObserver,
                                    kTab1AlertState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabAlertControllerObserver,
                                    kTab2AlertState);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

}  // namespace

class TabAlertControllerInteractiveUiTest
    : public glic::test::InteractiveGlicTest {
 public:
  TabAlertControllerInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton,
         glic::mojom::features::kGlicMultiTab},
        {});
  }
  ~TabAlertControllerInteractiveUiTest() override = default;

  void SetUp() override { glic::test::InteractiveGlicTest::SetUp(); }

  GURL GetTestUrl() const {
    return embedded_test_server()->GetURL("/links.html");
  }

  auto LoadStartingPage(ui::ElementIdentifier id,
                        std::optional<int> tab_index,
                        BrowserSpecifier in_browser) {
    return Steps(InstrumentTab(id, tab_index, in_browser),
                 NavigateWebContents(id, GetTestUrl()));
  }

 protected:
  const InteractiveBrowserTest::DeepQuery kMockGlicContextAccessButton = {
      "#contextAccessIndicator"};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabAlertControllerInteractiveUiTest,
                       TabAlertControllerAccessingSwitchTabs) {
  RunTestSequence(
      LoadStartingPage(kFirstTabId, 0, browser()),
      AddInstrumentedTab(kSecondTabId, GetTestUrl()),
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser(), 1),
      OpenGlicWindow(
          glic::test::InteractiveGlicTest::GlicWindowMode::kAttached),
      SelectTab(kTabStripElementId, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      SelectTab(kTabStripElementId, 1),
      WaitForState(kTab1AlertState, std::nullopt),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(TabAlertControllerInteractiveUiTest,
                       AlertControllerChangesOnTabMovedBetweenBrowsers) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  Browser* const browser2 = CreateBrowser(browser()->profile());
  RunTestSequence(
      LoadStartingPage(kFirstTabId, 0, browser()),
      LoadStartingPage(kSecondTabId, 0, browser2),
      OpenGlicWindow(
          glic::test::InteractiveGlicTest::GlicWindowMode::kDetached),
      ActivateSurface(kBrowserViewElementId),
      ObserveState(kTab1AlertState, browser(), 0),
      ObserveState(kTab2AlertState, browser2, 0),
      ClickMockGlicElement(kMockGlicContextAccessButton),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)),
      InContext(BrowserElements::From(browser2)->GetContext(),
                ActivateSurface(kBrowserViewElementId)),
      WaitForState(kTab1AlertState, std::nullopt),
      InContext(BrowserElements::From(browser2)->GetContext(),
                SelectTab(kTabStripElementId, 0)),
      WaitForState(kTab2AlertState,
                   std::make_optional(tabs::TabAlert::kGlicAccessing)));
}

IN_PROC_BROWSER_TEST_F(TabAlertControllerInteractiveUiTest,
                       GlicSharingUpdatesAlertController) {
  RunTestSequence(
      LoadStartingPage(kFirstTabId, 0, browser()),
      ObserveState(kTab1AlertState, browser(), 0), Do([this]() {
        TabStripModel* const tab_strip_model = browser()->GetTabStripModel();
        CHECK(glic::GlicKeyedService::Get(browser()->profile()));
        glic::GlicKeyedService::Get(browser()->profile())
            ->sharing_manager()
            .PinTabs({tab_strip_model->GetTabAtIndex(0)->GetHandle()});
      }),
      WaitForState(kTab1AlertState,
                   std::make_optional(tabs::TabAlert::kGlicSharing)),
      Do([this]() {
        glic::GlicKeyedService::Get(browser()->profile())
            ->sharing_manager()
            .UnpinAllTabs();
      }),
      WaitForState(kTab1AlertState, std::nullopt));
}
