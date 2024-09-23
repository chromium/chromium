// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_manager_impl.h"

#include <stdint.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

using Link = ntp_tiles::CustomLinksManager::Link;
using sync_preferences::TestingPrefServiceSyncable;

namespace ntp_tiles {

namespace {

struct TestCaseItem {
  const char* url;
  const char16_t* title;
};

const TestCaseItem kTestCase1[] = {{"http://foo1.com/", u"Foo1"}};
const TestCaseItem kTestCase2[] = {
    {"http://foo1.com/", u"Foo1"},
    {"http://foo2.com/", u"Foo2"},
};
const TestCaseItem kTestCase3[] = {
    {"http://foo1.com/", u"Foo1"},
    {"http://foo2.com/", u"Foo2"},
    {"http://foo3.com/", u"Foo3"},
};
const TestCaseItem kTestCaseMax[] = {
    {"http://foo1.com/", u"Foo1"}, {"http://foo2.com/", u"Foo2"},
    {"http://foo3.com/", u"Foo3"}, {"http://foo4.com/", u"Foo4"},
    {"http://foo5.com/", u"Foo5"}, {"http://foo6.com/", u"Foo6"},
    {"http://foo7.com/", u"Foo7"}, {"http://foo8.com/", u"Foo8"},
    {"http://foo9.com/", u"Foo9"}, {"http://foo10.com/", u"Foo10"},
};

const char kTestTitle[] = "Test";
const char16_t kTestTitle16[] = u"Test";
const char kTestUrl[] = "http://test.com/";

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char16_t kTestGmail16[] = u"Gmail";
const char kTestGmailURL[] =
    "chrome-extension://pjkljhegncpnkpknbcohdijeoejaedia/index.html";
#endif

base::Value::List FillTestList(const char* url,
                               const char* title,
                               const bool is_most_visited) {
  base::Value::List new_link_list;
  base::Value::Dict new_link;
  new_link.Set("url", url);
  new_link.Set("title", title);
  new_link.Set("isMostVisited", is_most_visited);
  new_link_list.Append(std::move(new_link));
  return new_link_list;
}

void AddTile(NTPTilesVector* tiles, const char* url, const char16_t* title) {
  NTPTile tile;
  tile.url = GURL(url);
  tile.title = title;
  tiles->push_back(std::move(tile));
}

NTPTilesVector FillTestTiles(base::span<const TestCaseItem> test_cases) {
  NTPTilesVector tiles;
  for (const auto& test_case : test_cases) {
    AddTile(&tiles, test_case.url, test_case.title);
  }
  return tiles;
}

std::vector<Link> FillTestLinks(base::span<const TestCaseItem> test_cases) {
  std::vector<Link> links;
  for (const auto& test_case : test_cases) {
    links.emplace_back(Link{GURL(test_case.url), test_case.title, true});
  }
  return links;
}

}  // namespace

class CustomLinksManagerImplTest : public testing::Test {
 public:
  CustomLinksManagerImplTest() {
    CustomLinksManagerImpl::RegisterProfilePrefs(prefs_.registry());
    auto defaults =
        base::Value::List().Append("pjkljhegncpnkpknbcohdijeoejaedia");
    prefs_.registry()->RegisterListPref(
        webapps::kWebAppsMigratedPreinstalledApps, std::move(defaults));
  }

  CustomLinksManagerImplTest(const CustomLinksManagerImplTest&) = delete;
  CustomLinksManagerImplTest& operator=(const CustomLinksManagerImplTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(scoped_temp_dir_.GetPath(),
                                                     /*create_db=*/false);
    custom_links_ = std::make_unique<CustomLinksManagerImpl>(
        &prefs_, history_service_.get());
  }

 protected:
  base::ScopedTempDir scoped_temp_dir_;
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<CustomLinksManagerImpl> custom_links_;
};

TEST_F(CustomLinksManagerImplTest, InitializeOnlyOnce) {
  ASSERT_FALSE(custom_links_->IsInitialized());
  ASSERT_TRUE(custom_links_->GetLinks().empty());

  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  EXPECT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to initialize again. This should fail and leave the links intact.
  EXPECT_FALSE(custom_links_->Initialize(FillTestTiles(kTestCase2)));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UninitializeDeletesOldLinks) {
  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());

  custom_links_->Uninitialize();
  EXPECT_TRUE(custom_links_->GetLinks().empty());

  // Initialize with no links.
  EXPECT_TRUE(custom_links_->Initialize(NTPTilesVector()));
  EXPECT_TRUE(custom_links_->GetLinks().empty());
}

