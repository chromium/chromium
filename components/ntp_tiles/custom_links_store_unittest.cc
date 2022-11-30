// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_store.h"

#include <stdint.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace ntp_tiles {

namespace {

const char16_t kTestTitle1[] = u"Foo1";
const char16_t kTestTitle2[] = u"Foo2";
const char kTestUrl1[] = "http://foo1.com/";
const char kTestUrl2[] = "http://foo2.com/";

}  // namespace

class CustomLinksStoreTest : public testing::Test {
 public:
  CustomLinksStoreTest() {
    custom_links_store_ = std::make_unique<CustomLinksStore>(&prefs_);
    CustomLinksStore::RegisterProfilePrefs(prefs_.registry());
  }

  CustomLinksStoreTest(const CustomLinksStoreTest&) = delete;
  CustomLinksStoreTest& operator=(const CustomLinksStoreTest&) = delete;

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<CustomLinksStore> custom_links_store_;
};

TEST_F(CustomLinksStoreTest, StoreAndRetrieveLinks) {
  std::vector<CustomLinksManager::Link> initial_links(
      {CustomLinksManager::Link{GURL(kTestUrl1), kTestTitle1, true}});

  custom_links_store_->StoreLinks(initial_links);
  std::vector<CustomLinksManager::Link> retrieved_links =
      custom_links_store_->RetrieveLinks();
  EXPECT_EQ(initial_links, retrieved_links);
}

TEST_F(CustomLinksStoreTest, StoreEmptyList) {
  std::vector<CustomLinksManager::Link> populated_links(
      {CustomLinksManager::Link{GURL(kTestUrl1), kTestTitle1, false},
       CustomLinksManager::Link{GURL(kTestUrl2), kTestTitle2, true}});

  custom_links_store_->StoreLinks(populated_links);
  std::vector<CustomLinksManager::Link> retrieved_links =
      custom_links_store_->RetrieveLinks();
  ASSERT_EQ(populated_links, retrieved_links);

  custom_links_store_->StoreLinks(std::vector<CustomLinksManager::Link>());
  retrieved_links = custom_links_store_->RetrieveLinks();
  EXPECT_TRUE(retrieved_links.empty());
}

TEST_F(CustomLinksStoreTest, ClearLinks) {
  std::vector<CustomLinksManager::Link> initial_links(
      {CustomLinksManager::Link{GURL(kTestUrl1), kTestTitle1}});

  custom_links_store_->StoreLinks(initial_links);
  std::vector<CustomLinksManager::Link> retrieved_links =
      custom_links_store_->RetrieveLinks();
  ASSERT_EQ(initial_links, retrieved_links);

  custom_links_store_->ClearLinks();
  retrieved_links = custom_links_store_->RetrieveLinks();
  EXPECT_TRUE(retrieved_links.empty());
}

TEST_F(CustomLinksStoreTest, LinksSavedAfterShutdown) {
  std::vector<CustomLinksManager::Link> initial_links(
      {CustomLinksManager::Link{GURL(kTestUrl1), kTestTitle1, false},
       CustomLinksManager::Link{GURL(kTestUrl2), kTestTitle2, true}});

  custom_links_store_->StoreLinks(initial_links);
  std::vector<CustomLinksManager::Link> retrieved_links =
      custom_links_store_->RetrieveLinks();
  ASSERT_EQ(initial_links, retrieved_links);

  // Simulate shutdown by recreating CustomLinksStore.
  custom_links_store_.reset();
  custom_links_store_ = std::make_unique<CustomLinksStore>(&prefs_);
  retrieved_links = custom_links_store_->RetrieveLinks();
  EXPECT_EQ(initial_links, retrieved_links);
}

}  // namespace ntp_tiles
