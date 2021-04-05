// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/public/all_download_item_notifier.h"

#include <memory>

#include "base/macros.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::SetArgPointee;
using testing::_;

namespace download {
namespace {

class MockNotifierObserver : public AllDownloadItemNotifier::Observer {
 public:
  MockNotifierObserver() {}
  ~MockNotifierObserver() override {}

  MOCK_METHOD2(OnDownloadCreated,
               void(content::DownloadManager* manager, DownloadItem* item));
  MOCK_METHOD2(OnDownloadUpdated,
               void(content::DownloadManager* manager, DownloadItem* item));
  MOCK_METHOD2(OnDownloadOpened,
               void(content::DownloadManager* manager, DownloadItem* item));
  MOCK_METHOD2(OnDownloadRemoved,
               void(content::DownloadManager* manager, DownloadItem* item));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNotifierObserver);
};

class AllDownloadItemNotifierTest : public testing::Test {
 public:
  AllDownloadItemNotifierTest()
      : download_manager_(new content::MockDownloadManager) {}

  ~AllDownloadItemNotifierTest() override {}

  content::MockDownloadManager& manager() { return *download_manager_; }

  download::MockDownloadItem& item() { return item_; }

  DownloadItem::Observer* NotifierAsItemObserver() const {
    return notifier_.get();
  }

  content::DownloadManager::Observer* NotifierAsManagerObserver() const {
    return notifier_.get();
  }

  MockNotifierObserver& observer() { return observer_; }

  void SetNotifier() {
    EXPECT_CALL(*download_manager_, AddObserver(_));
    notifier_ = std::make_unique<AllDownloadItemNotifier>(
        download_manager_.get(), &observer_);
  }

  void ClearNotifier() { notifier_.reset(); }

 private:
  NiceMock<download::MockDownloadItem> item_;
  std::unique_ptr<content::MockDownloadManager> download_manager_;
  std::unique_ptr<AllDownloadItemNotifier> notifier_;
  NiceMock<MockNotifierObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(AllDownloadItemNotifierTest);
};

}  // namespace

TEST_F(AllDownloadItemNotifierTest, AllDownloadItemNotifierTest_0) {
  content::DownloadManager::DownloadVector items;
  items.push_back(&item());
  EXPECT_CALL(manager(), GetAllDownloads(_)).WillOnce(SetArgPointee<0>(items));
  SetNotifier();

  EXPECT_CALL(observer(), OnDownloadUpdated(&manager(), &item()));
  NotifierAsItemObserver()->OnDownloadUpdated(&item());

  EXPECT_CALL(observer(), OnDownloadOpened(&manager(), &item()));
  NotifierAsItemObserver()->OnDownloadOpened(&item());

  EXPECT_CALL(observer(), OnDownloadRemoved(&manager(), &item()));
  NotifierAsItemObserver()->OnDownloadRemoved(&item());

  EXPECT_CALL(manager(), RemoveObserver(NotifierAsManagerObserver()));
  ClearNotifier();
}

TEST_F(AllDownloadItemNotifierTest, AllDownloadItemNotifierTest_1) {
  EXPECT_CALL(manager(), GetAllDownloads(_));
  SetNotifier();

  EXPECT_CALL(observer(), OnDownloadCreated(&manager(), &item()));
  NotifierAsManagerObserver()->OnDownloadCreated(&manager(), &item());

  EXPECT_CALL(manager(), RemoveObserver(NotifierAsManagerObserver()));
  NotifierAsManagerObserver()->ManagerGoingDown(&manager());

  ClearNotifier();
}

}  // namespace download
