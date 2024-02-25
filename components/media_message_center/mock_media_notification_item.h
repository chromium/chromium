// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MOCK_MEDIA_NOTIFICATION_ITEM_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MOCK_MEDIA_NOTIFICATION_ITEM_H_

#include "components/media_message_center/media_notification_item.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_message_center {
namespace test {

class MockMediaNotificationItem : public MediaNotificationItem {
 public:
  MockMediaNotificationItem();
  MockMediaNotificationItem(const MockMediaNotificationItem&) = delete;
  MockMediaNotificationItem& operator=(const MockMediaNotificationItem&) =
      delete;
  ~MockMediaNotificationItem() override;

  MOCK_METHOD(void, SetView, (MediaNotificationView*));
  MOCK_METHOD(void,
              OnMediaSessionActionButtonPressed,
              (media_session::mojom::MediaSessionAction));
  MOCK_METHOD(void, SeekTo, (base::TimeDelta));
  MOCK_METHOD(void, Dismiss, ());
  MOCK_METHOD(void, SetVolume, (float));
  MOCK_METHOD(void, SetMute, (bool));
  MOCK_METHOD(bool, RequestMediaRemoting, ());
  MOCK_METHOD(media_message_center::Source, GetSource, (), (const));
  MOCK_METHOD(media_message_center::SourceType, GetSourceType, (), (const));
  MOCK_METHOD((std::optional<base::UnguessableToken>),
              GetSourceId,
              (),
              (const));

  base::WeakPtr<MockMediaNotificationItem> GetWeakPtr();

 private:
  base::WeakPtrFactory<MockMediaNotificationItem> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MOCK_MEDIA_NOTIFICATION_ITEM_H_
