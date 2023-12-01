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
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceWebContentsElementId);
}  // namespace

class PerformanceSidePanelInteractiveTest : public InteractiveBrowserTest {
 public:
  PerformanceSidePanelInteractiveTest() = default;
  ~PerformanceSidePanelInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::kPerformanceControlsSidePanel);
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTest::SetUp();
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
