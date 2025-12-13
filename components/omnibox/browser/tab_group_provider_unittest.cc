// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/tab_group_provider.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class TabGroupProviderTest : public testing::Test {
 public:
  void SetUp() override {
    tab_group_provider_ = new TabGroupProvider(&client_);
  }

  FakeAutocompleteProviderClient& client() { return client_; }

  TabGroupProvider& tab_group_provider() { return *tab_group_provider_; }

  tab_groups::SavedTabGroup CreateSavedTabGroup(std::vector<std::string> urls,
                                                std::u16string title) {
    const base::Uuid kGroupGuid = base::Uuid::GenerateRandomV4();

    // Add a saved tab group locally and simulate a remote creation of a shared
    // tab group with the same GUID.
    tab_groups::SavedTabGroup saved_group(
        title, tab_groups::TabGroupColorId::kGrey,
        /*urls=*/{}, /*position=*/0, kGroupGuid);

    for (size_t i = 0; i < urls.size(); i++) {
      tab_groups::SavedTabGroupTab tab(GURL(urls[i]), u"Saved tab", kGroupGuid,
                                       /*position=*/i);
      saved_group.AddTabLocally(tab);
    }
    return saved_group;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  FakeAutocompleteProviderClient client_;
  scoped_refptr<TabGroupProvider> tab_group_provider_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TabGroupProviderTest, TestNoResults) {
  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, tab_group_provider().matches().size());
}

TEST_F(TabGroupProviderTest, TestOneTitleMatch) {
  std::vector<std::string> urls;
  urls.push_back("http://google.com/saved_1");
  urls.push_back("http://google.com/saved_2");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
  ASSERT_EQ("0", tab_group_provider().matches()[0].image_dominant_color);
  ASSERT_EQ(u"google.com/saved_1, google.com/saved_2",
            tab_group_provider().matches()[0].description);
}

TEST_F(TabGroupProviderTest, TestSkipProviderResultsOnIncognito) {
  std::vector<std::string> urls;
  urls.push_back("http://google.com/saved_1");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  // Mock that the browser is in incognito mode.
  EXPECT_CALL(client(), IsOffTheRecord()).WillRepeatedly(testing::Return(true));

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, tab_group_provider().matches().size());
}

TEST_F(TabGroupProviderTest, TestUrlMatchSameLinks) {
  std::vector<std::string> urls;
  urls.push_back("http://google.com/saved_1");
  urls.push_back("http://google.com/saved_2");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"goo",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
  ASSERT_EQ("0", tab_group_provider().matches()[0].image_dominant_color);
  ASSERT_EQ(u"google.com/saved_1, google.com/saved_2",
            tab_group_provider().matches()[0].description);
}

TEST_F(TabGroupProviderTest, TestUrlMatchDifferentLinksReorder) {
  std::vector<std::string> urls;
  urls.push_back("http://test.com/saved_1");
  urls.push_back("http://google.com/saved_2");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"goo",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
  ASSERT_EQ("0", tab_group_provider().matches()[0].image_dominant_color);
  ASSERT_EQ(u"google.com/saved_2, test.com/saved_1",
            tab_group_provider().matches()[0].description);
}

TEST_F(TabGroupProviderTest, TestUrlMatchSingleLink) {
  std::vector<std::string> urls;
  urls.push_back("http://google.com/saved_1");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"goo",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
  ASSERT_EQ("0", tab_group_provider().matches()[0].image_dominant_color);
  ASSERT_EQ(u"google.com/saved_1",
            tab_group_provider().matches()[0].description);
}

TEST_F(TabGroupProviderTest, TestSkipUrlMatchUnnamedGroup) {
  std::vector<std::string> urls;
  urls.push_back("http://google.com/saved_1");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u""));

  AutocompleteInput input(u"goo",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, tab_group_provider().matches().size());
}

TEST_F(TabGroupProviderTest, TestFilterChromePrefixedTabs) {
  std::vector<std::string> urls;
  urls.push_back("chrome://newtab/");
  urls.push_back("http://google.com/saved_2");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
  ASSERT_EQ("0", tab_group_provider().matches()[0].image_dominant_color);
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(u"google.com/saved_2",
            tab_group_provider().matches()[0].description);
#else
  ASSERT_EQ(u"chrome://newtab, google.com/saved_2",
            tab_group_provider().matches()[0].description);
#endif
}

TEST_F(TabGroupProviderTest, TestFilterChromePrefixedTabsNoDescription) {
  std::vector<std::string> urls;
  urls.push_back("chrome://newtab/");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
  ASSERT_EQ("0", tab_group_provider().matches()[0].image_dominant_color);
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(u"", tab_group_provider().matches()[0].description);
#else
  ASSERT_EQ(u"chrome://newtab", tab_group_provider().matches()[0].description);
#endif
}

TEST_F(TabGroupProviderTest, TestNoMatchResultsOnChromePrefixedUrlMatch) {
  std::vector<std::string> urls;
  urls.push_back("chrome://newtab/");

  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup(urls, u"test"));

  AutocompleteInput input(u"new",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(0UL, tab_group_provider().matches().size());
#else
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
#endif
}
