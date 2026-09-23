// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_test.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"

using OrganizerTrayViewTest = organizer_panel::test::OrganizerPanelTestBase;

TEST_F(OrganizerTrayViewTest, AnimatesOpen) {
  RunTestSequence(
      ShowPanel(),
      CheckView(
          kOrganizerPanelElementId,
          [](views::View* view) { return view->width(); },
          tray_view()->target_width()),
      CheckView(
          kOrganizerPanelElementId, [](views::View* view) { return view->x(); },
          0),
      CheckView(
          kOrganizerPanelElementId, [](views::View* view) { return view->y(); },
          testing::Gt(0)));
}

TEST_F(OrganizerTrayViewTest, AnimatesClosed) {
  RunTestSequence(ShowPanel(), HidePanel());
}

TEST_F(OrganizerTrayViewTest, PositionsElementsDuringAnimation) {
  RunTestSequence(
      ShowPanel(), SetAnimationValue(0.5),
      CheckView(
          kOrganizerPanelElementId,
          [](views::View* view) { return view->width(); },
          tray_view()->target_width()),
      CheckView(
          kOrganizerPanelElementId, [](views::View* view) { return view->x(); },
          testing::Lt(0)),
      CheckView(
          kOrganizerPanelElementId,
          [](views::View* view) {
            return view->bounds().right() - view->parent()->width();
          },
          0));
}

TEST_F(OrganizerTrayViewTest, CloseButtonFade) {
  RunTestSequence(
      ShowPanel(),
      SetExclusion(organizer_panel::kOrganizerPanelMinWidth / 2, 0),
      SetAnimationValue(0.25),
      CheckView(
          kOrganizerPanelCloseButtonElementId,
          [](views::View* view) { return view->layer()->opacity(); }, 0.0),
      SetAnimationValue(0.5),
      CheckView(
          kOrganizerPanelCloseButtonElementId,
          [](views::View* view) { return view->layer()->opacity(); }, 0.0),
      SetAnimationValue(0.75),
      CheckView(
          kOrganizerPanelCloseButtonElementId,
          [](views::View* view) {
            // Note: this will be less than 50% because the close button has
            // nonzero size and must fade out before it touches the exclusion
            // area.
            return view->layer()->opacity();
          },
          testing::AllOf(testing::Gt(0.0), testing::Le(0.5))),
      SetAnimationValue(1.0),
      CheckView(
          kOrganizerPanelCloseButtonElementId,
          [](views::View* view) { return view->layer()->opacity(); }, 1.0));
}

TEST_F(OrganizerTrayViewTest, SizeControlsToExclusionHeight) {
  int expected_top = 0;
  RunTestSequence(
      ShowPanel(),
      // Tall exclusion.
      SetExclusion(10, 100),
      CheckView(
          kOrganizerPanelControlsElementId,
          [](views::View* view) {
            return view->height() +
                   view->GetProperty(views::kMarginsKey)->height();
          },
          100)
          .SetDescription("Controls area should match exclusion height."),
      CheckView(
          kOrganizerPanelElementId, [](views::View* view) { return view->y(); },
          100)
          .SetDescription("Controls area should match exclusion height."),
      CheckView(
          kOrganizerPanelCloseButtonElementId,
          [](views::View* view) { return view->y(); }, testing::Gt(0))
          .SetDescription(
              "Button should float down to center in larger header."),
      // Short exclusion.
      SetExclusion(10, 1),
      CheckView(
          kOrganizerPanelControlsElementId,
          [&expected_top](views::View* view) {
            expected_top = view->bounds().bottom() +
                           view->GetProperty(views::kMarginsKey)->bottom();
            return view->height() - view->GetPreferredSize().height();
          },
          0)
          .SetDescription("For small exclusion, controls height should match "
                          "preferred height."),
      CheckView(
          kOrganizerPanelCloseButtonElementId,
          [](views::View* view) { return view->y(); }, 0)
          .SetDescription("For small exclusion, button should be top-aligned."),
      CheckView(
          kOrganizerPanelElementId,
          [&expected_top](views::View* view) {
            return view->y() - expected_top;
          },
          0)
          .SetDescription("Panel should start beneath controls."));
}

TEST_F(OrganizerTrayViewTest, ClearingPanelHandledGracefully) {
  RunTestSequence(
      ShowPanel(),
      CheckView(
          OrganizerTrayView::kTrayElementId,
          [](OrganizerTrayView* tray) {
            return OrganizerPanelHost::FromView(tray)->HasOrganizerPanelView();
          },
          true),
      Do([this]() {
        panel_ = nullptr;
        state_controller_->SetPanelViewForTesting(nullptr);
      }),
      WaitForHide(kOrganizerPanelElementId),
      CheckView(
          OrganizerTrayView::kTrayElementId,
          [](OrganizerTrayView* tray) {
            return OrganizerPanelHost::FromView(tray)->HasOrganizerPanelView();
          },
          false));
}
