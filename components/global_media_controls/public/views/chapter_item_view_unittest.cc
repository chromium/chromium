// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/chapter_item_view.h"

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/global_media_controls/media_view_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace global_media_controls {

namespace {

// Consts for this test.
const std::u16string kChapterTitle = u"Chapter 1";
const base::TimeDelta kChapterStartTime = base::Seconds(3661);

}  // anonymous namespace

class ChapterItemViewTest : public views::ViewsTestBase {
 public:
  // Mock callback:
  MOCK_METHOD(void, OnChapterPressed, (const base::TimeDelta time), ());

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    media_session::MediaImage test_image;
    test_image.src = GURL("https://www.google.com");
    media_session::ChapterInformation test_chapter(
        /*title=*/kChapterTitle, /*startTime=*/kChapterStartTime,
        /*artwork=*/{test_image});

    view_ = widget_->SetContentsView(std::make_unique<ChapterItemView>(
        test_chapter, media_message_center::MediaColorTheme(),
        /*on_chapter_pressed=*/
        base::BindRepeating(&ChapterItemViewTest::OnChapterPressed,
                            base::Unretained(this))));

    widget_->SetBounds(gfx::Rect(500, 500));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_->Close();

    ViewsTestBase::TearDown();
  }

  ChapterItemView* view() const { return view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ChapterItemView> view_ = nullptr;
};

TEST_F(ChapterItemViewTest, Labels) {
  EXPECT_EQ(views::AsViewClass<views::Label>(
                view()->GetViewByID(kChapterItemViewTitleId))
                ->GetText(),
            kChapterTitle);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                view()->GetViewByID(kChapterItemViewStartTimeId))
                ->GetText(),
            u"1:01:01");
}

TEST_F(ChapterItemViewTest, ClickOnView) {
  EXPECT_CALL(*this, OnChapterPressed(kChapterStartTime)).Times(1);
  views::test::ButtonTestApi(view()).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0));
  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace global_media_controls
