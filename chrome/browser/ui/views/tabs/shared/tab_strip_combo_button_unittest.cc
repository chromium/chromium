// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class TabStripComboButtonTest : public views::ViewsTestBase {
 public:
  TabStripComboButtonTest() = default;
  ~TabStripComboButtonTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({tab_groups::kProjectsPanel}, {});

    views::ViewsTestBase::SetUp();
    combo_button_ = std::make_unique<TabStripComboButton>(nullptr);
  }

 protected:
  std::unique_ptr<TabStripComboButton> combo_button_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TabStripComboButtonTest, UpdateStylesOnOrientationChange) {
  TabStripFlatEdgeButton* start_button = combo_button_->start_button();
  TabStripFlatEdgeButton* end_button = combo_button_->end_button();

  // Default is horizontal. Both visible.
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kRight);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kLeft);

  // Set to vertical.
  combo_button_->SetOrientation(views::LayoutOrientation::kVertical);
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kBottom);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kTop);

  // Set back to horizontal.
  combo_button_->SetOrientation(views::LayoutOrientation::kHorizontal);
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kRight);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kLeft);
}

TEST_F(TabStripComboButtonTest, UpdateStylesOnVisibilityChange) {
  TabStripFlatEdgeButton* start_button = combo_button_->start_button();
  TabStripFlatEdgeButton* end_button = combo_button_->end_button();

  // Initially both visible.
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kRight);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kLeft);

  // Hide end button.
  end_button->SetVisible(false);
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kNone);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kNone);

  // Show end button again.
  end_button->SetVisible(true);
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kRight);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kLeft);

  // Hide start button.
  start_button->SetVisible(false);
  EXPECT_EQ(start_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kNone);
  EXPECT_EQ(end_button->flat_edge_for_testing(),
            TabStripFlatEdgeButton::FlatEdge::kNone);
}
