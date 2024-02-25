// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/filtered_offline_item_observer.h"

#include "base/uuid.h"
#include "components/offline_items_collection/core/test_support/mock_filtered_offline_item_observer.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;

namespace offline_items_collection {
namespace {

TEST(FilteredOfflineItemObserverTest, TestBasicUsage) {
  ContentId id1("test", base::Uuid::GenerateRandomV4().AsLowercaseString());
  ContentId id2("test", base::Uuid::GenerateRandomV4().AsLowercaseString());
  ContentId id3("test2", id1.id);
  ContentId id4("test", base::Uuid::GenerateRandomV4().AsLowercaseString());

  OfflineItem item1(id1);
  OfflineItem item2(id2);

  MockOfflineContentProvider provider;
  FilteredOfflineItemObserver filter(&provider);

  MockFilteredOfflineItemObserver::ScopedMockObserver obs1(&filter, id1);
  MockFilteredOfflineItemObserver::ScopedMockObserver obs2(&filter, id2);
  MockFilteredOfflineItemObserver::ScopedMockObserver obs3(&filter, id3);

  EXPECT_CALL(obs2, OnItemUpdated(item2, Eq(std::nullopt))).Times(1);
  EXPECT_CALL(obs3, OnItemRemoved(id3)).Times(1);

  provider.NotifyOnItemsAdded({item1});
  provider.NotifyOnItemUpdated(item2, std::nullopt);
  provider.NotifyOnItemRemoved(id3);
  provider.NotifyOnItemRemoved(id4);
}

TEST(FilteredOfflineItemObserverTest, AddRemoveObservers) {
  ContentId id1("test", base::Uuid::GenerateRandomV4().AsLowercaseString());
  OfflineItem item1(id1);

  MockOfflineContentProvider provider;
  FilteredOfflineItemObserver filter(&provider);

  MockFilteredOfflineItemObserver::MockObserver obs1;

  {
    EXPECT_CALL(obs1, OnItemUpdated(_, _)).Times(0);
    provider.NotifyOnItemUpdated(item1, std::nullopt);
  }

  filter.AddObserver(id1, &obs1);

  {
    EXPECT_CALL(obs1, OnItemUpdated(_, _)).Times(1);
    provider.NotifyOnItemUpdated(item1, std::nullopt);
  }

  filter.RemoveObserver(id1, &obs1);

  {
    EXPECT_CALL(obs1, OnItemUpdated(_, _)).Times(0);
    provider.NotifyOnItemUpdated(item1, std::nullopt);
  }
}

}  // namespace
}  // namespace offline_items_collection
