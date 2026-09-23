// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_manager_impl.h"

#include <stdint.h>

#include <array>
#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using Link = ntp_tiles::CustomLinksManager::Link;
using sync_preferences::TestingPrefServiceSyncable;

namespace ntp_tiles {

void PrintTo(const CustomLinksManager::Link& link, std::ostream* os) {
  *os << "{url: " << link.url.spec()
      << ", title: " << base::UTF16ToUTF8(link.title)
      << ", is_most_visited: " << (link.is_most_visited ? "true" : "false")
      << "}";
}

namespace {

struct TestCaseItem {
  const char* url;
  const char16_t* title;
};

const auto kTestCase1 =
    std::to_array<TestCaseItem>({{"http://foo1.com/", u"Foo1"}});
constexpr auto kTestCase2 = std::to_array<TestCaseItem>({
    {"http://foo1.com/", u"Foo1"},
    {"http://foo2.com/", u"Foo2"},
});
constexpr auto kTestCase3 = std::to_array<TestCaseItem>({
    {"http://foo1.com/", u"Foo1"},
    {"http://foo2.com/", u"Foo2"},
    {"http://foo3.com/", u"Foo3"},
});
const TestCaseItem kTestCaseMax[] = {
    {"http://foo1.com/", u"Foo1"}, {"http://foo2.com/", u"Foo2"},
    {"http://foo3.com/", u"Foo3"}, {"http://foo4.com/", u"Foo4"},
    {"http://foo5.com/", u"Foo5"}, {"http://foo6.com/", u"Foo6"},
    {"http://foo7.com/", u"Foo7"}, {"http://foo8.com/", u"Foo8"},
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    {"http://foo9.com/", u"Foo9"}, {"http://foo10.com/", u"Foo10"},
#endif
};

const char kTestTitle[] = "Test";
const char16_t kTestTitle16[] = u"Test";
const char kTestUrl[] = "http://test.com/";

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char16_t kTestGmail16[] = u"Gmail";
const char kTestGmailURL[] =
    "chrome-extension://pjkljhegncpnkpknbcohdijeoejaedia/index.html";
#endif

base::ListValue FillTestList(const char* url,
                             const char* title,
                             const bool is_most_visited) {
  base::ListValue new_link_list;
  base::DictValue new_link;
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
        base::ListValue().Append("pjkljhegncpnkpknbcohdijeoejaedia");
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
        CustomLinksManagerImpl::Options{
            .prefs = &prefs_, .history_service = history_service_.get()});
  }

  std::unique_ptr<CustomLinksManagerImpl> CreateCustomLinksManager(
      bool enable_ai_mode_tile = false) {
    return std::make_unique<CustomLinksManagerImpl>(
        CustomLinksManagerImpl::Options{
            .prefs = &prefs_,
            .history_service = history_service_.get(),
            .max_links = enable_ai_mode_tile ? kMaxNumCustomLinks + 1
                                             : kMaxNumCustomLinks,
            .enable_ai_mode_tile = enable_ai_mode_tile});
  }

  int GetAiModeTileIndex(const CustomLinksManagerImpl* manager) const {
    return manager->GetAiModeTileIndex();
  }

  void SetAiModeTileIndex(CustomLinksManagerImpl* manager, int index) {
    manager->SetAiModeTileIndex(index);
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

TEST_F(CustomLinksManagerImplTest, AddLinkTo) {
  // Initialize.
  std::vector<Link> initial_links = FillTestLinks(kTestCase1);
  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  ASSERT_EQ(initial_links, custom_links_->GetLinks());

  // Add link in front.
  std::vector<Link> expected_links = initial_links;
  expected_links.insert(expected_links.begin(),
                        Link{GURL(kTestUrl), kTestTitle16, false});
  EXPECT_TRUE(custom_links_->AddLinkTo(GURL(kTestUrl), kTestTitle16, 0U));
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

TEST_F(CustomLinksManagerImplTest, AddLinkUpTo20WhenRedesignFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpShortcutsRedesign);

  // Recreate custom_links_ to pick up the enabled feature flag in its
  // constructor.
  custom_links_ =
      std::make_unique<CustomLinksManagerImpl>(CustomLinksManagerImpl::Options{
          .prefs = &prefs_, .history_service = history_service_.get()});

  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCaseMax)));

  // With the redesign flag enabled, we should be able to add up to 20 custom
  // links since GetMaxShortcutsInExpandedState defaults to 20.
  size_t current_size = custom_links_->GetLinks().size();
  for (size_t i = current_size; i < 20; ++i) {
    std::string url = "http://test.com/" + base::NumberToString(i);
    EXPECT_TRUE(custom_links_->AddLink(GURL(url), u"Test"));
  }

  // Reached 20 links limit. Adding the 21st link should fail.
  EXPECT_FALSE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  EXPECT_EQ(20u, custom_links_->GetLinks().size());
}