TEST_F(CustomLinksManagerImplTest, ReInitializeWithNewLinks) {
  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());

  custom_links_->Uninitialize();
  ASSERT_TRUE(custom_links_->GetLinks().empty());

  // Initialize with new links.
  EXPECT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase2)));
  EXPECT_EQ(FillTestLinks(kTestCase2), custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, AddLink) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Add link.
  std::vector<Link> expected_links = initial_links;
  expected_links.emplace_back(Link{GURL(kTestUrl), kTestTitle16, false});
  EXPECT_TRUE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  EXPECT_EQ(expected_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, AddLinkWhenAtMaxLinks) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCaseMax);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCaseMax)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Try to add link. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, AddDuplicateLink) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Try to add duplicate link. This should fail and not modify the list.
  EXPECT_FALSE(
      custom_links_->AddLink(GURL(kTestCase1[0].url), kTestCase1[0].title));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UpdateLink) {
  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());

  // Update the link's URL.
  EXPECT_TRUE(custom_links_->UpdateLink(GURL(kTestCase1[0].url), GURL(kTestUrl),
                                        std::u16string()));
  EXPECT_EQ(
      std::vector<Link>({Link{GURL(kTestUrl), kTestCase1[0].title, false}}),
      custom_links_->GetLinks());

  // Update the link's title.
  EXPECT_TRUE(custom_links_->UpdateLink(GURL(kTestUrl), GURL(), kTestTitle16));
  EXPECT_EQ(std::vector<Link>({Link{GURL(kTestUrl), kTestTitle16, false}}),
            custom_links_->GetLinks());

  // Update the link's URL and title.
  EXPECT_TRUE(custom_links_->UpdateLink(GURL(kTestUrl), GURL(kTestCase1[0].url),
                                        kTestCase1[0].title));
  EXPECT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase1[0].url), kTestCase1[0].title, false}}),
            custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UpdateLinkWithInvalidParams) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Try to update a link that does not exist. This should fail and not modify
  // the list.
  EXPECT_FALSE(custom_links_->UpdateLink(GURL(kTestUrl), GURL(), kTestTitle16));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to pass empty params. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->UpdateLink(GURL(kTestCase1[0].url), GURL(),
                                         std::u16string()));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to pass an invalid URL. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->UpdateLink(GURL("test"), GURL(), kTestTitle16));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
  EXPECT_FALSE(custom_links_->UpdateLink(GURL(kTestCase1[0].url), GURL("test"),
                                         std::u16string()));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UpdateLinkWhenUrlAlreadyExists) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase2);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase2)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Try to update a link with a URL that exists in the list. This should fail
  // and not modify the list.
  EXPECT_FALSE(custom_links_->UpdateLink(
      GURL(kTestCase2[0].url), GURL(kTestCase2[1].url), std::u16string()));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, ReorderLink) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase3);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase3)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Try to call reorder with the current index. This should fail and not modify
  // the list.
  EXPECT_FALSE(custom_links_->ReorderLink(GURL(kTestCase3[2].url), (size_t)2));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to call reorder with an invalid index. This should fail and not modify
  // the list.
  EXPECT_FALSE(custom_links_->ReorderLink(GURL(kTestCase3[2].url), (size_t)-1));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
  EXPECT_FALSE(custom_links_->ReorderLink(GURL(kTestCase3[2].url),
                                          initial_links.size()));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to call reorder with an invalid URL. This should fail and not modify
  // the list.
  EXPECT_FALSE(custom_links_->ReorderLink(GURL(kTestUrl), 0));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
  EXPECT_FALSE(custom_links_->ReorderLink(GURL("test"), 0));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Move the last link to the front.
  EXPECT_TRUE(custom_links_->ReorderLink(GURL(kTestCase3[2].url), (size_t)0));
  EXPECT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase3[2].url), kTestCase3[2].title, true},
                 Link{GURL(kTestCase3[0].url), kTestCase3[0].title, true},
                 Link{GURL(kTestCase3[1].url), kTestCase3[1].title, true}}),
            custom_links_->GetLinks());

  // Move the same link to the right.
  EXPECT_TRUE(custom_links_->ReorderLink(GURL(kTestCase3[2].url), (size_t)1));
  EXPECT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase3[0].url), kTestCase3[0].title, true},
                 Link{GURL(kTestCase3[2].url), kTestCase3[2].title, true},
                 Link{GURL(kTestCase3[1].url), kTestCase3[1].title, true}}),
            custom_links_->GetLinks());

  // Move the same link to the end.
  EXPECT_TRUE(custom_links_->ReorderLink(GURL(kTestCase3[2].url), (size_t)2));
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, DeleteLink) {
  // Initialize.
  NTPTilesVector initial_tiles;
  AddTile(&initial_tiles, kTestUrl, kTestTitle16);
  ASSERT_TRUE(custom_links_->Initialize(initial_tiles));
  ASSERT_EQ(std::vector<Link>({Link{GURL(kTestUrl), kTestTitle16, true}}),
            custom_links_->GetLinks());

  // Delete link.
  EXPECT_TRUE(custom_links_->DeleteLink(GURL(kTestUrl)));
  EXPECT_TRUE(custom_links_->GetLinks().empty());
}

