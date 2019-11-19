// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/add_unique_urls_task.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const char kTestNamespace[] = "test";
const char kClientId1[] = "ID-1";
const char kClientId2[] = "ID-2";
const char kClientId3[] = "ID-3";
const char kTestThumbnailURL[] = "http://thumbnail.com/";
PrefetchURL PrefetchURL1() {
  return {kClientId1,
          GURL("https://www.url1.com/"),
          base::UTF8ToUTF16("Title 1"),
          GURL("https://www.url1.com/thumbnail.png"),
          GURL("https://www.url1.com/favicon.png"),
          "snippet 1",
          "url1.com"};
}
PrefetchURL PrefetchURL2() {
  return {kClientId2,
          GURL("https://www.url2.com/"),
          base::UTF8ToUTF16("Title 2"),
          GURL("https://www.url2.com/thumbnail.png"),
          GURL("https://www.url2.com/favicon.png"),
          "snippet 2",
          "url2.com"};
}
PrefetchURL PrefetchURL3() {
  return {kClientId3,
          GURL("https://www.url3.com/"),
          base::UTF8ToUTF16("Title 3"),
          GURL("https://www.url3.com/thumbnail.png"),
          GURL("https://www.url3.com/favicon.png"),
          "snippet 3",
          "url3.com"};
}
}  // namespace

class AddUniqueUrlsTaskTest : public PrefetchTaskTestBase {
 public:
  AddUniqueUrlsTaskTest() = default;
  ~AddUniqueUrlsTaskTest() override = default;

  // Returns all items stored in a map keyed with client id.
  std::map<std::string, PrefetchItem> GetAllItems() {
    std::set<PrefetchItem> set;
    store_util()->GetAllItems(&set);

    std::map<std::string, PrefetchItem> map;
    for (const auto& item : set)
      map[item.client_id.id] = item;
    return map;
  }

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 private:
  TestPrefetchDispatcher dispatcher_;
};


TEST_F(AddUniqueUrlsTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  RunTask(std::make_unique<AddUniqueUrlsTask>(
      dispatcher(), store(), kTestNamespace, std::vector<PrefetchURL>()));
}

TEST_F(AddUniqueUrlsTaskTest, AddTaskInEmptyStore) {
  std::vector<PrefetchURL> urls;
  PrefetchURL url1 = PrefetchURL1();
  url1.thumbnail_url = GURL(kTestThumbnailURL);
  urls.push_back(url1);
  urls.push_back(PrefetchURL2());
  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(2U, items.size());
  // Check that all fields of PrefetchURL are correct for item 1.
  ASSERT_GT(items.count(kClientId1), 0U);
  EXPECT_EQ(PrefetchURL1().url, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(PrefetchURL1().title, items[kClientId1].title);
  EXPECT_EQ(kTestThumbnailURL, items[kClientId1].thumbnail_url);
  EXPECT_EQ(PrefetchURL1().favicon_url, items[kClientId1].favicon_url);
  EXPECT_EQ(PrefetchURL1().snippet, items[kClientId1].snippet);
  EXPECT_EQ(PrefetchURL1().attribution, items[kClientId1].attribution);

  // Check that the second item exists.
  ASSERT_GT(items.count(kClientId2), 0UL);

  EXPECT_EQ(1, dispatcher()->task_schedule_count);
}

TEST_F(AddUniqueUrlsTaskTest, SingleDuplicateUrlNotAdded) {
  // Add the same URL twice in a single round. Only one entry should be added.
  PrefetchURL item1 = PrefetchURL1();
  PrefetchURL item2 = PrefetchURL2();
  item2.url = item1.url;
  const std::vector<PrefetchURL> urls = {item1, item2};
  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));
  EXPECT_EQ(1, dispatcher()->task_schedule_count);

  // AddUniqueUrlsTask with no URLs should not increment task schedule count.
  RunTask(std::make_unique<AddUniqueUrlsTask>(
      dispatcher(), store(), kTestNamespace, std::vector<PrefetchURL>()));
  // The task schedule count should not have changed with no new URLs.
  EXPECT_EQ(1, dispatcher()->task_schedule_count);

  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));
  // The task schedule count should not have changed with no new URLs.
  EXPECT_EQ(1, dispatcher()->task_schedule_count);
  EXPECT_EQ(1UL, GetAllItems().size());
}

