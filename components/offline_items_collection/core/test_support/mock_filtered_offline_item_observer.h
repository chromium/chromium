// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_MOCK_FILTERED_OFFLINE_ITEM_OBSERVER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_MOCK_FILTERED_OFFLINE_ITEM_OBSERVER_H_

#include "base/macros.h"
#include "components/offline_items_collection/core/filtered_offline_item_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_items_collection {

class MockFilteredOfflineItemObserver {
 public:
  class MockObserver : public FilteredOfflineItemObserver::Observer {
   public:
    MockObserver();
    ~MockObserver() override;

    // FilteredOfflineItemObserver::Observer implementation.
    MOCK_METHOD1(OnItemRemoved, void(const ContentId&));
    MOCK_METHOD2(OnItemUpdated,
                 void(const OfflineItem&, const base::Optional<UpdateDelta>&));
  };

  class ScopedMockObserver : public MockObserver {
   public:
    ScopedMockObserver(FilteredOfflineItemObserver* observer,
                       const ContentId& id);
    ~ScopedMockObserver() override;

   private:
    ContentId id_;
    FilteredOfflineItemObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(ScopedMockObserver);
  };

 private:
  // Do not allow instantiation.
  MockFilteredOfflineItemObserver() = default;
  ~MockFilteredOfflineItemObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(MockFilteredOfflineItemObserver);
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_MOCK_FILTERED_OFFLINE_ITEM_OBSERVER_H_
