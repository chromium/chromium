// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_producer.h"

#include <set>
#include <string>

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using global_media_controls::test::MockDevicePickerObserver;

class SupplementalDevicePickerProducerTest : public testing::Test {
 public:
  // Returns the ID of the item.
  std::string ShowItem() {
    notification_producer_.CreateItem(base::UnguessableToken::Create());
    notification_producer_.ShowItem();
    const std::set<std::string> ids =
        notification_producer_.GetActiveControllableItemIds();
    EXPECT_EQ(1u, ids.size());
    return *ids.begin();
  }

 protected:
  std::unique_ptr<MockDevicePickerObserver> CreateObserver() {
    auto observer = std::make_unique<MockDevicePickerObserver>();
    provider_remote_->AddObserver(observer->PassRemote());
    provider_remote_.FlushForTesting();
    return observer;
  }

  content::BrowserTaskEnvironment task_environment_;
  global_media_controls::test::MockMediaItemManager item_manager_;
  SupplementalDevicePickerProducer notification_producer_{&item_manager_};
  mojo::Remote<global_media_controls::mojom::DevicePickerProvider>
      provider_remote_{notification_producer_.PassRemote()};
};

TEST_F(SupplementalDevicePickerProducerTest, ShowItem) {
  std::string id = ShowItem();
  EXPECT_TRUE(notification_producer_.GetMediaItem(id));
  EXPECT_FALSE(notification_producer_.HasFrozenItems());
  EXPECT_FALSE(notification_producer_.IsItemActivelyPlaying(id));
}

TEST_F(SupplementalDevicePickerProducerTest, HasActiveItemOnItemShown) {
  notification_producer_.CreateItem(base::UnguessableToken::Create());
  EXPECT_CALL(item_manager_, ShowItem).WillOnce([&]() {
    // MediaItemManager::ShowItem() leads to showing the media UI, at which
    // point `notification_producer_` should claim that it has an item to show.
    EXPECT_EQ(1u, notification_producer_.GetActiveControllableItemIds().size());
  });
  notification_producer_.ShowItem();
}

TEST_F(SupplementalDevicePickerProducerTest, HideItem) {
  std::string id = ShowItem();
  notification_producer_.HideItem();
  EXPECT_TRUE(notification_producer_.GetMediaItem(id));
  EXPECT_TRUE(notification_producer_.GetActiveControllableItemIds().empty());
}

TEST_F(SupplementalDevicePickerProducerTest, DeleteItem) {
  std::string id = ShowItem();
  notification_producer_.DeleteItem();
  EXPECT_FALSE(notification_producer_.GetMediaItem(id));
  EXPECT_TRUE(notification_producer_.GetActiveControllableItemIds().empty());
}

TEST_F(SupplementalDevicePickerProducerTest, GetOrCreateNotificationItem) {
  const SupplementalDevicePickerItem& supplemental_item =
      notification_producer_.GetOrCreateNotificationItem(
          base::UnguessableToken::Create());
  EXPECT_FALSE(supplemental_item.id().empty());
}

TEST_F(SupplementalDevicePickerProducerTest, OnMediaDialogOpened) {
  std::unique_ptr<MockDevicePickerObserver> observer = CreateObserver();
  EXPECT_CALL(*observer, OnMediaUIOpened);
  notification_producer_.OnMediaDialogOpened();
  observer->FlushForTesting();
}

TEST_F(SupplementalDevicePickerProducerTest, OnMediaDialogClosed) {
  std::unique_ptr<MockDevicePickerObserver> observer = CreateObserver();
  EXPECT_CALL(*observer, OnMediaUIClosed);
  notification_producer_.OnMediaDialogClosed();
  observer->FlushForTesting();
}

TEST_F(SupplementalDevicePickerProducerTest, OnItemListChanged) {
  std::unique_ptr<MockDevicePickerObserver> observer = CreateObserver();
  EXPECT_CALL(*observer, OnMediaUIUpdated);
  notification_producer_.OnItemListChanged();
  observer->FlushForTesting();
}

TEST_F(SupplementalDevicePickerProducerTest, OnMediaItemUIDismissed) {
  std::unique_ptr<MockDevicePickerObserver> observer = CreateObserver();
  EXPECT_CALL(*observer, OnPickerDismissed);
  notification_producer_.OnMediaItemUIDismissed(ShowItem());
  observer->FlushForTesting();
}
