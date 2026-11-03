// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_test.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

using OrganizerTrayViewTest = organizer_panel::test::OrganizerPanelTestBase;

TEST_F(OrganizerTrayViewTest, AnimatesOpen) {
  RunTestSequence(ShowPanel(),
                  CheckView(
                      kOrganizerPanelElementId,
                      [](views::View* view) { return view->width(); },
                      tray_view()->target_width()),
                  CheckView(
                      kOrganizerPanelElementId,
                      [](views::View* view) { return view->x(); }, 0),
                  CheckView(
                      kOrganizerPanelElementId,
                      [](views::View* view) { return view->y(); }, 0));
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

TEST_F(OrganizerTrayViewTest, NotWindowCaption) {
  RunTestSequence(
      ShowPanel(),
      CheckView(
          kOrganizerPanelElementId,
          [this](views::View* panel) {
            gfx::Point point_in_panel(10, 10);
            gfx::Point point_in_tray = point_in_panel;
            views::View::ConvertPointToTarget(panel, tray_view(),
                                              &point_in_tray);
            return tray_view()->IsPositionInWindowCaption(point_in_tray);
          },
          false));
}