// The following tests include a default chrome app; these tests are only
// relevant if extensions and apps are enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(CustomLinksManagerImplTest, MigratedDefaultAppDeletedSingle) {
  NTPTilesVector initial_tiles;
  AddTile(&initial_tiles, kTestGmailURL, kTestGmail16);
  // Initialize tile with Gmail URL and then remove them.
  ASSERT_TRUE(custom_links_->Initialize(initial_tiles));
  // Create new instance of CustomLinksManagerImpl to trigger the logic.
  std::unique_ptr<CustomLinksManagerImpl> custom_links_test_ =
      std::make_unique<CustomLinksManagerImpl>(&prefs_, history_service_.get());
  // Should be empty as NTP Default App is Removed.
  ASSERT_TRUE(custom_links_test_->GetLinks().empty());
}

TEST_F(CustomLinksManagerImplTest, DeletedMigratedDefaultAppMultiLink) {
  // Initialize tiles vector with random links + Gmail.
  NTPTilesVector initial_tiles = FillTestTiles(kTestCase2);
  AddTile(&initial_tiles, kTestGmailURL, kTestGmail16);
  // Initialize tiles and fill up custom links.
  ASSERT_TRUE(custom_links_->Initialize(initial_tiles));
  // Create new instance of CustomLinksManagerImpl to trigger the logic.
  std::unique_ptr<CustomLinksManagerImpl> custom_links_test_ =
      std::make_unique<CustomLinksManagerImpl>(&prefs_, history_service_.get());
  // Verify that Gmail does not exist in the custom links.
  ASSERT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase2[0].url), kTestCase2[0].title, true},
                 Link{GURL(kTestCase2[1].url), kTestCase2[1].title, true}}),
            custom_links_test_->GetLinks());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(CustomLinksManagerImplTest, DeleteLinkWhenUrlDoesNotExist) {
  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(NTPTilesVector()));
  ASSERT_TRUE(custom_links_->GetLinks().empty());

  // Try to delete link. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->DeleteLink(GURL(kTestUrl)));
  EXPECT_TRUE(custom_links_->GetLinks().empty());
}

