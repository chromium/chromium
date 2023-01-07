// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/mock_media_notification_item.h"

namespace media_message_center {
namespace test {

MockMediaNotificationItem::MockMediaNotificationItem() = default;

MockMediaNotificationItem::~MockMediaNotificationItem() = default;

base::WeakPtr<MockMediaNotificationItem>
MockMediaNotificationItem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace test
}  // namespace media_message_center
