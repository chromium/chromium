// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_test.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/view.h"

namespace {

class TestOrganizerPanelView : public OrganizerPanelView {
 public:
  explicit TestOrganizerPanelView(BrowserWindowInterface& browser)
      : OrganizerPanelView(browser) {}
  ~TestOrganizerPanelView() override = default;

  void ClearFocusedElement() override { clear_focused_element_called_ = true; }

  bool clear_focused_element_called() const {
    return clear_focused_element_called_;
  }
  void reset_clear_focused_element_called() {
    clear_focused_element_called_ = false;
  }

 private:
  bool clear_focused_element_called_ = false;
};

}  // namespace

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

TEST_F(OrganizerTrayViewTest, NotWindowCaption) {
  RunTestSequence(
      ShowPanel(),
      CheckView(
          kOrganizerPanelControlsElementId,
          [this](views::View* controls) {
            gfx::Point point_in_controls(10, 10);
            gfx::Point point_in_tray = point_in_controls;
            views::View::ConvertPointToTarget(controls, tray_view(),
                                              &point_in_tray);
            return tray_view()->IsPositionInWindowCaption(point_in_tray);
          },
          false),
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

TEST_F(OrganizerTrayViewTest, ClickAbovePanelClearsFocusedElement) {
  TestOrganizerPanelView* test_panel = nullptr;
  views::View* focused_child = nullptr;
  RunTestSequence(
      Do([this, &test_panel]() {
        auto panel = std::make_unique<TestOrganizerPanelView>(browser_);
        test_panel = panel.get();
        panel_ = test_panel;
        state_controller_->SetPanelViewForTesting(std::move(panel));
      }),
      ShowPanel(), Do([this, &test_panel, &focused_child]() {
        auto child = std::make_unique<views::View>();
        child->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
        focused_child = panel_->AddChildView(std::move(child));
        focused_child->RequestFocus();
        EXPECT_EQ(tray_view()->GetFocusManager()->GetFocusedView(),
                  focused_child);

        // Click above the panel in the tray (e.g. at (10, 10)).
        const gfx::Point point_above(10, 10);
        ui::MouseEvent press_above(ui::EventType::kMousePressed, point_above,
                                   point_above, ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);
        EXPECT_TRUE(tray_view()->OnMousePressed(press_above));
        EXPECT_TRUE(test_panel->clear_focused_element_called());
        // Verify native Views focus was preserved.
        EXPECT_EQ(tray_view()->GetFocusManager()->GetFocusedView(),
                  focused_child);

        test_panel->reset_clear_focused_element_called();

        // Re-focus the child and verify that a right click above the panel does
        // not clear focused element and is not handled.
        focused_child->RequestFocus();
        EXPECT_EQ(tray_view()->GetFocusManager()->GetFocusedView(),
                  focused_child);

        ui::MouseEvent right_click(ui::EventType::kMousePressed, point_above,
                                   point_above, ui::EventTimeForNow(),
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON);
        EXPECT_FALSE(tray_view()->OnMousePressed(right_click));
        EXPECT_FALSE(test_panel->clear_focused_element_called());
        EXPECT_EQ(tray_view()->GetFocusManager()->GetFocusedView(),
                  focused_child);

        // Verify that a click within the panel does not clear focused element
        // and is not handled.
        focused_child->RequestFocus();
        EXPECT_EQ(tray_view()->GetFocusManager()->GetFocusedView(),
                  focused_child);

        const gfx::Point point_in_panel = panel_->bounds().CenterPoint();
        ui::MouseEvent press_in_panel(
            ui::EventType::kMousePressed, point_in_panel, point_in_panel,
            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
            ui::EF_LEFT_MOUSE_BUTTON);
        EXPECT_FALSE(tray_view()->OnMousePressed(press_in_panel));
        EXPECT_FALSE(test_panel->clear_focused_element_called());
        EXPECT_EQ(tray_view()->GetFocusManager()->GetFocusedView(),
                  focused_child);
      }));
}
