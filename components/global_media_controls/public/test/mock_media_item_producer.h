// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_PRODUCER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_PRODUCER_H_

#include <map>
#include <string>

#include "components/global_media_controls/public/media_item_producer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls {
namespace test {

class MockMediaItemProducer : public MediaItemProducer {
 public:
  MockMediaItemProducer();
  MockMediaItemProducer(const MockMediaItemProducer&) = delete;
  MockMediaItemProducer& operator=(const MockMediaItemProducer&) = delete;
  ~MockMediaItemProducer() override;

  void AddItem(const std::string& id, bool active, bool frozen, bool playing);

  base::WeakPtr<media_message_center::MediaNotificationItem> GetMediaItem(
      const std::string& id) override;
  std::set<std::string> GetActiveControllableItemIds() const override;
  bool HasFrozenItems() override;
  MOCK_METHOD(void, OnItemShown, (const std::string&, MediaItemUI*));
  MOCK_METHOD(void, OnDialogDisplayed, ());
  bool IsItemActivelyPlaying(const std::string& id) override;

 private:
  struct Item;

  std::map<std::string, Item> items_;
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_PRODUCER_H_