TEST_F(AddUniqueUrlsTaskTest, DontAddURLIfItAlreadyExists) {
  // Overrides and initializes a test clock.
  TestScopedOfflineClock clock;
  const base::Time start_time = base::Time::Now();
  clock.SetNow(start_time);

  // Populate the store with pre-existing items.
  RunTask(std::make_unique<AddUniqueUrlsTask>(
      dispatcher(), store(), kTestNamespace,
      std::vector<PrefetchURL>{PrefetchURL1(), PrefetchURL2()}));
  EXPECT_EQ(1, dispatcher()->task_schedule_count);

  // Advance time by 1 hour to verify that timestamp of ID-1 is updated on the
  // next task execution.
  clock.Advance(base::TimeDelta::FromHours(1));
  const base::Time later_time = clock.Now();

  // Turn ID-1 and ID-2 items into zombies.
  // Note: ZombifyPrefetchItem returns the number of affected items.
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(kTestNamespace,
                                                  PrefetchURL1().url));
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(kTestNamespace,
                                                  PrefetchURL2().url));

  RunTask(std::make_unique<AddUniqueUrlsTask>(
      dispatcher(), store(), kTestNamespace,
      std::vector<PrefetchURL>{PrefetchURL1(), PrefetchURL3()}));
  EXPECT_EQ(2, dispatcher()->task_schedule_count);

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(3U, items.size());
  ASSERT_GT(items.count(kClientId1), 0U);

  // Re-suggested ID-1 should have its timestamp updated.
  EXPECT_EQ(PrefetchURL1().url, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(PrefetchURL1().title, items[kClientId1].title);
  EXPECT_EQ(PrefetchItemState::ZOMBIE, items[kClientId1].state);
  // Note: as timestamps are inserted with microsecond variations, we're
  // comparing them using a safe range of 1 second.
  EXPECT_LE(later_time, items[kClientId1].creation_time);
  EXPECT_GE(later_time + base::TimeDelta::FromSeconds(1),
            items[kClientId1].creation_time);
  EXPECT_LE(later_time, items[kClientId1].freshness_time);
  EXPECT_GE(later_time + base::TimeDelta::FromSeconds(1),
            items[kClientId1].freshness_time);

  // Previously existing ID-2 should not have been modified.
  ASSERT_GT(items.count(kClientId2), 0U);
  EXPECT_EQ(PrefetchURL2().url, items[kClientId2].url);
  EXPECT_EQ(kTestNamespace, items[kClientId2].client_id.name_space);
  EXPECT_EQ(PrefetchURL2().title, items[kClientId2].title);
  EXPECT_EQ(PrefetchItemState::ZOMBIE, items[kClientId2].state);
  EXPECT_LE(start_time, items[kClientId2].creation_time);
  EXPECT_GE(start_time + base::TimeDelta::FromSeconds(1),
            items[kClientId2].creation_time);
  EXPECT_LE(start_time, items[kClientId2].freshness_time);
  EXPECT_GE(start_time + base::TimeDelta::FromSeconds(1),
            items[kClientId2].freshness_time);

  // Newly suggested ID-3 should be added.
  ASSERT_GT(items.count(kClientId3), 0U);
  EXPECT_EQ(PrefetchURL3().url, items[kClientId3].url);
  EXPECT_EQ(kTestNamespace, items[kClientId3].client_id.name_space);
  EXPECT_EQ(PrefetchURL3().title, items[kClientId3].title);
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, items[kClientId3].state);
  EXPECT_LE(later_time, items[kClientId3].creation_time);
  EXPECT_GE(later_time + base::TimeDelta::FromSeconds(1),
            items[kClientId3].creation_time);
  EXPECT_LE(later_time, items[kClientId3].freshness_time);
  EXPECT_GE(later_time + base::TimeDelta::FromSeconds(1),
            items[kClientId3].freshness_time);
}

}  // namespace offline_pages
