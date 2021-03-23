// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/test_helper.h"

#include "components/media_message_center/media_notification_item.h"
#include "testing/gmock/include/gmock/gmock.h"

MockMediaNotificationItem::MockMediaNotificationItem() = default;
MockMediaNotificationItem::~MockMediaNotificationItem() = default;

base::WeakPtr<MockMediaNotificationItem>
MockMediaNotificationItem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
