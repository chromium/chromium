// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/all_download_event_notifier.h"

#include "base/functional/callback_helpers.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/common/mock_simple_download_manager.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace download {
namespace {

class MockNotifierObserver : public AllDownloadEventNotifier::Observer {
 public:
  MockNotifierObserver() = default;

  MockNotifierObserver(const MockNotifierObserver&) = delete;
  MockNotifierObserver& operator=(const MockNotifierObserver&) = delete;

  ~MockNotifierObserver() override = default;

  MOCK_METHOD2(OnDownloadsInitialized,
               void(SimpleDownloadManagerCoordinator* manager,
                    bool active_downloads_only));
  MOCK_METHOD2(OnDownloadCreated,
               void(SimpleDownloadManagerCoordinator* manager,
                    DownloadItem* item));
  MOCK_METHOD2(OnDownloadUpdated,
               void(SimpleDownloadManagerCoordinator* manager,
                    DownloadItem* item));
  MOCK_METHOD2(OnDownloadOpened,
               void(SimpleDownloadManagerCoordinator* manager,
                    DownloadItem* item));
  MOCK_METHOD2(OnDownloadRemoved,
               void(SimpleDownloadManagerCoordinator* manager,
                    DownloadItem* item));
};

class AllDownloadEventNotifierTest : public testing::Test {
 public:
  AllDownloadEventNotifierTest() : coordinator_(base::NullCallback()) {}

  AllDownloadEventNotifierTest(const AllDownloadEventNotifierTest&) = delete;
  AllDownloadEventNotifierTest& operator=(const AllDownloadEventNotifierTest&) =
      delete;

  ~AllDownloadEventNotifierTest() override = default;

  SimpleDownloadManagerCoordinator* coordinator() { return &coordinator_; }

  MockDownloadItem& item() { return item_; }

  MockNotifierObserver& observer() { return observer_; }

 private:
  NiceMock<MockDownloadItem> item_;
  NiceMock<MockNotifierObserver> observer_;
  SimpleDownloadManagerCoordinator coordinator_;
};

}  // namespace

// Tests that OnDownloadCreated() will be propagated correctly.
TEST_F(AllDownloadEventNotifierTest, OnDownloadCreated) {
  NiceMock<MockSimpleDownloadManager> manager;
  coordinator()->SetSimpleDownloadManager(&manager, true);
  coordinator()->GetNotifier()->AddObserver(&observer());
  EXPECT_CALL(observer(), OnDownloadCreated(coordinator(), &item()));
  manager.NotifyOnNewDownloadCreated(&item());
}

// Tests that OnDownloadsInitialized() will be propated correctly.
TEST_F(AllDownloadEventNotifierTest, OnDownloadsInitialized) {
  NiceMock<MockSimpleDownloadManager> manager;
  coordinator()->SetSimpleDownloadManager(&manager, false);
  coordinator()->GetNotifier()->AddObserver(&observer());
  EXPECT_CALL(observer(), OnDownloadsInitialized(coordinator(), true));
  manager.NotifyOnDownloadInitialized();

  NiceMock<MockSimpleDownloadManager> manager2;
  coordinator()->SetSimpleDownloadManager(&manager2, true);
  EXPECT_CALL(observer(), OnDownloadsInitialized(coordinator(), false));
  manager2.NotifyOnDownloadInitialized();
}

// Tests that the full manaager is set before the in progress manager
// initializes.
TEST_F(AllDownloadEventNotifierTest,
       SetFullManagerBeforeInProgressManagerInitializes) {
  NiceMock<MockSimpleDownloadManager> manager;
  coordinator()->SetSimpleDownloadManager(&manager, false);
  coordinator()->GetNotifier()->AddObserver(&observer());

  // Sets the full manager before the first one is initialized.
  NiceMock<MockSimpleDownloadManager> manager2;
  coordinator()->SetSimpleDownloadManager(&manager2, true);

  EXPECT_CALL(observer(), OnDownloadsInitialized(coordinator(), _)).Times(0);
  // Initializing the first manager shouldn't impact the coordinator now.
  manager.NotifyOnDownloadInitialized();
  EXPECT_CALL(observer(), OnDownloadsInitialized(coordinator(), false));
  manager2.NotifyOnDownloadInitialized();
}

// Tests that OnDownloadUpdated() will be propagated correctly.
TEST_F(AllDownloadEventNotifierTest, OnDownloadUpdated) {
  NiceMock<MockSimpleDownloadManager> manager;
  coordinator()->SetSimpleDownloadManager(&manager, true);
  coordinator()->GetNotifier()->AddObserver(&observer());
  EXPECT_CALL(observer(), OnDownloadCreated(coordinator(), &item()));
  manager.NotifyOnNewDownloadCreated(&item());

  EXPECT_CALL(observer(), OnDownloadUpdated(coordinator(), &item()));
  item().NotifyObserversDownloadUpdated();
}

// Tests that observer will be notified if it is added after manager
// initialization.
TEST_F(AllDownloadEventNotifierTest, AddObserverAfterManagerInitialization) {
  NiceMock<MockSimpleDownloadManager> manager;
  coordinator()->SetSimpleDownloadManager(&manager, true);
  manager.NotifyOnDownloadInitialized();
  EXPECT_CALL(observer(), OnDownloadsInitialized(coordinator(), false));
  coordinator()->GetNotifier()->AddObserver(&observer());
}

// Tests that observer will only be notified once if multiple manager calls
// OnDownloadCreated() on the same download.
TEST_F(AllDownloadEventNotifierTest, MultipleOnDownloadCreatedOnSameDownload) {
  NiceMock<MockSimpleDownloadManager> manager;
  coordinator()->GetNotifier()->AddObserver(&observer());
  coordinator()->SetSimpleDownloadManager(&manager, false);
  EXPECT_CALL(observer(), OnDownloadCreated(coordinator(), &item())).Times(1);
  manager.NotifyOnNewDownloadCreated(&item());

  // Sets the full manager and call OnDownloadCreated() again.
  NiceMock<MockSimpleDownloadManager> manager2;
  coordinator()->SetSimpleDownloadManager(&manager2, true);
  manager2.NotifyOnNewDownloadCreated(&item());

  EXPECT_CALL(observer(), OnDownloadUpdated(coordinator(), &item()));
  item().NotifyObserversDownloadUpdated();
}

}  // namespace download
