// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"

#include "components/global_media_controls/public/test/mock_media_item_ui_observer.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace global_media_controls {

using ::global_media_controls::test::MockMediaItemUIObserver;
using ::media_message_center::test::MockMediaNotificationItem;
using ::testing::NiceMock;

namespace {

const char kTestId[] = "test-id";

}  // anonymous namespace

class MediaItemUIUpdatedViewTest : public views::ViewsTestBase {
 public:
  MediaItemUIUpdatedViewTest() = default;
  MediaItemUIUpdatedViewTest(const MediaItemUIUpdatedViewTest&) = delete;
  MediaItemUIUpdatedViewTest& operator=(const MediaItemUIUpdatedViewTest&) =
      delete;
  ~MediaItemUIUpdatedViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    item_ = std::make_unique<NiceMock<MockMediaNotificationItem>>();
    widget_ = CreateTestWidget();
    view_ = widget_->SetContentsView(std::make_unique<MediaItemUIUpdatedView>(
        kTestId, item_->GetWeakPtr(), media_message_center::MediaColorTheme()));

    observer_ = std::make_unique<NiceMock<MockMediaItemUIObserver>>();
    view_->AddObserver(observer_.get());
    widget_->Show();
  }

  void TearDown() override {
    view_->RemoveObserver(observer_.get());
    view_ = nullptr;
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  MediaItemUIUpdatedView* view() { return view_; }
  MockMediaNotificationItem& item() { return *item_; }
  MockMediaItemUIObserver& observer() { return *observer_; }

 private:
  raw_ptr<MediaItemUIUpdatedView> view_;
  std::unique_ptr<MockMediaNotificationItem> item_;
  std::unique_ptr<MockMediaItemUIObserver> observer_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithMediaMetadata) {
  EXPECT_EQ(view()->GetSourceLabelForTesting()->GetText(), u"");
  EXPECT_EQ(view()->GetArtistLabelForTesting()->GetText(), u"");
  EXPECT_EQ(view()->GetTitleLabelForTesting()->GetText(), u"");

  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title";
  metadata.title = u"title";
  metadata.artist = u"artist";

  EXPECT_CALL(observer(), OnMediaItemUIMetadataChanged());
  view()->UpdateWithMediaMetadata(metadata);

  EXPECT_EQ(view()->GetSourceLabelForTesting()->GetText(),
            metadata.source_title);
  EXPECT_EQ(view()->GetArtistLabelForTesting()->GetText(), metadata.artist);
  EXPECT_EQ(view()->GetTitleLabelForTesting()->GetText(), metadata.title);
}

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithMediaArtwork) {
  EXPECT_FALSE(view()->GetArtworkViewForTesting()->GetVisible());

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  view()->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(image));
  EXPECT_TRUE(view()->GetArtworkViewForTesting()->GetVisible());

  view()->UpdateWithMediaArtwork(gfx::ImageSkia());
  EXPECT_FALSE(view()->GetArtworkViewForTesting()->GetVisible());
}

}  // namespace global_media_controls
