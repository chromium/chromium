// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::ReturnPointee;

namespace {

class MockMediaToolbarButtonControllerDelegate
    : public MediaToolbarButtonControllerDelegate {
 public:
  MockMediaToolbarButtonControllerDelegate() = default;
  ~MockMediaToolbarButtonControllerDelegate() override = default;

  // MediaToolbarButtonControllerDelegate implementation.
  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, Enable, ());
  MOCK_METHOD(void, Disable, ());
  MOCK_METHOD(void, MaybeShowLocalMediaCastingPromo, ());
  MOCK_METHOD(void, MaybeShowStopCastingPromo, ());
};

}  // anonymous namespace

class MediaToolbarButtonControllerTest : public testing::Test {
 public:
  MediaToolbarButtonControllerTest() = default;
  MediaToolbarButtonControllerTest(const MediaToolbarButtonControllerTest&) =
      delete;
  MediaToolbarButtonControllerTest& operator=(
      const MediaToolbarButtonControllerTest&) = delete;
  ~MediaToolbarButtonControllerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(item_manager_, HasActiveItems())
        .WillRepeatedly(ReturnPointee(&has_active_items_));
    EXPECT_CALL(item_manager_, HasFrozenItems())
        .WillRepeatedly(ReturnPointee(&has_frozen_items_));
    EXPECT_CALL(item_manager_, HasOpenDialog())
        .WillRepeatedly(ReturnPointee(&has_open_dialog_));
    controller_ = std::make_unique<MediaToolbarButtonController>(
        &delegate_, &item_manager_);
  }

  void TearDown() override { controller_.reset(); }

 protected:
  void SetHasActiveItems(bool has_active_items) {
    has_active_items_ = has_active_items;
  }

  void SetHasFrozenItems(bool has_frozen_items) {
    has_frozen_items_ = has_frozen_items;
  }

  void SetHasOpenDialog(bool has_open_dialog) {
    has_open_dialog_ = has_open_dialog;
  }

  global_media_controls::test::MockMediaItemManager& item_manager() {
    return item_manager_;
  }

  MockMediaToolbarButtonControllerDelegate& delegate() { return delegate_; }

  MediaToolbarButtonController* controller() { return controller_.get(); }

 private:
  bool has_active_items_ = false;
  bool has_frozen_items_ = false;
  bool has_open_dialog_ = false;

  NiceMock<global_media_controls::test::MockMediaItemManager> item_manager_;
  NiceMock<MockMediaToolbarButtonControllerDelegate> delegate_;
  std::unique_ptr<MediaToolbarButtonController> controller_;
};

TEST_F(MediaToolbarButtonControllerTest, HidesAfterTimeoutAndShowsAgainOnPlay) {
  // First, show the button by playing media.
  EXPECT_CALL(delegate(), Show());
  SetHasActiveItems(true);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SetHasActiveItems(false);
  SetHasFrozenItems(true);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Once the time is elapsed, the button should be hidden.
  EXPECT_CALL(delegate(), Hide());
  SetHasFrozenItems(false);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SetHasActiveItems(true);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest, DoesNotDisableButtonIfDialogIsOpen) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  SetHasActiveItems(true);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, open a dialog.
  SetHasOpenDialog(true);
  controller()->OnMediaDialogOpened();

  // Then, have the session lose focus. This should not disable the button when
  // a dialog is present (since the button can still be used to close the
  // dialog).
  EXPECT_CALL(delegate(), Disable()).Times(0);
  SetHasActiveItems(false);
  SetHasFrozenItems(true);
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest,
       DoesNotHideIfMediaStartsPlayingWithinTimeout) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  SetHasActiveItems(true);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SetHasActiveItems(false);
  SetHasFrozenItems(true);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SetHasActiveItems(true);
  SetHasFrozenItems(false);
  controller()->OnItemListChanged();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}
