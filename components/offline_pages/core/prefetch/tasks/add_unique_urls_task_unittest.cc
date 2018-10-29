// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/add_unique_urls_task.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const char kTestNamespace[] = "test";
const char kClientId1[] = "ID-1";
const char kClientId2[] = "ID-2";
const char kClientId3[] = "ID-3";
const char kClientId4[] = "ID-5";
const GURL kTestURL1("https://www.google.com/");
const GURL kTestURL2("http://www.example.com/");
const GURL kTestURL3("https://news.google.com/");
const GURL kTestURL4("https://chrome.google.com/");
const base::string16 kTestTitle1 = base::ASCIIToUTF16("Title 1");
const base::string16 kTestTitle2 = base::ASCIIToUTF16("Title 2");
const base::string16 kTestTitle3 = base::ASCIIToUTF16("Title 3");
const base::string16 kTestTitle4 = base::ASCIIToUTF16("Title 4");
}  // namespace

class AddUniqueUrlsTaskTest : public PrefetchTaskTestBase {
 public:
  AddUniqueUrlsTaskTest();
  ~AddUniqueUrlsTaskTest() override = default;

  // Returns all items stored in a map keyed with client id.
  std::map<std::string, PrefetchItem> GetAllItems();

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 private:
  TestPrefetchDispatcher dispatcher_;
};

AddUniqueUrlsTaskTest::AddUniqueUrlsTaskTest() {}

std::map<std::string, PrefetchItem> AddUniqueUrlsTaskTest::GetAllItems() {
  std::set<PrefetchItem> set;
  store_util()->GetAllItems(&set);

  std::map<std::string, PrefetchItem> map;
  for (const auto& item : set)
    map[item.client_id.id] = item;
  return map;
}

TEST_F(AddUniqueUrlsTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  RunTask(std::make_unique<AddUniqueUrlsTask>(
      dispatcher(), store(), kTestNamespace, std::vector<PrefetchURL>()));
}

TEST_F(AddUniqueUrlsTaskTest, AddTaskInEmptyStore) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId2, kTestURL2, kTestTitle2});
  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(2u, items.size());
  ASSERT_TRUE(items.count(kClientId1) > 0);
  EXPECT_EQ(kTestURL1, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(kTestTitle1, items[kClientId1].title);
  ASSERT_TRUE(items.count(kClientId2) > 0);
  EXPECT_EQ(kTestURL2, items[kClientId2].url);
  EXPECT_EQ(kTestNamespace, items[kClientId2].client_id.name_space);
  EXPECT_EQ(kTestTitle2, items[kClientId2].title);

  EXPECT_GT(items[kClientId1].creation_time, items[kClientId2].creation_time);

  EXPECT_EQ(1, dispatcher()->task_schedule_count);
}

TEST_F(AddUniqueUrlsTaskTest, SingleDuplicateUrlNotAdded) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
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
}

TEST_F(AddUniqueUrlsTaskTest, DontAddURLIfItExists) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId2, kTestURL2, kTestTitle2});
  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));
  EXPECT_EQ(1, dispatcher()->task_schedule_count);

  urls.clear();
  // This PrefetchURL has a duplicate URL, should not be added.
  urls.push_back(PrefetchURL{kClientId4, kTestURL1, kTestTitle4});
  urls.push_back(PrefetchURL{kClientId3, kTestURL3, kTestTitle3});

  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));
  EXPECT_EQ(2, dispatcher()->task_schedule_count);

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(3u, items.size());
  ASSERT_TRUE(items.count(kClientId1) > 0);
  EXPECT_EQ(kTestURL1, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(kTestTitle1, items[kClientId1].title);
  ASSERT_TRUE(items.count(kClientId2) > 0);
  EXPECT_EQ(kTestURL2, items[kClientId2].url);
  EXPECT_EQ(kTestNamespace, items[kClientId2].client_id.name_space);
  EXPECT_EQ(kTestTitle2, items[kClientId2].title);
  ASSERT_TRUE(items.count(kClientId3) > 0);
  EXPECT_EQ(kTestURL3, items[kClientId3].url);
  EXPECT_EQ(kTestNamespace, items[kClientId3].client_id.name_space);
  EXPECT_EQ(kTestTitle3, items[kClientId3].title);
}

TEST_F(AddUniqueUrlsTaskTest, HandleZombiePrefetchItems) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId2, kTestURL2, kTestTitle2});
  urls.push_back(PrefetchURL{kClientId3, kTestURL3, kTestTitle3});
  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));
  EXPECT_EQ(1, dispatcher()->task_schedule_count);

  // ZombifyPrefetchItem returns the number of affected items.
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(kTestNamespace, urls[0].url));
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(kTestNamespace, urls[1].url));

  urls.clear();
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId3, kTestURL3, kTestTitle3});
  urls.push_back(PrefetchURL{kClientId4, kTestURL4, kTestTitle4});
  // ID-1 is expected to stay in zombie state.
  // ID-2 is expected to be removed, because it is in zombie state.
  // ID-3 is still requested, so it is ignored.
  // ID-4 is added.
  RunTask(std::make_unique<AddUniqueUrlsTask>(dispatcher(), store(),
                                              kTestNamespace, urls));
  EXPECT_EQ(2, dispatcher()->task_schedule_count);

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(3u, items.size());
  ASSERT_TRUE(items.count(kClientId1) > 0);
  EXPECT_EQ(kTestURL1, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(kTestTitle1, items[kClientId1].title);
  ASSERT_TRUE(items.count(kClientId3) > 0);
  EXPECT_EQ(kTestURL3, items[kClientId3].url);
  EXPECT_EQ(kTestNamespace, items[kClientId3].client_id.name_space);
  EXPECT_EQ(kTestTitle3, items[kClientId3].title);
  ASSERT_TRUE(items.count(kClientId4) > 0);
  EXPECT_EQ(kTestURL4, items[kClientId4].url);
  EXPECT_EQ(kTestNamespace, items[kClientId4].client_id.name_space);
  EXPECT_EQ(kTestTitle4, items[kClientId4].title);
}

}  // namespace offline_pages