// Existing tests check the default 8-link mobile cap. This dedicated test
// verifies the 10-link limit injected via Options on WebUI NTP (AL) builds.
TEST_F(CustomLinksManagerImplTest, AddLinkUpToConfiguredMaxLinks) {
  custom_links_ = std::make_unique<CustomLinksManagerImpl>(
      CustomLinksManagerImpl::Options{.prefs = &prefs_,
                                      .history_service = history_service_.get(),
                                      .max_links = 10});

  ASSERT_TRUE(custom_links_->Initialize(FillTestTiles(kTestCase1)));
  EXPECT_EQ(10u, custom_links_->GetMaxLinks());

  size_t current_size = custom_links_->GetLinks().size();
  for (size_t i = current_size; i < 10; ++i) {
    std::string url = "http://test.com/" + base::NumberToString(i);
    EXPECT_TRUE(custom_links_->AddLink(GURL(url), u"Test"));
  }

  // Reached 10 links limit. Adding the 11th link should fail.
  EXPECT_FALSE(custom_links_->AddLink(GURL(kTestUrl), kTestTitle16));
  EXPECT_EQ(10u, custom_links_->GetLinks().size());
}

TEST_F(CustomLinksManagerImplTest,
       ShouldNotTruncateOverflowLinksWhenExperimentDisabled) {
  // Fallback limit is 10 links.
  // Directly fill the preference store with 15 links as if the user came from
  // the experiment.
  base::ListValue links_list;
  for (int i = 0; i < 15; ++i) {
    std::string url = "http://foo" + base::NumberToString(i) + ".com/";
    std::string title = "Foo" + base::NumberToString(i);

    base::DictValue link;
    link.Set("url", url);
    link.Set("title", title);
    link.Set("isMostVisited", false);
    links_list.Append(std::move(link));
  }

  prefs_.SetUserPref(prefs::kCustomLinksInitialized, base::Value(true));
  prefs_.SetUserPref(prefs::kCustomLinksList,
                     base::Value(std::move(links_list)));

  // Recreate CustomLinksManagerImpl without experiment enabled.
  custom_links_ =
      std::make_unique<CustomLinksManagerImpl>(CustomLinksManagerImpl::Options{
          .prefs = &prefs_, .history_service = history_service_.get()});

  // It should NOT truncate the links to 10, and instead preserve all 15!
  EXPECT_EQ(15u, custom_links_->GetLinks().size());
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
      std::make_unique<CustomLinksManagerImpl>(CustomLinksManagerImpl::Options{
          .prefs = &prefs_, .history_service = history_service_.get()});
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
      std::make_unique<CustomLinksManagerImpl>(CustomLinksManagerImpl::Options{
          .prefs = &prefs_, .history_service = history_service_.get()});
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
      std::make_unique<CustomLinksManagerImpl>(CustomLinksManagerImpl::Options{
          .prefs = &prefs_, .history_service = history_service_.get()});

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
  prefs_.SetUserPref(prefs::kCustomLinksList, base::Value(base::ListValue()));
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
  prefs_.SetUserPref(prefs::kCustomLinksList, base::Value(base::ListValue()));
  EXPECT_TRUE(custom_links_->IsInitialized());
  EXPECT_EQ(std::vector<Link>(), custom_links_->GetLinks());
  prefs_.SetUserPref(prefs::kCustomLinksInitialized, base::Value(false));
  EXPECT_FALSE(custom_links_->IsInitialized());
  EXPECT_EQ(std::vector<Link>(), custom_links_->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, CustomMaxLinksLimit) {
  // Instantiate with a custom limit of 3.
  auto custom_links_limit_3 = std::make_unique<CustomLinksManagerImpl>(
      CustomLinksManagerImpl::Options{.prefs = &prefs_,
                                      .history_service = history_service_.get(),
                                      .max_links = 3});

  ASSERT_TRUE(
      custom_links_limit_3->Initialize(FillTestTiles(kTestCase1)));  // 1 link
  EXPECT_EQ(3u, custom_links_limit_3->GetMaxLinks());

  // Add 2nd link.
  EXPECT_TRUE(custom_links_limit_3->AddLink(GURL("http://foo2.com/"), u"Foo2"));
  // Add 3rd link.
  EXPECT_TRUE(custom_links_limit_3->AddLink(GURL("http://foo3.com/"), u"Foo3"));

  // Reached limit of 3. Adding a 4th link should fail.
  EXPECT_FALSE(
      custom_links_limit_3->AddLink(GURL("http://foo4.com/"), u"Foo4"));

  std::vector<CustomLinksManager::Link> expected;
  for (size_t i = 0; i < kTestCase3.size(); ++i) {
    expected.emplace_back(GURL(kTestCase3[i].url), kTestCase3[i].title,
                          /*is_most_visited=*/i == 0);
  }
  EXPECT_THAT(custom_links_limit_3->GetLinks(),
              testing::ElementsAreArray(expected));
}

// Test that the AI Mode virtual link is inserted when enabled, is not stored
// in persistent storage, that reordering updates the preference correctly,
// and that updating the virtual link is rejected.
TEST_F(CustomLinksManagerImplTest, AiModeLinkInsertionAndReorder) {
  auto custom_links_aim =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/true);

  // Set AI mode link index to 1.
  SetAiModeTileIndex(custom_links_aim.get(), 1);

  // Initialize with test tiles (size 2).
  NTPTilesVector tiles = FillTestTiles(kTestCase2);
  ASSERT_TRUE(custom_links_aim->Initialize(tiles));

  const std::u16string ai_mode_title =
      l10n_util::GetStringUTF16(IDS_NTP_TILES_AI_MODE_TITLE);

  // The AI mode link should be inserted at index 1.
  std::vector<Link> expected_links = {
      Link{GURL(kTestCase2[0].url), kTestCase2[0].title, true},
      Link{GURL(kAiModeTileUrl), ai_mode_title, false},
      Link{GURL(kTestCase2[1].url), kTestCase2[1].title, true},
  };
  EXPECT_EQ(expected_links, custom_links_aim->GetLinks());

  // Verify that it is NOT stored in the preferences (retrieving links directly
  // from the store should not contain chrome://ai-mode).
  CustomLinksStore store(&prefs_);
  std::vector<Link> stored_links = store.RetrieveLinks();
  EXPECT_EQ(FillTestLinks(kTestCase2), stored_links);
  EXPECT_EQ(1, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));

  // Reordering the AI mode link should update the preference.
  EXPECT_TRUE(custom_links_aim->ReorderLink(GURL(kAiModeTileUrl), 0));
  EXPECT_EQ(0, GetAiModeTileIndex(custom_links_aim.get()));
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));

  std::vector<Link> reordered_links = {
      Link{GURL(kAiModeTileUrl), ai_mode_title, false},
      Link{GURL(kTestCase2[0].url), kTestCase2[0].title, true},
      Link{GURL(kTestCase2[1].url), kTestCase2[1].title, true},
  };
  EXPECT_EQ(reordered_links, custom_links_aim->GetLinks());
  EXPECT_EQ(FillTestLinks(kTestCase2), store.RetrieveLinks());

  // Updating the virtual AI mode link should be rejected.
  EXPECT_FALSE(custom_links_aim->UpdateLink(
      GURL(kAiModeTileUrl), GURL("http://new-url.com"), u"New Title"));
  EXPECT_EQ(0, GetAiModeTileIndex(custom_links_aim.get()));
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));
  EXPECT_EQ(reordered_links, custom_links_aim->GetLinks());
  EXPECT_EQ(FillTestLinks(kTestCase2), store.RetrieveLinks());
}

