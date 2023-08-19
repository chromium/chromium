// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/media_item_manager_impl.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/global_media_controls/public/test/mock_media_dialog_delegate.h"
#include "components/global_media_controls/public/test/mock_media_item_manager_observer.h"
#include "components/global_media_controls/public/test/mock_media_item_producer.h"
#include "components/media_message_center/media_notification_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;

namespace global_media_controls {

class MediaItemManagerImplTest : public testing::Test {
 public:
  MediaItemManagerImplTest() = default;
  ~MediaItemManagerImplTest() override = default;

  void SetUp() override {
    item_manager_ = std::make_unique<MediaItemManagerImpl>();
    item_manager_->AddObserver(&observer_);
  }

  void TearDown() override { item_manager_.reset(); }

 protected:
  void ExpectHistogramCountRecorded(int count, int size) {
    histogram_tester_.ExpectBucketCount(
        media_message_center::kCountHistogramName, count, size);
  }

  test::MockMediaItemManagerObserver& observer() { return observer_; }

  MediaItemManagerImpl* item_manager() { return item_manager_.get(); }

 private:
  NiceMock<test::MockMediaItemManagerObserver> observer_;
  std::unique_ptr<MediaItemManagerImpl> item_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MediaItemManagerImplTest, ShowsItemsFromAllProducers) {
  // Before there are any active items, the item manager should not say there
  // are active items.
  test::MockMediaItemProducer producer1;
  test::MockMediaItemProducer producer2;
  item_manager()->AddItemProducer(&producer1);
  item_manager()->AddItemProducer(&producer2);

  EXPECT_FALSE(item_manager()->HasActiveItems());

  // Once there are active items, the item manager should say so.
  producer1.AddItem("foo", true, false, false);
  producer1.AddItem("foo2", true, false, false);
  producer2.AddItem("bar", true, false, false);

  EXPECT_TRUE(item_manager()->HasActiveItems());

  // It should inform observers of this change.
  EXPECT_CALL(observer(), OnItemListChanged());
  item_manager()->OnItemsChanged();
  testing::Mock::VerifyAndClearExpectations(&observer());

  // When a dialog is opened, it should receive all the items.
  NiceMock<test::MockMediaDialogDelegate> dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo", _));
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo2", _));
  ;
  EXPECT_CALL(dialog_delegate, ShowMediaItem("bar", _));
  EXPECT_CALL(observer(), OnMediaDialogOpened());
  EXPECT_CALL(producer1, OnDialogDisplayed());
  EXPECT_CALL(producer2, OnDialogDisplayed());

  item_manager()->SetDialogDelegate(&dialog_delegate);

  // Ensure that we properly recorded the number of active sessions shown.
  ExpectHistogramCountRecorded(3, 1);

  EXPECT_CALL(observer(), OnMediaDialogClosed());
  item_manager()->SetDialogDelegate(nullptr);
}

TEST_F(MediaItemManagerImplTest, NewMediaSessionWhileDialogOpen) {
  // First, start playing active media.
  test::MockMediaItemProducer producer;
  item_manager()->AddItemProducer(&producer);
  producer.AddItem("foo", true, false, false);
  EXPECT_TRUE(item_manager()->HasActiveItems());

  // Then, open a dialog.
  NiceMock<test::MockMediaDialogDelegate> dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo", _));
  item_manager()->SetDialogDelegate(&dialog_delegate);
  ExpectHistogramCountRecorded(1, 1);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Then, have a new item start while the dialog is opened. This
  // should update the dialog.
  EXPECT_CALL(dialog_delegate, ShowMediaItem("bar", _));
  producer.AddItem("bar", true, false, false);
  item_manager()->ShowItem("bar");
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // If we close this dialog and open a new one, the new one should receive both
  // media sessions immediately.
  item_manager()->SetDialogDelegate(nullptr);
  NiceMock<test::MockMediaDialogDelegate> new_dialog;
  EXPECT_CALL(new_dialog, ShowMediaItem("foo", _));
  EXPECT_CALL(new_dialog, ShowMediaItem("bar", _));
  item_manager()->SetDialogDelegate(&new_dialog);
  ExpectHistogramCountRecorded(1, 1);
  ExpectHistogramCountRecorded(2, 1);
  item_manager()->SetDialogDelegate(nullptr);
}

TEST_F(MediaItemManagerImplTest, CanOpenDialogForSpecificItem) {
  // Set up multiple producers with multiple items.
  test::MockMediaItemProducer producer1;
  test::MockMediaItemProducer producer2;
  item_manager()->AddItemProducer(&producer1);
  item_manager()->AddItemProducer(&producer2);
  producer1.AddItem("foo", true, false, false);
  producer1.AddItem("foo2", true, false, false);
  producer2.AddItem("bar", true, false, false);
  producer2.AddItem("bar2", true, false, false);

  // If we open a dialog for a specific item, it should only receive that item.
  NiceMock<test::MockMediaDialogDelegate> dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo", _)).Times(0);
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo2", _));
  ;
  EXPECT_CALL(dialog_delegate, ShowMediaItem("bar", _)).Times(0);
  EXPECT_CALL(dialog_delegate, ShowMediaItem("bar2", _)).Times(0);

  // The producers shouldn't be informed of dialog opens for a specific item,
  // but the observer still should be.
  EXPECT_CALL(producer1, OnDialogDisplayed()).Times(0);
  EXPECT_CALL(producer2, OnDialogDisplayed()).Times(0);
  EXPECT_CALL(observer(), OnMediaDialogOpened());

  item_manager()->SetDialogDelegateForId(&dialog_delegate, "foo2");

  // We should not have recorded any histograms for item count.
  ExpectHistogramCountRecorded(1, 0);

  // If a new item becomes active while the dialog is opened for a specific
  // item, that new item should not show up in the dialog.
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo3", _)).Times(0);
  producer1.AddItem("foo3", true, false, false);
  item_manager()->ShowItem("foo3");

  item_manager()->SetDialogDelegate(nullptr);
}

TEST_F(MediaItemManagerImplTest, RefreshItems) {
  test::MockMediaItemProducer producer;
  item_manager()->AddItemProducer(&producer);
  producer.AddItem("foo", true, false, false);
  item_manager()->ShowItem("foo");

  // Then, open a dialog.
  NiceMock<test::MockMediaDialogDelegate> dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaItem("foo", _));
  item_manager()->SetDialogDelegate(&dialog_delegate);
  testing::Mock::VerifyAndClearExpectations(&dialog_delegate);

  // Refresh this item.
  EXPECT_CALL(dialog_delegate, RefreshMediaItem("foo", _));
  item_manager()->RefreshItem("foo");

  item_manager()->SetDialogDelegate(nullptr);
}

}  // namespace global_media_controls
