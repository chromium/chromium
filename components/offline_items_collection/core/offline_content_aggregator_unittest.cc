// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/offline_content_aggregator.h"

#include <map>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "components/offline_items_collection/core/test_support/scoped_mock_offline_content_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::Eq;
using testing::Return;

namespace offline_items_collection {
namespace {

using GetVisualsOptions = OfflineContentProvider::GetVisualsOptions;

struct CompareOfflineItemsById {
  bool operator()(const OfflineItem& a, const OfflineItem& b) const {
    return a.id < b.id;
  }
};

// A custom comparator that makes sure two vectors contain the same elements.
// TODO(dtrainor): Look into building a better matcher that works with gmock.
template <typename T>
bool VectorContentsEq(const std::vector<T>& list1,
                      const std::vector<T>& list2) {
  if (list1.size() != list2.size())
    return false;

  std::map<T, int, CompareOfflineItemsById> occurance_counts;
  for (auto it = list1.begin(); it != list1.end(); it++)
    occurance_counts[*it]++;

  for (auto it = list2.begin(); it != list2.end(); it++)
    occurance_counts[*it]--;

  for (auto it = occurance_counts.begin(); it != occurance_counts.end(); it++) {
    if (it->second != 0)
      return false;
  }

  return true;
}

MATCHER_P(OpenParamsEqual, params, "") {
  return arg.launch_location == params.launch_location;
}

// Helper class that automatically unregisters itself from the aggregator in the
// case that someone calls OpenItem on it.
class OpenItemRemovalOfflineContentProvider
    : public ScopedMockOfflineContentProvider {
 public:
  OpenItemRemovalOfflineContentProvider(const std::string& name_space,
                                        OfflineContentAggregator* aggregator)
      : ScopedMockOfflineContentProvider(name_space, aggregator) {}
  ~OpenItemRemovalOfflineContentProvider() override {}

  void OpenItem(const OpenParams& open_params, const ContentId& id) override {
    ScopedMockOfflineContentProvider::OpenItem(open_params, id);
    Unregister();
  }
};

class OfflineContentAggregatorTest : public testing::Test {
 public:
  OfflineContentAggregatorTest()
      : task_runner_(new base::TestMockTimeTaskRunner), handle_(task_runner_) {}
  ~OfflineContentAggregatorTest() override {}

 protected:
  MOCK_METHOD1(OnGetAllItemsDone,
               void(const OfflineContentProvider::OfflineItemList&));
  MOCK_METHOD1(OnGetItemByIdDone, void(const base::Optional<OfflineItem>&));

  void GetAllItemsAndVerify(
      OfflineContentProvider* provider,
      const OfflineContentProvider::OfflineItemList& expected);
  void GetSingleItemAndVerify(OfflineContentProvider* provider,
                              const ContentId& id,
                              const base::Optional<OfflineItem>& expected);

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;
  OfflineContentAggregator aggregator_;
  base::WeakPtrFactory<OfflineContentAggregatorTest> weak_ptr_factory_{this};
};

void OfflineContentAggregatorTest::GetAllItemsAndVerify(
    OfflineContentProvider* provider,
    const OfflineContentProvider::OfflineItemList& expected) {
  EXPECT_CALL(*this, OnGetAllItemsDone(expected)).Times(1);
  provider->GetAllItems(
      base::BindOnce(&OfflineContentAggregatorTest::OnGetAllItemsDone,
                     weak_ptr_factory_.GetWeakPtr()));
  task_runner_->RunUntilIdle();
}

void OfflineContentAggregatorTest::GetSingleItemAndVerify(
    OfflineContentProvider* provider,
    const ContentId& id,
    const base::Optional<OfflineItem>& expected) {
  EXPECT_CALL(*this, OnGetItemByIdDone(expected)).Times(1);
  provider->GetItemById(
      id, base::BindOnce(&OfflineContentAggregatorTest::OnGetItemByIdDone,
                         weak_ptr_factory_.GetWeakPtr()));
  task_runner_->RunUntilIdle();
}

TEST_F(OfflineContentAggregatorTest, QueryingItemsWith2Providers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  OfflineContentProvider::OfflineItemList items1;
  items1.push_back(OfflineItem(ContentId("1", "A")));
  items1.push_back(OfflineItem(ContentId("1", "B")));

  OfflineContentProvider::OfflineItemList items2;
  items2.push_back(OfflineItem(ContentId("2", "C")));
  items2.push_back(OfflineItem(ContentId("2", "D")));

  provider1.SetItems(items1);
  provider2.SetItems(items2);

  GetAllItemsAndVerify(&provider1, items1);
  GetAllItemsAndVerify(&provider2, items2);
  OfflineContentProvider::OfflineItemList combined_items(items1);
  combined_items.insert(combined_items.end(), items2.begin(), items2.end());
  GetAllItemsAndVerify(&aggregator_, combined_items);
}

TEST_F(OfflineContentAggregatorTest, QueryingItemFromRemovedProvider) {
  ContentId id("1", "A");
  OfflineItem item(id);

  {
    ScopedMockOfflineContentProvider provider("1", &aggregator_);
    provider.SetItems({item});
    GetSingleItemAndVerify(&aggregator_, id, item);
  }

  GetSingleItemAndVerify(&aggregator_, id, base::nullopt);
}

TEST_F(OfflineContentAggregatorTest, GetItemByIdPropagatesToRightProvider) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  ContentId id3("1", "C");
  ContentId id4("3", "D");
  OfflineItem item1(id1);
  OfflineItem item2(id2);