// Test that deleting the AI Mode virtual link unpins it, sets the preference
// to -1, and removes it from the links list.
TEST_F(CustomLinksManagerImplTest, AiModeLinkDeletion) {
  auto custom_links_aim =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/true);

  // Set AI mode link index to 0.
  SetAiModeTileIndex(custom_links_aim.get(), 0);

  NTPTilesVector tiles = FillTestTiles(kTestCase1);
  ASSERT_TRUE(custom_links_aim->Initialize(tiles));

  const std::u16string ai_mode_title =
      l10n_util::GetStringUTF16(IDS_NTP_TILES_AI_MODE_TITLE);

  // The AI mode link should be inserted at index 0.
  std::vector<Link> expected_links = {
      Link{GURL(kAiModeTileUrl), ai_mode_title, false},
      Link{GURL(kTestCase1[0].url), kTestCase1[0].title, true},
  };
  EXPECT_EQ(expected_links, custom_links_aim->GetLinks());

  // Deleting the AI mode link should set the index to -1 and remove it.
  EXPECT_TRUE(custom_links_aim->DeleteLink(GURL(kAiModeTileUrl)));
  EXPECT_EQ(-1, GetAiModeTileIndex(custom_links_aim.get()));
  EXPECT_EQ(-1, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));
  EXPECT_EQ(FillTestLinks(kTestCase1), custom_links_aim->GetLinks());

  CustomLinksStore store(&prefs_);
  EXPECT_EQ(FillTestLinks(kTestCase1), store.RetrieveLinks());

  // A new instance should not restore the AI mode link when unpinned (-1).
  auto custom_links_reloaded =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/true);
  EXPECT_EQ(FillTestLinks(kTestCase1), custom_links_reloaded->GetLinks());
  EXPECT_EQ(-1, GetAiModeTileIndex(custom_links_reloaded.get()));
}

