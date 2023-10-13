// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"

#include "base/functional/callback_forward.h"
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

using offline_items_collection::ContentId;
using ::testing::NiceMock;
using ::testing::ReturnRefOfCopy;

int CountRowsWithId(const std::list<DownloadBubbleRowViewInfo>& rows,
                    const ContentId& id) {
  int count = 0;
  for (const auto& row : rows) {
    if (row.model()->GetContentId() == id) {
      ++count;
    }
  }
  return count;
}

class DownloadBubbleRowListViewInfoTest
    : public testing::Test,
      public DownloadBubbleRowListViewInfoObserver {
 public:
  void CreateItem(const std::string& id) {
    NiceMock<download::MockDownloadItem>* item = &items_[id];
    ON_CALL(*item, GetGuid()).WillByDefault(ReturnRefOfCopy(id));
    content::DownloadItemUtils::AttachInfoForTesting(item, &profile_, nullptr);
  }

  NiceMock<download::MockDownloadItem>* GetItem(const std::string& id) {
    return &items_[id];
  }

  ContentId GetContentId(const std::string& id) {
    return OfflineItemUtils::GetContentIdForDownload(&items_[id]);
  }

  void SetRowAddedCallback(
      base::OnceCallback<void(const ContentId&)> callback) {
    on_row_added_ = std::move(callback);
  }

  void SetRowWillBeRemovedCallback(
      base::OnceCallback<void(const ContentId&)> callback) {
    on_row_will_be_removed_ = std::move(callback);
  }

  void SetAnyRowRemovedCallback(base::OnceClosure callback) {
    on_any_row_removed_ = std::move(callback);
  }

 private:
  // DownloadBubbleRowListViewInfoObserver implementation:
  void OnRowAdded(const offline_items_collection::ContentId& id) override {
    if (on_row_added_) {
      std::move(on_row_added_).Run(id);
    }
  }
  void OnRowWillBeRemoved(
      const offline_items_collection::ContentId& id) override {
    if (on_row_will_be_removed_) {
      std::move(on_row_will_be_removed_).Run(id);
    }
  }
  void OnAnyRowRemoved() override {
    if (on_any_row_removed_) {
      std::move(on_any_row_removed_).Run();
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::map<std::string, NiceMock<download::MockDownloadItem>> items_;
  base::OnceCallback<void(const ContentId&)> on_row_added_;
  base::OnceCallback<void(const ContentId&)> on_row_will_be_removed_;
  base::OnceClosure on_any_row_removed_;
};

TEST_F(DownloadBubbleRowListViewInfoTest, AddRow) {
  CreateItem("item1");
  CreateItem("item2");

  ContentId id1 = GetContentId("item1");
  ContentId id2 = GetContentId("item2");

  std::vector<DownloadUIModel::DownloadUIModelPtr> models;
  models.push_back(DownloadItemModel::Wrap(GetItem("item1")));
  DownloadBubbleRowListViewInfo info(std::move(models));
  EXPECT_EQ(CountRowsWithId(info.rows(), id1), 1);
  EXPECT_EQ(info.rows().size(), 1u);

  info.AddRow(DownloadItemModel::Wrap(GetItem("item2")));
  EXPECT_EQ(CountRowsWithId(info.rows(), id2), 1);
  EXPECT_EQ(info.rows().size(), 2u);
}

TEST_F(DownloadBubbleRowListViewInfoTest, AddRowsInOrder) {
  std::vector<std::string> ids = {"item1", "item2", "item3", "item4"};
  std::vector<ContentId> content_ids;
  std::vector<DownloadUIModel::DownloadUIModelPtr> models;

  for (const auto& id : ids) {
    CreateItem(id);
    content_ids.push_back(GetContentId(id));
    models.push_back(DownloadItemModel::Wrap(GetItem(id)));
  }

  DownloadBubbleRowListViewInfo info(std::move(models));

  // Verify that all rows are present.
  EXPECT_EQ(info.rows().size(), ids.size());
  for (const auto& id : content_ids) {
    EXPECT_EQ(CountRowsWithId(info.rows(), id), 1);
  }

  // Verify order.
  int i = 0;
  for (auto it = info.rows().begin(); it != info.rows().end(); ++it) {
    EXPECT_EQ(it->model()->GetContentId(), content_ids[i]);
    ++i;
  }
}

TEST_F(DownloadBubbleRowListViewInfoTest, NotifyObserverAddRow) {
  CreateItem("item1");
  CreateItem("item2");
  ContentId id2 = GetContentId("item2");

  std::vector<DownloadUIModel::DownloadUIModelPtr> models;
  models.push_back(DownloadItemModel::Wrap(GetItem("item1")));
  DownloadBubbleRowListViewInfo info(std::move(models));
  info.AddObserver(this);

  bool notified = false;
  SetRowAddedCallback(base::BindLambdaForTesting([&](const ContentId& id) {
    EXPECT_EQ(id, id2);
    notified = true;
  }));

  info.AddRow(DownloadItemModel::Wrap(GetItem("item2")));
  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowListViewInfoTest, NotifyObserverRowWillBeRemoved) {
  CreateItem("item1");
  CreateItem("item2");
  ContentId id = GetContentId("item2");

  std::vector<DownloadUIModel::DownloadUIModelPtr> models;
  models.push_back(DownloadItemModel::Wrap(GetItem("item1")));
  models.push_back(DownloadItemModel::Wrap(GetItem("item2")));

  DownloadBubbleRowListViewInfo info(std::move(models));
  info.AddObserver(this);

  bool notified = false;
  SetRowWillBeRemovedCallback(
      base::BindLambdaForTesting([&](const ContentId& id) {
        EXPECT_EQ(CountRowsWithId(info.rows(), id), 1);
        notified = true;
      }));

  info.RemoveRow(id);

  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowListViewInfoTest, NotifyObserverAnyRowRemoved) {
  CreateItem("item1");
  CreateItem("item2");
  ContentId id = GetContentId("item2");

  std::vector<DownloadUIModel::DownloadUIModelPtr> models;
  models.push_back(DownloadItemModel::Wrap(GetItem("item1")));
  models.push_back(DownloadItemModel::Wrap(GetItem("item2")));

  DownloadBubbleRowListViewInfo info(std::move(models));
  info.AddObserver(this);

  bool notified = false;
  SetAnyRowRemovedCallback(base::BindLambdaForTesting([&]() {
    EXPECT_EQ(CountRowsWithId(info.rows(), id), 0);
    notified = true;
  }));

  info.RemoveRow(id);

  EXPECT_TRUE(notified);
}

}  // namespace
