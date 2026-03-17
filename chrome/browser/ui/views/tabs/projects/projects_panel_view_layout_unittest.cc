// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view_layout.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"

namespace {
constexpr int kControlsViewPreferredWidth = 100;
constexpr int kControlsViewPreferredHeight = 50;

constexpr int kTabGroupsContainerPreferredWidth = 100;
constexpr int kTabGroupsContainerPreferredHeight = 100;

constexpr int kThreadsContainerPreferredWidth = 100;
constexpr int kThreadsContainerPreferredHeight = 150;

constexpr int kSeparatorViewPreferredWidth = 100;
constexpr int kSeparatorViewPreferredHeight = 1;

constexpr int kHostViewWidth = 300;
}  // namespace

class ProjectsPanelViewLayoutTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    host_ = std::make_unique<views::View>();

    controls_view_ = host_->AddChildView(std::make_unique<views::View>());
    controls_view_->SetPreferredSize(
        gfx::Size(kControlsViewPreferredWidth, kControlsViewPreferredHeight));

    tab_groups_container_ =
        host_->AddChildView(std::make_unique<views::View>());
    tab_groups_container_->SetPreferredSize(gfx::Size(
        kTabGroupsContainerPreferredWidth, kTabGroupsContainerPreferredHeight));

    threads_container_ = host_->AddChildView(std::make_unique<views::View>());
    threads_container_->SetPreferredSize(gfx::Size(
        kThreadsContainerPreferredWidth, kThreadsContainerPreferredHeight));

    separator_view_ = host_->AddChildView(std::make_unique<views::View>());
    separator_view_->SetPreferredSize(
        gfx::Size(kSeparatorViewPreferredWidth, kSeparatorViewPreferredHeight));

    layout_ = host_->SetLayoutManager(std::make_unique<ProjectsPanelViewLayout>(
        controls_view_, tab_groups_container_, threads_container_,
        separator_view_));
  }

 protected:
  std::unique_ptr<views::View> host_;
  raw_ptr<views::View> controls_view_;
  raw_ptr<views::View> tab_groups_container_;
  raw_ptr<views::View> threads_container_;
  raw_ptr<views::View> separator_view_;
  raw_ptr<ProjectsPanelViewLayout> layout_;
};

TEST_F(ProjectsPanelViewLayoutTest, PreferredSize) {
  // controls: kControlsViewPreferredHeight
  // tab groups: kTabGroupsContainerPreferredHeight
  // separator: kSeparatorViewPreferredHeight + separator margins
  // threads: kThreadsContainerPreferredHeight
  // vertical margins: interior margins height
  const int expected_height =
      kControlsViewPreferredHeight + kTabGroupsContainerPreferredHeight +
      kSeparatorViewPreferredHeight +
      projects_panel::kListsSeparatorMargins.height() +
      kThreadsContainerPreferredHeight +
      projects_panel::kProjectsPanelRegionInteriorMargins.height();

  gfx::Size pref_size = host_->GetPreferredSize();
  EXPECT_EQ(pref_size.height(), expected_height);
  EXPECT_EQ(pref_size.width(), projects_panel::kProjectsPanelMinWidth);
}

TEST_F(ProjectsPanelViewLayoutTest, LayoutAllVisible) {
  // Set host size.
  host_->SetBounds(0, 0, kHostViewWidth, 500);
  views::test::RunScheduledLayout(host_.get());

  int expected_width =
      kHostViewWidth -
      projects_panel::kProjectsPanelRegionInteriorMargins.width();
  int x = projects_panel::kProjectsPanelRegionInteriorMargins.left();
  int y = projects_panel::kProjectsPanelRegionInteriorMargins.top();

  // Controls view.
  EXPECT_EQ(controls_view_->bounds(),
            gfx::Rect(x, y, expected_width, kControlsViewPreferredHeight));
  y += kControlsViewPreferredHeight;

  // Space for tab groups and threads.
  // fixed_height = (y + interior bottom) + kSeparatorViewPreferredHeight +
  // separator margins pref_height_tg = kTabGroupsContainerPreferredHeight
  // pref_height_th = kThreadsContainerPreferredHeight

  // The tab groups container should fill the entire width.
  EXPECT_EQ(
      tab_groups_container_->bounds(),
      gfx::Rect(0, y, kHostViewWidth, kTabGroupsContainerPreferredHeight));
  y += kTabGroupsContainerPreferredHeight;

  // Separator.
  y += projects_panel::kListsSeparatorMargins.top();
  EXPECT_EQ(separator_view_->bounds(),
            gfx::Rect(x, y, expected_width, kSeparatorViewPreferredHeight));
  y += kSeparatorViewPreferredHeight;
  y += projects_panel::kListsSeparatorMargins.bottom();

  // The threads container should fill the entire width.
  EXPECT_EQ(threads_container_->bounds(),
            gfx::Rect(0, y, kHostViewWidth, kThreadsContainerPreferredHeight));
}

