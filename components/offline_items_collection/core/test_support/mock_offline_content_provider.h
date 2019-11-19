// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_MOCK_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_MOCK_OFFLINE_CONTENT_PROVIDER_H_

#include "base/observer_list.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_items_collection {

class MockOfflineContentProvider : public OfflineContentProvider {
 public:
  class MockObserver : public OfflineContentProvider::Observer {
   public:
    MockObserver();
    ~MockObserver() override;

    // OfflineContentProvider::Observer implementation.
    MOCK_METHOD1(OnItemsAdded, void(const OfflineItemList&));
    MOCK_METHOD1(OnItemRemoved, void(const ContentId&));
    MOCK_METHOD2(OnItemUpdated,
                 void(const OfflineItem&, const base::Optional<UpdateDelta>&));
  };

  MockOfflineContentProvider();
  ~MockOfflineContentProvider() override;

  bool HasObserver(Observer* observer);
  void SetItems(const OfflineItemList& items);
  // Sets visuals returned by |GetVisualsForItem()|. If this is not called,
  // then the mocked method |GetVisualsForItem_()| is called instead.
  void SetVisuals(std::map<ContentId, OfflineItemVisuals> visuals);
  void NotifyOnItemsAdded(const OfflineItemList& items);
  void NotifyOnItemRemoved(const ContentId& id);
  void NotifyOnItemUpdated(const OfflineItem& item,
                           const base::Optional<UpdateDelta>& update_delta);

  // OfflineContentProvider implementation.
  MOCK_METHOD2(OpenItem, void(LaunchLocation, const ContentId&));
  MOCK_METHOD1(RemoveItem, void(const ContentId&));
  MOCK_METHOD1(CancelDownload, void(const ContentId&));
  MOCK_METHOD1(PauseDownload, void(const ContentId&));
  MOCK_METHOD2(ResumeDownload, void(const ContentId&, bool));
  MOCK_METHOD3(GetVisualsForItem_,
               void(const ContentId&,
                    GetVisualsOptions,
                    const VisualsCallback&));
  void GetVisualsForItem(const ContentId& id,
                         GetVisualsOptions options,
                         VisualsCallback callback) override;
  MOCK_METHOD2(GetShareInfoForItem, void(const ContentId&, ShareCallback));
  void GetAllItems(MultipleItemCallback callback) override;
  void GetItemById(const ContentId& id, SingleItemCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  MOCK_METHOD3(RenameItem,
               void(const ContentId&, const std::string&, RenameCallback));

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  OfflineItemList items_;
  std::map<ContentId, OfflineItemVisuals> visuals_;
  bool override_visuals_ = false;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_MOCK_OFFLINE_CONTENT_PROVIDER_H_