// Test that when the feature is disabled after being enabled, the AI Mode link
// is removed, and when re-enabled later, it is restored at its previous index.
TEST_F(CustomLinksManagerImplTest,
       AiModeLinkDisabledAndReenabledPositionPreserved) {
  // Initially enabled.
  auto custom_links_enabled =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/true);

  NTPTilesVector tiles = FillTestTiles(kTestCase2);
  ASSERT_TRUE(custom_links_enabled->Initialize(tiles));

  const std::u16string ai_mode_title =
      l10n_util::GetStringUTF16(IDS_NTP_TILES_AI_MODE_TITLE);

  // Move AIM link to index 1.
  EXPECT_TRUE(custom_links_enabled->ReorderLink(GURL(kAiModeTileUrl), 1));
  EXPECT_EQ(1, GetAiModeTileIndex(custom_links_enabled.get()));
  EXPECT_EQ(1, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));
  std::vector<Link> expected_links_enabled = {
      Link{GURL(kTestCase2[0].url), kTestCase2[0].title, true},
      Link{GURL(kAiModeTileUrl), ai_mode_title, false},
      Link{GURL(kTestCase2[1].url), kTestCase2[1].title, true},
  };
  EXPECT_EQ(expected_links_enabled, custom_links_enabled->GetLinks());

  // Feature disabled (e.g. user turned off AimAsMvt).
  auto custom_links_disabled =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/false);

  // The AIM link must not be present when disabled.
  EXPECT_EQ(FillTestLinks(kTestCase2), custom_links_disabled->GetLinks());
  // The saved index must be preserved.
  EXPECT_EQ(1, GetAiModeTileIndex(custom_links_disabled.get()));
  EXPECT_EQ(1, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));

  // Feature re-enabled.
  auto custom_links_reenabled =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/true);

  // The AIM link must be restored at its previous index (1).
  EXPECT_EQ(expected_links_enabled, custom_links_reenabled->GetLinks());
  EXPECT_EQ(1, GetAiModeTileIndex(custom_links_reenabled.get()));
  EXPECT_EQ(1, prefs_.GetInteger(prefs::kCustomLinksAiModeTileIndex));
}

