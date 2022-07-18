// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class PresentationRequestNotificationItemTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PresentationRequestNotificationItemTest() = default;
  PresentationRequestNotificationItemTest(
      const PresentationRequestNotificationItemTest&) = delete;
  PresentationRequestNotificationItemTest& operator=(
      const PresentationRequestNotificationItemTest&) = delete;
  ~PresentationRequestNotificationItemTest() override = default;

  content::PresentationRequest CreatePresentationRequest() {
    return content::PresentationRequest(
        main_rfh()->GetGlobalId(), {GURL("http://presentation.com")},
        url::Origin::Create(GURL("http://google2.com")));
  }
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

  // The item should prioritize Media Session metadata.
  EXPECT_CALL(view, UpdateWithMediaMetadata(data));

  item->SetView(&view);
}