TEST_F(ProjectsPanelViewLayoutTest, LimitedSpaceDistribution) {
  // Set host size small enough to trigger shrinking.
  const int fixed_height =
      kControlsViewPreferredHeight +
      projects_panel::kProjectsPanelRegionInteriorMargins.height() +
      kSeparatorViewPreferredHeight +
      projects_panel::kListsSeparatorMargins.height();
  const int available_height = 101;
  const int host_height = fixed_height + available_height;

  host_->SetBounds(0, 0, kHostViewWidth, host_height);
  views::test::RunScheduledLayout(host_.get());

  // total_pref_height = kTabGroupsContainerPreferredHeight +
  //                     kThreadsContainerPreferredHeight = 100 + 150 = 250
  // available_height = 101
  // height_overflow = 250 - 101 = 149
  // th_height is taller (150).
  // pref_height_th - height_overflow = 150 - 149 = 1
  // 1 < pref_height_tg (100), so we split space evenly.
  // tg_height = 101 / 2 = 50
  // th_height = 101 - 50 = 51

  EXPECT_EQ(tab_groups_container_->bounds().height(), 50);
  EXPECT_EQ(threads_container_->bounds().height(), 51);
  EXPECT_EQ(tab_groups_container_->bounds().width(), kHostViewWidth);
  EXPECT_EQ(threads_container_->bounds().width(), kHostViewWidth);
}

TEST_F(ProjectsPanelViewLayoutTest, HideViews) {
  separator_view_->SetVisible(false);
  threads_container_->SetVisible(false);

  host_->SetBounds(0, 0, kHostViewWidth, 500);
  views::test::RunScheduledLayout(host_.get());

  // fixed_height = (y after controls) + interior bottom
  // available_height = 500 - fixed_height
  // tg_height = available_height

  int fixed_height =
      kControlsViewPreferredHeight +
      projects_panel::kProjectsPanelRegionInteriorMargins.height();
  int expected_tg_height = 500 - fixed_height;

  EXPECT_EQ(tab_groups_container_->bounds().height(), expected_tg_height);
  EXPECT_EQ(tab_groups_container_->bounds().width(), kHostViewWidth);
  EXPECT_FALSE(threads_container_->GetVisible());
  EXPECT_FALSE(separator_view_->GetVisible());
}

TEST_F(ProjectsPanelViewLayoutTest, TallSectionShrinks) {
  // total_pref_height = 250
  // available_height = 230
  // height_overflow = 20
  // th_height: 150 - 20 = 130 > 100 (tg_height)
  // tg_height = 100, th_height = 130

  const int fixed_height =
      kControlsViewPreferredHeight +
      projects_panel::kProjectsPanelRegionInteriorMargins.height() +
      kSeparatorViewPreferredHeight +
      projects_panel::kListsSeparatorMargins.height();
  const int host_height = fixed_height + 230;

  host_->SetBounds(0, 0, kHostViewWidth, host_height);
  views::test::RunScheduledLayout(host_.get());

  EXPECT_EQ(tab_groups_container_->bounds().height(),
            kTabGroupsContainerPreferredHeight);
  EXPECT_EQ(threads_container_->bounds().height(), 130);
}