  provider1.SetItems({item1});
  provider2.SetItems({item2});
  GetSingleItemAndVerify(&aggregator_, id1, item1);
  GetSingleItemAndVerify(&aggregator_, id2, item2);
  GetSingleItemAndVerify(&aggregator_, id3, base::nullopt);
  GetSingleItemAndVerify(&aggregator_, id4, base::nullopt);
}

TEST_F(OfflineContentAggregatorTest, ActionPropagatesToRightProvider) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  testing::InSequence sequence;
  ContentId id1("1", "A");
  ContentId id2("2", "B");
  EXPECT_CALL(
      provider1,
      OpenItem(OpenParamsEqual(OpenParams(LaunchLocation::DOWNLOAD_HOME)), id1))
      .Times(1);
  EXPECT_CALL(
      provider2,
      OpenItem(OpenParamsEqual(OpenParams(LaunchLocation::NOTIFICATION)), id2))
      .Times(1);
  EXPECT_CALL(provider1, RemoveItem(id1)).Times(1);
  EXPECT_CALL(provider2, RemoveItem(id2)).Times(1);
  EXPECT_CALL(provider1, CancelDownload(id1)).Times(1);
  EXPECT_CALL(provider2, CancelDownload(id2)).Times(1);
  EXPECT_CALL(provider1, ResumeDownload(id1, false)).Times(1);
  EXPECT_CALL(provider2, ResumeDownload(id2, true)).Times(1);
  EXPECT_CALL(provider1, PauseDownload(id1)).Times(1);
  EXPECT_CALL(provider2, PauseDownload(id2)).Times(1);
  EXPECT_CALL(provider1, GetVisualsForItem_(id1, _, _)).Times(1);
  EXPECT_CALL(provider2, GetVisualsForItem_(id2, _, _)).Times(1);
  EXPECT_CALL(provider1, GetShareInfoForItem(id1, _)).Times(1);
  EXPECT_CALL(provider2, GetShareInfoForItem(id2, _)).Times(1);
  aggregator_.OpenItem(OpenParams(LaunchLocation::DOWNLOAD_HOME), id1);
  aggregator_.OpenItem(OpenParams(LaunchLocation::NOTIFICATION), id2);
  aggregator_.RemoveItem(id1);
  aggregator_.RemoveItem(id2);
  aggregator_.CancelDownload(id1);
  aggregator_.CancelDownload(id2);
  aggregator_.ResumeDownload(id1, false);
  aggregator_.ResumeDownload(id2, true);
  aggregator_.PauseDownload(id1);
  aggregator_.PauseDownload(id2);
  aggregator_.GetVisualsForItem(id1, GetVisualsOptions::IconOnly(),
                                OfflineContentProvider::VisualsCallback());
  aggregator_.GetVisualsForItem(id2, GetVisualsOptions::IconOnly(),
                                OfflineContentProvider::VisualsCallback());
  aggregator_.GetShareInfoForItem(id1, OfflineContentProvider::ShareCallback());
  aggregator_.GetShareInfoForItem(id2, OfflineContentProvider::ShareCallback());
}

TEST_F(OfflineContentAggregatorTest, ActionPropagatesImmediately) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  ContentId id3("2", "C");

  testing::InSequence sequence;
  EXPECT_CALL(provider1, PauseDownload(id1)).Times(1);
  EXPECT_CALL(provider1, ResumeDownload(id1, true)).Times(1);
  EXPECT_CALL(
      provider1,
      OpenItem(OpenParamsEqual(OpenParams(LaunchLocation::DOWNLOAD_HOME)), id1))
      .Times(1);
  EXPECT_CALL(
      provider2,
      OpenItem(OpenParamsEqual(OpenParams(LaunchLocation::NOTIFICATION)), id2))
      .Times(1);
  EXPECT_CALL(provider2, RemoveItem(id3)).Times(1);

  aggregator_.PauseDownload(id1);
  aggregator_.ResumeDownload(id1, true);
  aggregator_.OpenItem(OpenParams(LaunchLocation::DOWNLOAD_HOME), id1);
  aggregator_.OpenItem(OpenParams(LaunchLocation::NOTIFICATION), id2);
  aggregator_.RemoveItem(id3);
}

