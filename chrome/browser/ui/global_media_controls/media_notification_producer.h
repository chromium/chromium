// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PRODUCER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

class MediaNotificationContainerImpl;

// Creates and owns the media notification items shown in the Global Media
// Controls. There are multiple MediaNotificationProducers for different types
// of notification items.
class MediaNotificationProducer {
 public:
  virtual base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id) = 0;

  virtual std::set<std::string> GetActiveControllableNotificationIds()
      const = 0;

  virtual void OnItemShown(const std::string& id,
                           MediaNotificationContainerImpl* container) {}
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PRODUCER_H_
