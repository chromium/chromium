// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_

#include "components/media_message_center/media_notification_item.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockMediaNotificationItem
    : public media_message_center::MediaNotificationItem {
 public:
  MockMediaNotificationItem();
  ~MockMediaNotificationItem() final;

  base::WeakPtr<MockMediaNotificationItem> GetWeakPtr();

  MOCK_METHOD(void, SetView, (media_message_center::MediaNotificationView*));
  MOCK_METHOD(void,
              OnMediaSessionActionButtonPressed,
              (media_session::mojom::MediaSessionAction));
  MOCK_METHOD(void, Dismiss, ());
  MOCK_METHOD(media_message_center::SourceType, SourceType, ());

 private:
  base::WeakPtrFactory<MockMediaNotificationItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