TEST_F(OfflineContentAggregatorTest, OnItemsAddedPropagatedToObservers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);

  task_runner_->RunUntilIdle();

  OfflineContentProvider::OfflineItemList items1;
  items1.push_back(OfflineItem(ContentId("1", "A")));
  items1.push_back(OfflineItem(ContentId("1", "B")));

  OfflineContentProvider::OfflineItemList items2;
  items2.push_back(OfflineItem(ContentId("2", "C")));
  items2.push_back(OfflineItem(ContentId("2", "D")));

  EXPECT_CALL(observer1, OnItemsAdded(items1)).Times(1);
  EXPECT_CALL(observer1, OnItemsAdded(items2)).Times(1);
  EXPECT_CALL(observer2, OnItemsAdded(items1)).Times(1);
  EXPECT_CALL(observer2, OnItemsAdded(items2)).Times(1);
  provider1.NotifyOnItemsAdded(items1);
  provider2.NotifyOnItemsAdded(items2);
}

TEST_F(OfflineContentAggregatorTest, OnItemRemovedPropagatedToObservers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);

  task_runner_->RunUntilIdle();

  ContentId id1("1", "A");
  ContentId id2("2", "B");

  EXPECT_CALL(observer1, OnItemRemoved(id1)).Times(1);
  EXPECT_CALL(observer1, OnItemRemoved(id2)).Times(1);
  EXPECT_CALL(observer2, OnItemRemoved(id1)).Times(1);
  EXPECT_CALL(observer2, OnItemRemoved(id2)).Times(1);
  provider1.NotifyOnItemRemoved(id1);
  provider2.NotifyOnItemRemoved(id2);
}

TEST_F(OfflineContentAggregatorTest, OnItemUpdatedPropagatedToObservers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);

  task_runner_->RunUntilIdle();

  OfflineItem item1(ContentId("1", "A"));
  OfflineItem item2(ContentId("2", "B"));

  EXPECT_CALL(observer1, OnItemUpdated(item1, Eq(base::nullopt))).Times(1);
  EXPECT_CALL(observer1, OnItemUpdated(item2, Eq(base::nullopt))).Times(1);
  EXPECT_CALL(observer2, OnItemUpdated(item1, Eq(base::nullopt))).Times(1);
  EXPECT_CALL(observer2, OnItemUpdated(item2, Eq(base::nullopt))).Times(1);
  provider1.NotifyOnItemUpdated(item1, base::nullopt);
  provider2.NotifyOnItemUpdated(item2, base::nullopt);
}

TEST_F(OfflineContentAggregatorTest, ProviderRemovedDuringCallbackFlush) {
  OpenItemRemovalOfflineContentProvider provider1("1", &aggregator_);

  ContentId id1("1", "A");
  ContentId id2("1", "B");

  EXPECT_CALL(
      provider1,
      OpenItem(OpenParamsEqual(OpenParams(LaunchLocation::DOWNLOAD_HOME)), id1))
      .Times(1);
  EXPECT_CALL(provider1, RemoveItem(id2)).Times(0);

  aggregator_.OpenItem(OpenParams(LaunchLocation::DOWNLOAD_HOME), id1);
  aggregator_.OpenItem(OpenParams(LaunchLocation::NOTIFICATION), id2);
  aggregator_.RemoveItem(id2);
}

TEST_F(OfflineContentAggregatorTest, SameProviderWithMultipleNamespaces) {
  MockOfflineContentProvider provider;
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&aggregator_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  OfflineItem item1(id1);
  OfflineItem item2(id2);
  OfflineContentProvider::OfflineItemList items;
  items.push_back(item1);
  items.push_back(item2);
  provider.SetItems(items);

  aggregator_.RegisterProvider("1", &provider);
  aggregator_.RegisterProvider("2", &provider);
  EXPECT_TRUE(provider.HasObserver(&aggregator_));

  GetAllItemsAndVerify(&aggregator_, items);
  GetSingleItemAndVerify(&aggregator_, id1, item1);
  GetSingleItemAndVerify(&aggregator_, id2, item2);

  aggregator_.UnregisterProvider("1");
  EXPECT_TRUE(provider.HasObserver(&aggregator_));
  GetSingleItemAndVerify(&aggregator_, id1, base::nullopt);
  GetSingleItemAndVerify(&aggregator_, id2, item2);

  aggregator_.UnregisterProvider("2");
  EXPECT_FALSE(provider.HasObserver(&aggregator_));
}

}  // namespace
}  // namespace offline_items_collection