TEST_F(CustomLinksManagerImplTest, UndoAddLink) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Try to undo before add is called. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->UndoAction());
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Add link.
  EXPECT_TRUE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  EXPECT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase1[0].url), kTestCase1[0].title, true},
                 {Link{GURL(kTestUrl), kTestTitle16, false}}}),
            custom_links_->GetLinks());

  // Undo add link.
  EXPECT_TRUE(custom_links_->UndoAction());
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to undo again. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->UndoAction());
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UndoUpdateLink) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Update the link's URL.
  EXPECT_TRUE(custom_links_->UpdateLink(GURL(kTestCase1[0].url), GURL(kTestUrl),
                                        std::u16string()));
  EXPECT_EQ(
      std::vector<Link>({Link{GURL(kTestUrl), kTestCase1[0].title, false}}),
      custom_links_->GetLinks());

  // Undo update link.
  EXPECT_TRUE(custom_links_->UndoAction());
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Update the link's title.
  EXPECT_TRUE(
      custom_links_->UpdateLink(GURL(kTestCase1[0].url), GURL(), kTestTitle16));
  EXPECT_EQ(
      std::vector<Link>({Link{GURL(kTestCase1[0].url), kTestTitle16, false}}),
      custom_links_->GetLinks());

  // Undo update link.
  EXPECT_TRUE(custom_links_->UndoAction());
  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  // Try to undo again. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->UndoAction());
  EXPECT_EQ(initial_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UndoDeleteLink) {
  // Initialize.
  NTPTilesVector initial_tiles;
  AddTile(&initial_tiles, kTestUrl, kTestTitle16);
  std::vector<Link> expected_links({Link{GURL(kTestUrl), kTestTitle16, true}});
  ASSERT_TRUE(custom_links_->Initialize(initial_tiles));
  ASSERT_EQ(expected_links, custom_links_->GetLinks());

  // Delete link.
  ASSERT_TRUE(custom_links_->DeleteLink(GURL(kTestUrl)));
  ASSERT_TRUE(custom_links_->GetLinks().empty());

  // Undo delete link.
  EXPECT_TRUE(custom_links_->UndoAction());
  EXPECT_EQ(expected_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UndoDeleteLinkAfterAdd) {
  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(NTPTilesVector()));
  ASSERT_TRUE(custom_links_->GetLinks().empty());

  // Add link.
  std::vector<Link> expected_links({Link{GURL(kTestUrl), kTestTitle16, false}});
  ASSERT_TRUE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  ASSERT_EQ(expected_links, custom_links_->GetLinks());

  // Delete link.
  ASSERT_TRUE(custom_links_->DeleteLink(GURL(kTestUrl)));
  ASSERT_TRUE(custom_links_->GetLinks().empty());

  // Undo delete link.
  EXPECT_TRUE(custom_links_->UndoAction());
  EXPECT_EQ(expected_links, custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, ShouldDeleteMostVisitedOnHistoryDeletion) {
  NTPTilesVector initial_tiles = FillTestTiles(kTestCase2);
  std::vector<Link> initial_links = FillTestLinks(kTestCase2);
  std::vector<Link> expected_links(initial_links);
  // Remove the link that will be deleted on history clear.
  expected_links.pop_back();

  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase2)));
  ASSERT_EQ(FillTestLinks(kTestCase2), custom_links_->GetLinks());

  // Delete a specific Most Visited link.
  EXPECT_CALL(callback, Run());
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::Invalid(),
                                /*expired=*/false,
                                {history::URLRow(GURL(kTestCase2[1].url))},
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));
  EXPECT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase2[0].url), kTestCase2[0].title, true}}),
            custom_links_->GetLinks());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest,
       ShouldDeleteMostVisitedOnAllHistoryDeletion) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase2)));
  ASSERT_EQ(FillTestLinks(kTestCase2), custom_links_->GetLinks());

  // Delete all Most Visited links.
  EXPECT_CALL(callback, Run());
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::AllTime(),
                                /*expired=*/false, history::URLRows(),
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));
  EXPECT_TRUE(custom_links_->GetLinks().empty());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest, ShouldDeleteOnHistoryDeletionAfterShutdown) {
  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase2)));
  ASSERT_EQ(FillTestLinks(kTestCase2), custom_links_->GetLinks());

  // Simulate shutdown by recreating CustomLinksManagerImpl.
  custom_links_.reset();
  custom_links_ =
      std::make_unique<CustomLinksManagerImpl>(&prefs_, history_service_.get());

  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Delete all Most Visited links.
  EXPECT_CALL(callback, Run());
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::AllTime(),
                                /*expired=*/false, history::URLRows(),
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));
  EXPECT_TRUE(custom_links_->GetLinks().empty());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest, ShouldNotDeleteCustomLinkOnHistoryDeletion) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  std::vector<Link> links_after_add(
      {Link{GURL(kTestCase1[0].url), kTestCase1[0].title, true},
       Link{GURL(kTestUrl), kTestTitle16, false}});
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());
  // Add link.
  ASSERT_TRUE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  ASSERT_EQ(links_after_add, custom_links_->GetLinks());

  // Try to delete the added link. This should fail and not modify the list.
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::Invalid(),
                                /*expired=*/false,
                                {history::URLRow(GURL(kTestUrl))},
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));
  EXPECT_EQ(links_after_add, custom_links_->GetLinks());

  // Delete all Most Visited links.
  EXPECT_CALL(callback, Run());
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::AllTime(),
                                /*expired=*/false, history::URLRows(),
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));
  EXPECT_EQ(std::vector<Link>({Link{GURL(kTestUrl), kTestTitle16, false}}),
            custom_links_->GetLinks());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest, ShouldIgnoreHistoryExpiredDeletions) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  EXPECT_CALL(callback, Run()).Times(0);
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::AllTime(),
                                /*expired=*/true, history::URLRows(),
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(
          // /*history_service=*/nullptr,
          history_service_.get(),
          history::DeletionInfo(history::DeletionTimeRange::Invalid(),
                                /*expired=*/true,
                                {history::URLRow(GURL(kTestCase1[0].url))},
                                /*favicon_urls=*/std::set<GURL>(),
                                /*restrict_urls=*/std::nullopt));

  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest, ShouldIgnoreEmptyHistoryDeletions) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  EXPECT_CALL(callback, Run()).Times(0);
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(history_service_.get(),
                           history::DeletionInfo::ForUrls({}, {}));

  EXPECT_EQ(initial_links, custom_links_->GetLinks());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest, ShouldNotUndoAfterHistoryDeletion) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());
  // Add link.
  std::vector<Link> links_after_add(
      {Link{GURL(kTestCase1[0].url), kTestCase1[0].title, true},
       Link{GURL(kTestUrl), kTestTitle16, false}});
  ASSERT_TRUE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  ASSERT_EQ(links_after_add, custom_links_->GetLinks());

  // Try an empty history deletion. This should do nothing.
  EXPECT_CALL(callback, Run()).Times(0);
  static_cast<history::HistoryServiceObserver*>(custom_links_.get())
      ->OnHistoryDeletions(history_service_.get(),
                           history::DeletionInfo::ForUrls({}, {}));
  EXPECT_EQ(links_after_add, custom_links_->GetLinks());

  // Try to undo. This should fail and not modify the list.
  EXPECT_FALSE(custom_links_->UndoAction());
  EXPECT_EQ(links_after_add, custom_links_->GetLinks());

  task_environment_.RunUntilIdle();
}

