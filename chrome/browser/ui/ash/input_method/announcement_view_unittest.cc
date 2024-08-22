// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/announcement_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/ax_event_counter.h"

namespace ui {
namespace ime {

class AnnouncementViewTest : public ChromeViewsTestBase {
 public:
  AnnouncementViewTest() {}
  ~AnnouncementViewTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    announcement_view_ = new AnnouncementView(GetContext(), u"TestView");
  }

  void TearDown() override {
    announcement_view_->GetWidget()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  raw_ptr<AnnouncementView, DanglingUntriaged> announcement_view_;
};

TEST_F(AnnouncementViewTest, MakesAnnouncement) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kLiveRegionChanged));
  announcement_view_->Announce(u"test");
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kLiveRegionChanged));
}

TEST_F(AnnouncementViewTest, HeaderAccessibilityProperties) {
  EXPECT_EQ(announcement_view_->announcement_label_->GetViewAccessibility()
                .GetCachedDescription(),
            u"");
  announcement_view_->Announce(u"test");
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(announcement_view_->announcement_label_->GetViewAccessibility()
                .GetCachedDescription(),
            u"test");
}

}  // namespace ime
}  // namespace ui
