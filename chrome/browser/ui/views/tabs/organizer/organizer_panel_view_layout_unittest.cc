// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view_layout.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"

namespace {
constexpr int kControlsViewPreferredWidth = 100;
constexpr int kControlsViewPreferredHeight = 50;
constexpr int kHostViewWidth = 300;
}  // namespace

class OrganizerPanelViewLayoutTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    host_ = std::make_unique<views::View>();

    controls_view_ = host_->AddChildView(std::make_unique<views::View>());
    controls_view_->SetPreferredSize(
        gfx::Size(kControlsViewPreferredWidth, kControlsViewPreferredHeight));

    layout_ = host_->SetLayoutManager(
        std::make_unique<OrganizerPanelViewLayout>(controls_view_));
  }

 protected:
  std::unique_ptr<views::View> host_;
  raw_ptr<views::View> controls_view_;
  raw_ptr<OrganizerPanelViewLayout> layout_;
};

TEST_F(OrganizerPanelViewLayoutTest, PreferredSize) {
  const int expected_height =
      kControlsViewPreferredHeight +
      organizer_panel::kOrganizerPanelRegionInteriorMargins.height();

  gfx::Size pref_size = host_->GetPreferredSize();
  EXPECT_EQ(pref_size.height(), expected_height);
  EXPECT_EQ(pref_size.width(), organizer_panel::kOrganizerPanelMinWidth);
}

TEST_F(OrganizerPanelViewLayoutTest, LayoutControlsView) {
  host_->SetBounds(0, 0, kHostViewWidth, 500);
  views::test::RunScheduledLayout(host_.get());

  int expected_width =
      kHostViewWidth -
      organizer_panel::kOrganizerPanelRegionInteriorMargins.width();
  int x = organizer_panel::kOrganizerPanelRegionInteriorMargins.left();
  int y = organizer_panel::kOrganizerPanelRegionInteriorMargins.top();

  // Controls view.
  EXPECT_EQ(controls_view_->bounds(),
            gfx::Rect(x, y, expected_width, kControlsViewPreferredHeight));
}