TEST_F(CustomLinksManagerImplTest, UpdateListAfterRemoteChange) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(NTPTilesVector()));
  ASSERT_EQ(std::vector<Link>(), custom_links_->GetLinks());

  // Modifying ourselves should not notify.
  EXPECT_CALL(callback, Run()).Times(0);
  EXPECT_TRUE(
      custom_links_->AddLink(GURL(kTestCase1[0].url), kTestCase1[0].title));
  EXPECT_EQ(std::vector<Link>(
                {Link{GURL(kTestCase1[0].url), kTestCase1[0].title, false}}),
            custom_links_->GetLinks());

  // Modify the preference. This should notify and update the current list of
  // links.
  EXPECT_CALL(callback, Run());
  prefs_.SetUserPref(prefs::kCustomLinksList,
                     base::Value(FillTestList(kTestUrl, kTestTitle, true)));
  EXPECT_EQ(std::vector<Link>({Link{GURL(kTestUrl), kTestTitle16, true}}),
            custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, InitializeListAfterRemoteChange) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  ASSERT_FALSE(custom_links_->IsInitialized());

  // Modify the preference. This should notify and initialize custom links.
  EXPECT_CALL(callback, Run()).Times(2);
  prefs_.SetUserPref(prefs::kCustomLinksInitialized, base::Value(true));
  prefs_.SetUserPref(prefs::kCustomLinksList,
                     base::Value(FillTestList(kTestUrl, kTestTitle, false)));
  EXPECT_TRUE(custom_links_->IsInitialized());
  EXPECT_EQ(std::vector<Link>({Link{GURL(kTestUrl), kTestTitle16, false}}),
            custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, UninitializeListAfterRemoteChange) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());

  // Modify the preference. This should notify and uninitialize custom links.
  EXPECT_CALL(callback, Run()).Times(2);
  prefs_.SetUserPref(prefs::kCustomLinksInitialized, base::Value(false));
  prefs_.SetUserPref(prefs::kCustomLinksList, base::Value(base::Value::List()));
  EXPECT_FALSE(custom_links_->IsInitialized());
  EXPECT_EQ(std::vector<Link>(), custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, ClearThenUninitializeListAfterRemoteChange) {
  // Set up Most Visited callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      custom_links_->RegisterCallbackForOnChanged(callback.Get());

  // Initialize.
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(FillTestLinks(kTestCase1), custom_links_->GetLinks());

  // Modify the preference. Simulates when the list preference is synced before
  // the initialized preference. This should notify and uninitialize custom
  // links.
  EXPECT_CALL(callback, Run()).Times(2);
  prefs_.SetUserPref(prefs::kCustomLinksList, base::Value(base::Value::List()));
  EXPECT_TRUE(custom_links_->IsInitialized());
  EXPECT_EQ(std::vector<Link>(), custom_links_->GetLinks());
  prefs_.SetUserPref(prefs::kCustomLinksInitialized, base::Value(false));
  EXPECT_FALSE(custom_links_->IsInitialized());
  EXPECT_EQ(std::vector<Link>(), custom_links_->GetLinks());
}

}  // namespace ntp_tiles
