// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

#include "base/test/bind.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetOnCall(bool* to_set) {
  *to_set = true;
}

using offline_items_collection::ContentId;
using DownloadState = download::DownloadItem::DownloadState;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class DownloadBubbleRowViewInfoTest : public testing::Test,
                                      public DownloadBubbleRowViewInfoObserver {
 public:
  void CreateItem() {
    item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*item_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(std::string("id")));
    content::DownloadItemUtils::AttachInfoForTesting(item_.get(), &profile_,
                                                     nullptr);
  }

  void DestroyItem() { item_.reset(); }

  NiceMock<download::MockDownloadItem>* item() { return item_.get(); }

  ContentId content_id() {
    return OfflineItemUtils::GetContentIdForDownload(item_.get());
  }

  void SetInfoChangedCallback(base::OnceClosure callback) {
    on_info_changed_ = std::move(callback);
  }

  void SetDownloadStateChangedCallback(
      base::OnceCallback<void(DownloadState, DownloadState)> callback) {
    on_download_state_changed_ = std::move(callback);
  }

  void SetDownloadDestroyedCallback(
      base::OnceCallback<void(const ContentId&)> callback) {
    on_download_destroyed_ = std::move(callback);
  }

 private:
  // DownloadBubbleRowViewInfoObserver implementation:
  void OnInfoChanged() override {
    if (on_info_changed_) {
      std::move(on_info_changed_).Run();
    }
  }
  void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) override {
    if (on_download_destroyed_) {
      std::move(on_download_destroyed_).Run(id);
    }
  }
  void OnDownloadStateChanged(DownloadState old_state,
                              DownloadState new_state) override {
    if (on_download_state_changed_) {
      std::move(on_download_state_changed_).Run(old_state, new_state);
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NiceMock<download::MockDownloadItem>> item_;
  base::OnceClosure on_info_changed_;
  base::OnceCallback<void(const ContentId&)> on_download_destroyed_;
  base::OnceCallback<void(DownloadState, DownloadState)>
      on_download_state_changed_;
};

TEST_F(DownloadBubbleRowViewInfoTest, NotifyObserverOnUpdate) {
  CreateItem();
  DownloadBubbleRowViewInfo info(DownloadItemModel::Wrap(item()));
  info.AddObserver(this);
  bool notified = false;
  SetInfoChangedCallback(base::BindOnce(&SetOnCall, &notified));

  item()->NotifyObserversDownloadUpdated();

  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowViewInfoTest, NotifyObserverOnStateUpdate) {
  CreateItem();
  EXPECT_CALL(*item(), GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  DownloadBubbleRowViewInfo info(DownloadItemModel::Wrap(item()));
  info.AddObserver(this);
  bool notified = false;
  SetDownloadStateChangedCallback(
      base::IgnoreArgs<DownloadState, DownloadState>(
          base::BindOnce(&SetOnCall, &notified)));

  EXPECT_CALL(*item(), GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  item()->NotifyObserversDownloadUpdated();

  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowViewInfoTest, NotifyObserverOnDestroyed) {
  CreateItem();
  DownloadBubbleRowViewInfo info(DownloadItemModel::Wrap(item()));
  info.AddObserver(this);
  bool notified = false;
  ContentId expected_id = content_id();
  SetDownloadDestroyedCallback(
      base::BindLambdaForTesting([&](const ContentId& id) {
        EXPECT_EQ(id, expected_id);
        notified = true;
      }));

  DestroyItem();

  EXPECT_TRUE(notified);
}

}  // namespace
