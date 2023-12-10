// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceWebContentsElementId);
}  // namespace

class PerformanceSidePanelInteractiveTest : public InteractiveBrowserTest {
 public:
  PerformanceSidePanelInteractiveTest() = default;
  ~PerformanceSidePanelInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::kPerformanceControlsSidePanel);
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetHighEfficiencyModeEnabled(true);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL(base::StringPiece path) {
    return embedded_test_server()->GetURL("example.com", path);
  }

  auto TryDiscardTab(int tab_index) {
    return Do(base::BindLambdaForTesting([=]() {
      performance_manager::user_tuning::UserPerformanceTuningManager::
          GetInstance()
              ->DiscardPageForTesting(
                  browser()->tab_strip_model()->GetWebContentsAt(tab_index));
    }));
  }

  // Attempts to discard the tab at discard_tab_index and navigates to that
  // tab and waits for it to reload
  auto DiscardAndSelectTab(int discard_tab_index,
                           const ui::ElementIdentifier& contents_id) {
    return Steps(FlushEvents(),
                 // This has to be done on a fresh message loop to prevent
                 // a tab being discarded while it is notifying its observers
                 TryDiscardTab(discard_tab_index), WaitForHide(contents_id),
                 SelectTab(kTabStripElementId, discard_tab_index),
                 WaitForShow(contents_id));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       SelectPerformanceSidePanel) {
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click on the toolbar button to open the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForShow(kSidePanelElementId),
      WaitForShow(kSidePanelComboboxElementId),
      //  Switch to the performance entry using the header combobox
      WithElement(
          kSidePanelComboboxElementId,
          base::BindOnce([](ui::TrackedElement* el) {
            auto* const view = el->AsA<views::TrackedElementViews>()->view();
            auto* const combobox = views::AsViewClass<views::Combobox>(view);
            auto* const model = combobox->GetModel();

            for (int i = 0; i < static_cast<int>(model->GetItemCount()); i++) {
              if (model->GetItemAt(i) ==
                  l10n_util::GetStringUTF16(IDS_SHOW_PERFORMANCE)) {
                combobox->MenuSelectionAt(i);
                return;
              }
            }
          })),
      CheckElement(kSidePanelComboboxElementId,
                   base::BindOnce([](ui::TrackedElement* el) {
                     auto* const view =
                         el->AsA<views::TrackedElementViews>()->view();
                     auto* const combobox =
                         views::AsViewClass<views::Combobox>(view);
                     if (combobox->GetModel()->GetItemAt(
                             combobox->GetSelectedIndex().value()) !=
                         l10n_util::GetStringUTF16(IDS_SHOW_PERFORMANCE)) {
                       LOG(ERROR) << "Performance side panel is not selected.";
                       return false;
                     }
                     return true;
                   })));
}

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       OpenSidePanelFromAppMenu) {
  const DeepQuery kPathToFirstCardElement{"performance-app",
                                          ".card:nth-of-type(1)"};
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCardIsVisible);
  StateChange card_is_visible;
  card_is_visible.event = kCardIsVisible;
  card_is_visible.where = kPathToFirstCardElement;
  card_is_visible.type = StateChange::Type::kExists;

  RunTestSequence(
      Do([this]() {
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
            ->SetNoDelaysForTesting(true);
      }),
      MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
      SelectMenuItem(AppMenuModel::kPerformanceMenuItem),
      WaitForHide(AppMenuModel::kPerformanceMenuItem),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      WaitForShow(kPerformanceSidePanelWebViewElementId),
      InstrumentNonTabWebView(kPerformanceWebContentsElementId,
                              kPerformanceSidePanelWebViewElementId),
      WaitForStateChange(kPerformanceWebContentsElementId, card_is_visible),
      CheckJsResultAt(kPerformanceWebContentsElementId, kPathToFirstCardElement,
                      "el => el.tagName.toLowerCase()", "browser-health-card"));
}

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       OpenSidePanelFromMemorySaverChip) {
  const DeepQuery kPathToFirstCardElement{"performance-app",
                                          ".card:nth-of-type(1)"};
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCardIsVisible);
  StateChange card_is_visible;
  card_is_visible.event = kCardIsVisible;
  card_is_visible.where = kPathToFirstCardElement;
  card_is_visible.type = StateChange::Type::kExists;

  RunTestSequence(
      Do([this]() {
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
            ->SetNoDelaysForTesting(true);
      }),
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(kSidePanelElementId),
      WaitForShow(kPerformanceSidePanelWebViewElementId),
      InstrumentNonTabWebView(kPerformanceWebContentsElementId,
                              kPerformanceSidePanelWebViewElementId),
      WaitForStateChange(kPerformanceWebContentsElementId, card_is_visible),
      CheckJsResultAt(kPerformanceWebContentsElementId, kPathToFirstCardElement,
                      "el => el.id", "memorySaverCard"));
}
