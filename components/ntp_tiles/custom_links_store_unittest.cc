// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_store.h"

#include <stdint.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/ntp_tiles/pref_names.h"
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

TEST_F(CustomLinksStoreTest, MobileScopeUsesMobilePrefKey) {
  auto mobile_store =
      std::make_unique<CustomLinksStore>(&prefs_, CustomLinksScope::kMobile);

  std::vector<CustomLinksManager::Link> links(
      {CustomLinksManager::Link{GURL(kTestUrl1), kTestTitle1, true}});

  // Store links using mobile store.
  mobile_store->StoreLinks(links);

  // Verify data is in the mobile pref key.
  EXPECT_FALSE(prefs_.GetList(prefs::kCustomLinksListMobile).empty());

  // Verify desktop pref key is unaffected.
  EXPECT_TRUE(prefs_.GetList(prefs::kCustomLinksList).empty());

  // Retrieve from mobile store should return the links.
  std::vector<CustomLinksManager::Link> retrieved =
      mobile_store->RetrieveLinks();
  EXPECT_EQ(links, retrieved);
}

TEST_F(CustomLinksStoreTest, DesktopAndMobilePrefsAreIsolated) {
  // |custom_links_store_| is the fixture's default-scope (desktop) store.
  auto mobile_store =
      std::make_unique<CustomLinksStore>(&prefs_, CustomLinksScope::kMobile);

  std::vector<CustomLinksManager::Link> desktop_links(
      {CustomLinksManager::Link{GURL(kTestUrl1), kTestTitle1, true}});
  std::vector<CustomLinksManager::Link> mobile_links(
      {CustomLinksManager::Link{GURL(kTestUrl2), kTestTitle2, false}});

  // Store different links in each store.
  custom_links_store_->StoreLinks(desktop_links);
  mobile_store->StoreLinks(mobile_links);

  // Each store should only see its own links.
  EXPECT_EQ(desktop_links, custom_links_store_->RetrieveLinks());
  EXPECT_EQ(mobile_links, mobile_store->RetrieveLinks());
}

TEST_F(CustomLinksStoreTest, DefaultScopeUsesDesktopPrefKey) {
  // Callers that omit the scope get the desktop storage domain.
  CustomLinksStore default_store(&prefs_);
  EXPECT_STREQ(prefs::kCustomLinksList, default_store.list_pref_name());

  CustomLinksStore mobile_store(&prefs_, CustomLinksScope::kMobile);
  EXPECT_STREQ(prefs::kCustomLinksListMobile, mobile_store.list_pref_name());
}

}  // namespace ntp_tiles
