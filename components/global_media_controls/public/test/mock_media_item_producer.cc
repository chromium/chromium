// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/test/mock_media_item_producer.h"

#include "components/media_message_center/mock_media_notification_item.h"

namespace global_media_controls {
namespace test {

struct MockMediaItemProducer::Item {
 public:
  Item(const std::string& item_id,
       bool is_active,
       bool is_frozen,
       bool is_playing)
      : id(item_id),
        active(is_active),
        frozen(is_frozen),
        playing(is_playing) {}
  Item(const Item&) = delete;
  Item& operator=(const Item&) = delete;
  ~Item() = default;

  const std::string id;
  bool active;
  bool frozen;
  bool playing;
  media_message_center::test::MockMediaNotificationItem item;
};

MockMediaItemProducer::MockMediaItemProducer() = default;

MockMediaItemProducer::~MockMediaItemProducer() = default;

void MockMediaItemProducer::AddItem(const std::string& id,
                                    bool active,
                                    bool frozen,
                                    bool playing) {
  items_.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                 std::forward_as_tuple(id, active, frozen, playing));
}

base::WeakPtr<media_message_center::MediaNotificationItem>
MockMediaItemProducer::GetMediaItem(const std::string& id) {
  auto iter = items_.find(id);
  if (iter == items_.end())
    return nullptr;

  return iter->second.item.GetWeakPtr();
}

std::set<std::string> MockMediaItemProducer::GetActiveControllableItemIds()
    const {
  std::set<std::string> active_items;
  for (auto const& item_pair : items_) {
    if (item_pair.second.active)
      active_items.insert(item_pair.second.id);
  }
  return active_items;
}

bool MockMediaItemProducer::HasFrozenItems() {
  for (auto const& item_pair : items_) {
    if (item_pair.second.frozen)
      return true;
  }
  return false;
}

bool MockMediaItemProducer::IsItemActivelyPlaying(const std::string& id) {
  auto iter = items_.find(id);
  if (iter == items_.end())
    return false;

  return iter->second.playing;
}

}  // namespace test
}  // namespace global_media_controls
