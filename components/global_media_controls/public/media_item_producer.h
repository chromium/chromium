// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_PRODUCER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_PRODUCER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace global_media_controls {

class MediaItemUI;

// Creates and owns the media items shown in the Global Media Controls. There
// are multiple MediaItemProducers for different types of media items.
class MediaItemProducer {
 public:
  virtual base::WeakPtr<media_message_center::MediaNotificationItem>
  GetMediaItem(const std::string& id) = 0;

  virtual std::set<std::string> GetActiveControllableItemIds() const = 0;

  // Returns true if the item producer has any "frozen" items, which are items
  // that were recently active with a chance to become active again.
  virtual bool HasFrozenItems() = 0;

  // Called when the item identified by |id| is inserted into a dialog along
  // with a pointer to the MediaItemUI element representing that item.
  virtual void OnItemShown(const std::string& id, MediaItemUI* item_ui) {}

  // Called when a dialog is opened with all media items. Not called when a
  // dialog is opened for a single item.
  virtual void OnDialogDisplayed() {}

  // Returns true if the item identified by |id| is actively playing.
  virtual bool IsItemActivelyPlaying(const std::string& id) = 0;

 protected:
  virtual ~MediaItemProducer() = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_PRODUCER_H_