#if BUILDFLAG(IS_IOS)
// Test that there are 9 links managed when the AI mode tile is enabled.
TEST_F(CustomLinksManagerImplTest, AiModeLinkNineLinksWhenEnabled) {
  // When AI mode tile is disabled with a max of 8 links, limit is 8.
  auto custom_links_disabled =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/false);
  EXPECT_EQ(8u, custom_links_disabled->GetMaxLinks());

  auto custom_links_enabled =
      CreateCustomLinksManager(/*enable_ai_mode_tile=*/true);
  EXPECT_EQ(9u, custom_links_enabled->GetMaxLinks());

  // Initialize with 8 tiles. The AI mode tile is inserted, making 9 links.
  NTPTilesVector tiles = FillTestTiles(base::span(kTestCaseMax).first(8u));
  ASSERT_TRUE(custom_links_enabled->Initialize(tiles));
  EXPECT_EQ(9u, custom_links_enabled->GetLinks().size());

  // Attempting to add a 10th link should fail because the capacity is 9.
  EXPECT_FALSE(
      custom_links_enabled->AddLink(GURL("http://overflow.com/"), u"Overflow"));
  EXPECT_EQ(9u, custom_links_enabled->GetLinks().size());

  // Persistent storage should only contain the 8 non-virtual custom links.
  CustomLinksStore store(&prefs_);
  EXPECT_EQ(8u, store.RetrieveLinks().size());

  // Deleting a custom link frees up a slot, allowing a new link to be added
  // up to the 9 link limit again.
  EXPECT_TRUE(custom_links_enabled->DeleteLink(GURL(kTestCaseMax[0].url)));
  EXPECT_EQ(8u, custom_links_enabled->GetLinks().size());

  EXPECT_TRUE(custom_links_enabled->AddLink(GURL("http://replacement.com/"),
                                            u"Replacement"));
  EXPECT_EQ(9u, custom_links_enabled->GetLinks().size());
  EXPECT_FALSE(custom_links_enabled->AddLink(GURL("http://overflow2.com/"),
                                             u"Overflow2"));
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(CustomLinksManagerImplTest, MobileScopeUsesMobilePrefKeys) {
  auto mobile_links = std::make_unique<CustomLinksManagerImpl>(
      CustomLinksManagerImpl::Options{.prefs = &prefs_,
                                      .history_service = history_service_.get(),
                                      .scope = CustomLinksScope::kMobile});

  // Initialize with tiles.
  ASSERT_TRUE(mobile_links->Initialize(FillTestTiles(kTestCase1)));

  // Verify data is stored in the mobile pref key.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kCustomLinksInitializedMobile));
  EXPECT_FALSE(prefs_.GetList(prefs::kCustomLinksListMobile).empty());

  // Verify desktop pref key is unaffected.
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kCustomLinksInitialized));
  EXPECT_TRUE(prefs_.GetList(prefs::kCustomLinksList).empty());
}

TEST_F(CustomLinksManagerImplTest, MobileScopeRemoteChangeNotifiesCorrectPref) {
  auto mobile_links = std::make_unique<CustomLinksManagerImpl>(
      CustomLinksManagerImpl::Options{.prefs = &prefs_,
                                      .history_service = history_service_.get(),
                                      .scope = CustomLinksScope::kMobile});

  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      mobile_links->RegisterCallbackForOnChanged(callback.Get());

  ASSERT_FALSE(mobile_links->IsInitialized());

  // Modify the mobile preference. This should notify and initialize.
  EXPECT_CALL(callback, Run()).Times(2);
  prefs_.SetUserPref(prefs::kCustomLinksInitializedMobile, base::Value(true));
  prefs_.SetUserPref(prefs::kCustomLinksListMobile,
                     base::Value(FillTestList(kTestUrl, kTestTitle, false)));
  EXPECT_TRUE(mobile_links->IsInitialized());
}

TEST_F(CustomLinksManagerImplTest, DesktopChangeDoesNotReachMobileScope) {
  auto mobile_links = std::make_unique<CustomLinksManagerImpl>(
      CustomLinksManagerImpl::Options{.prefs = &prefs_,
                                      .history_service = history_service_.get(),
                                      .scope = CustomLinksScope::kMobile});

  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      mobile_links->RegisterCallbackForOnChanged(callback.Get());

  ASSERT_FALSE(mobile_links->IsInitialized());

  // A change to the desktop prefs must not be observed by a mobile-scoped
  // manager, otherwise desktop shortcuts and mobile tiles would overwrite each
  // other through sync.
  EXPECT_CALL(callback, Run()).Times(0);
  prefs_.SetUserPref(prefs::kCustomLinksInitialized, base::Value(true));
  prefs_.SetUserPref(prefs::kCustomLinksList,
                     base::Value(FillTestList(kTestUrl, kTestTitle, false)));

  EXPECT_FALSE(mobile_links->IsInitialized());
  EXPECT_EQ(std::vector<Link>(), mobile_links->GetLinks());
}

TEST_F(CustomLinksManagerImplTest, DefaultScopeUsesDesktopPrefKeys) {
  // Embedders that do not name a scope get the desktop storage domain. Keep
  // this pinned: flipping the default silently moves every such caller's data.
  auto default_links =
      std::make_unique<CustomLinksManagerImpl>(CustomLinksManagerImpl::Options{
          .prefs = &prefs_, .history_service = history_service_.get()});

  ASSERT_TRUE(default_links->Initialize(FillTestTiles(kTestCase1)));

  EXPECT_TRUE(prefs_.GetBoolean(prefs::kCustomLinksInitialized));
  EXPECT_FALSE(prefs_.GetList(prefs::kCustomLinksList).empty());

  EXPECT_FALSE(prefs_.GetBoolean(prefs::kCustomLinksInitializedMobile));
  EXPECT_TRUE(prefs_.GetList(prefs::kCustomLinksListMobile).empty());
}

}  // namespace ntp_tiles
