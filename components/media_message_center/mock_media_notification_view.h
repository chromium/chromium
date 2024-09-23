// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MOCK_MEDIA_NOTIFICATION_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MOCK_MEDIA_NOTIFICATION_VIEW_H_

#include "components/media_message_center/media_notification_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_message_center {
namespace test {

class MockMediaNotificationView : public MediaNotificationView {
 public:
  MockMediaNotificationView();
  MockMediaNotificationView(const MockMediaNotificationView&) = delete;
  MockMediaNotificationView& operator=(const MockMediaNotificationView&) =
      delete;
  ~MockMediaNotificationView() override;

  MOCK_METHOD1(SetExpanded, void(bool));
  MOCK_METHOD2(UpdateCornerRadius, void(int, int));
  MOCK_METHOD1(SetForcedExpandedState, void(bool*));
  MOCK_METHOD1(UpdateWithMediaSessionInfo,
               void(const media_session::mojom::MediaSessionInfoPtr&));
  MOCK_METHOD1(UpdateWithMediaMetadata,
               void(const media_session::MediaMetadata&));
  MOCK_METHOD1(
      UpdateWithMediaActions,
      void(const base::flat_set<media_session::mojom::MediaSessionAction>&));
  MOCK_METHOD1(UpdateWithMediaPosition,
               void(const media_session::MediaPosition&));
  MOCK_METHOD1(UpdateWithMediaArtwork, void(const gfx::ImageSkia&));
  MOCK_METHOD2(UpdateWithChapterArtwork,
               void(int index, const gfx::ImageSkia& image));
  MOCK_METHOD1(UpdateWithFavicon, void(const gfx::ImageSkia&));
  MOCK_METHOD1(UpdateWithVectorIcon, void(const gfx::VectorIcon* vector_icon));
  MOCK_METHOD1(UpdateWithMuteStatus, void(bool));
  MOCK_METHOD1(UpdateWithVolume, void(float));
  MOCK_METHOD1(UpdateDeviceSelectorVisibility, void(bool visible));
  MOCK_METHOD1(UpdateDeviceSelectorAvailability, void(bool has_devices));
};

}  // namespace test
}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MOCK_MEDIA_NOTIFICATION_VIEW_H_
