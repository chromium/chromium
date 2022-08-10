// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockMediaSession : public content::MediaSession {
 public:
  MOCK_METHOD(void,
              DidReceiveAction,
              (media_session::mojom::MediaSessionAction action),
              (override));
  MOCK_METHOD(void,
              SetDuckingVolumeMultiplier,
              (double multiplier),
              (override));
  MOCK_METHOD(void,
              SetAudioFocusGroupId,
              (const base::UnguessableToken& group_id),
              (override));
  MOCK_METHOD(void, Suspend, (SuspendType suspend_type), (override));
  MOCK_METHOD(void, Resume, (SuspendType suspend_type), (override));
  MOCK_METHOD(void, StartDucking, (), (override));
  MOCK_METHOD(void, StopDucking, (), (override));
  MOCK_METHOD(void,
              GetMediaSessionInfo,
              (GetMediaSessionInfoCallback callback),
              (override));
  MOCK_METHOD(void, GetDebugInfo, (GetDebugInfoCallback callback), (override));
  MOCK_METHOD(void,
              AddObserver,
              (mojo::PendingRemote<media_session::mojom::MediaSessionObserver>
                   observer),
              (override));
  MOCK_METHOD(void, PreviousTrack, (), (override));
  MOCK_METHOD(void, NextTrack, (), (override));
  MOCK_METHOD(void, SkipAd, (), (override));
  MOCK_METHOD(void, Seek, (base::TimeDelta seek_time), (override));
  MOCK_METHOD(void, Stop, (SuspendType suspend_type), (override));
  MOCK_METHOD(void,
              GetMediaImageBitmap,
              (const media_session::MediaImage& image,
               int minimum_size_px,
               int desired_size_px,
               GetMediaImageBitmapCallback callback),
              (override));
  MOCK_METHOD(void, SeekTo, (base::TimeDelta seek_time), (override));
  MOCK_METHOD(void, ScrubTo, (base::TimeDelta seek_time), (override));
  MOCK_METHOD(void, EnterPictureInPicture, (), (override));
  MOCK_METHOD(void, ExitPictureInPicture, (), (override));
  MOCK_METHOD(void,
              SetAudioSinkId,
              (const absl::optional<std::string>& id),
              (override));
  MOCK_METHOD(void, ToggleMicrophone, (), (override));
  MOCK_METHOD(void, ToggleCamera, (), (override));
  MOCK_METHOD(void, HangUp, (), (override));
  MOCK_METHOD(void, Raise, (), (override));
  MOCK_METHOD(void, SetMute, (bool mute), (override));
};

class PresentationRequestNotificationItemTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PresentationRequestNotificationItemTest() = default;
  PresentationRequestNotificationItemTest(
      const PresentationRequestNotificationItemTest&) = delete;
  PresentationRequestNotificationItemTest& operator=(
      const PresentationRequestNotificationItemTest&) = delete;
  ~PresentationRequestNotificationItemTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PresentationRequestNotificationItem::SetMediaSessionForTest(
        &media_session_);
  }

  void TearDown() override {
    PresentationRequestNotificationItem::SetMediaSessionForTest(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::PresentationRequest CreatePresentationRequest() {
    return content::PresentationRequest(
        main_rfh()->GetGlobalId(), {GURL("http://presentation.com")},
        url::Origin::Create(GURL("http://google2.com")));
  }

 private:
  MockMediaSession media_session_;
};

TEST_F(PresentationRequestNotificationItemTest, NotificationHeader) {
  global_media_controls::test::MockMediaItemManager item_manager;
  auto request = CreatePresentationRequest();
  auto context = std::make_unique<media_router::StartPresentationContext>(
      request, base::DoNothing(), base::DoNothing());
  std::unique_ptr<PresentationRequestNotificationItem> item =
      std::make_unique<PresentationRequestNotificationItem>(
          &item_manager, request, std::move(context));
  testing::NiceMock<media_message_center::test::MockMediaNotificationView> view;

  const std::u16string title = u"This is the page title";
  web_contents()->UpdateTitleForEntry(controller().GetVisibleEntry(), title);
  media_session::MediaMetadata data;
  data.source_title = u"google2.com";
  data.artist = title;
  EXPECT_CALL(view, UpdateWithMediaMetadata(data));

  item->SetView(&view);
}

TEST_F(PresentationRequestNotificationItemTest,
       UsesMediaSessionMetadataWhenAvailable) {
  global_media_controls::test::MockMediaItemManager item_manager;
  auto request = CreatePresentationRequest();
  auto context = std::make_unique<media_router::StartPresentationContext>(
      request, base::DoNothing(), base::DoNothing());
  std::unique_ptr<PresentationRequestNotificationItem> item =
      std::make_unique<PresentationRequestNotificationItem>(
          &item_manager, request, std::move(context));
  testing::NiceMock<media_message_center::test::MockMediaNotificationView> view;

  // Supply Media Session metadata.
  media_session::MediaMetadata data;
  data.source_title = u"Source title";
  data.artist = u"Artist";
  item->MediaSessionMetadataChanged(data);

  // Also give the WebContents data.
  const std::u16string title = u"This is the page title";
  web_contents()->UpdateTitleForEntry(controller().GetVisibleEntry(), title);

  // The item should prioritize Media Session metadata except for
  // `source_title`, which should come from the Presentation Request.
  data.source_title = u"google2.com";
  EXPECT_CALL(view, UpdateWithMediaMetadata(data));

  item->SetView(&view);
}
