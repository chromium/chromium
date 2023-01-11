// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_SESSION_NOTIFICATION_ITEM_DELEGATE_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_SESSION_NOTIFICATION_ITEM_DELEGATE_H_

#include "components/global_media_controls/public/media_session_notification_item.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls::test {

class MockMediaSessionNotificationItemDelegate
    : public MediaSessionNotificationItem::Delegate {
 public:
  MockMediaSessionNotificationItemDelegate();
  MockMediaSessionNotificationItemDelegate(
      const MockMediaSessionNotificationItemDelegate&) = delete;
  MockMediaSessionNotificationItemDelegate& operator=(
      const MockMediaSessionNotificationItemDelegate&) = delete;
  ~MockMediaSessionNotificationItemDelegate() override;

  MOCK_METHOD(void, ActivateItem, (const std::string&));
  MOCK_METHOD(void, HideItem, (const std::string&));
  MOCK_METHOD(void, RemoveItem, (const std::string&));
  MOCK_METHOD(void, RefreshItem, (const std::string&));
  MOCK_METHOD(void,
              LogMediaSessionActionButtonPressed,
              (const std::string&, media_session::mojom::MediaSessionAction));
};
}  // namespace global_media_controls::test

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_SESSION_NOTIFICATION_ITEM_DELEGATE_H_
