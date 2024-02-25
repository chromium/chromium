// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/test/mock_media_session.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using global_media_controls::test::MockDevicePickerProvider;

class PresentationRequestNotificationItemTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PresentationRequestNotificationItem::SetMediaSessionForTest(
        &media_session_);

    provider_ = std::make_unique<MockDevicePickerProvider>();
    content::PresentationRequest request = CreatePresentationRequest();
    auto context = std::make_unique<media_router::StartPresentationContext>(
        request, base::DoNothing(), base::DoNothing());
    provider_remote_.Bind(provider_->PassRemote());
    item_ = std::make_unique<PresentationRequestNotificationItem>(
        request, std::move(context), provider_remote_);
  }

  void TearDown() override {
    item_.reset();
    provider_->FlushForTesting();
    provider_.reset();
    PresentationRequestNotificationItem::SetMediaSessionForTest(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::PresentationRequest CreatePresentationRequest() {
    return content::PresentationRequest(
        main_rfh()->GetGlobalId(), {GURL("https://receiver.example.com")},
        url::Origin::Create(GURL("https://example.com")));
  }

  std::unique_ptr<MockDevicePickerProvider> provider_;
  mojo::Remote<global_media_controls::mojom::DevicePickerProvider>
      provider_remote_;
  std::unique_ptr<PresentationRequestNotificationItem> item_;
  content::MockMediaSession media_session_;
};

TEST_F(PresentationRequestNotificationItemTest, MediaSessionMetadataChanged) {
  media_session::MediaMetadata metadata;
  metadata.source_title = u"some-other-domain.com";
  metadata.artist = u"My title";

  // The item should prioritize Media Session metadata except for
  // `source_title`, which should come from the Presentation Request.
  EXPECT_CALL(*provider_, OnMetadataChanged)
      .WillOnce(testing::Invoke(
          [metadata](const media_session::MediaMetadata& metadata_arg) {
            EXPECT_EQ(u"example.com", metadata_arg.source_title);
            EXPECT_EQ(metadata.artist, metadata_arg.artist);
          }));
  item_->MediaSessionMetadataChanged(metadata);
}

TEST_F(PresentationRequestNotificationItemTest, MediaSessionImagesChanged) {
  media_session::MediaImage image;
  image.src = GURL{"https://example.com"};
  image.sizes = {{100, 100}};

  EXPECT_CALL(*provider_, OnArtworkImageChanged);
  EXPECT_CALL(*provider_, OnFaviconImageChanged);
  item_->MediaSessionImagesChanged(
      {{media_session::mojom::MediaSessionImageType::kArtwork, {image}}});
}
