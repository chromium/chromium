// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class OpenTabProviderTest : public testing::Test {
 public:
  OpenTabProviderTest() {
    client_.set_template_url_service(
        search_engines_test_environment_.template_url_service());
    open_tab_provider_ = new OpenTabProvider(&client_);
  }

  FakeAutocompleteProviderClient& client() { return client_; }

  OpenTabProvider& open_tab_provider() { return *open_tab_provider_; }

  void SetUpStarterPack() {
    std::vector<std::unique_ptr<TemplateURLData>> turls =
        template_url_starter_pack_data::GetStarterPackEngines();
    for (auto& turl : turls) {
      client().GetTemplateURLService()->Add(
          std::make_unique<TemplateURL>(std::move(*turl)));
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  FakeAutocompleteProviderClient client_;
  scoped_refptr<OpenTabProvider> open_tab_provider_;
};

TEST_F(OpenTabProviderTest, TestNoResults) {
  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, open_tab_provider().matches().size());
}

TEST_F(OpenTabProviderTest, TestTitleMatch) {
  TabMatcher::TabWrapper open_tab(u"test title", GURL("http://google.com"),
                                  base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, open_tab_provider().matches().size());
}

TEST_F(OpenTabProviderTest, TestURLMatch) {
  TabMatcher::TabWrapper open_tab(u"google", GURL("http://test.com"),
                                  base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, open_tab_provider().matches().size());
}

TEST_F(OpenTabProviderTest, TestChromeNewTabPageOmitted) {
  TabMatcher::TabWrapper open_tab(u"test", GURL("chrome-native://newtab/"),
                                  base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  open_tab = TabMatcher::TabWrapper(
      u"test bookmarks", GURL("chrome-native://bookmarks/"), base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  open_tab = TabMatcher::TabWrapper(u"test history", GURL("chrome://history/"),
                                    base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  open_tab = TabMatcher::TabWrapper(u"test scheme match",
                                    GURL("test://newtab/"), base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);

  int test_index = 0;
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(3UL, open_tab_provider().matches().size());
#else
  ASSERT_EQ(4UL, open_tab_provider().matches().size());
  ASSERT_EQ(open_tab_provider().matches()[test_index++].destination_url,
            "chrome-native://newtab/");
#endif
  ASSERT_EQ(open_tab_provider().matches()[test_index++].destination_url,
            "chrome-native://bookmarks/");
  ASSERT_EQ(open_tab_provider().matches()[test_index++].destination_url,
            "chrome://history/");
  ASSERT_EQ(open_tab_provider().matches()[test_index++].destination_url,
            "test://newtab/");
}

TEST_F(OpenTabProviderTest, TestNoMatches) {
  FakeTabMatcher& tab_matcher = static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"foo", GURL("http://foo.com"), base::Time()));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"bar", GURL("http://bar.com"), base::Time()));

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, open_tab_provider().matches().size());
}

TEST_F(OpenTabProviderTest, TestZPS) {
  FakeTabMatcher& tab_matcher = static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"foo", GURL("http://foo.com"), base::Time()));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"bar", GURL("http://bar.com"), base::Time()));

  AutocompleteInput input(u"",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);

#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(2UL, open_tab_provider().matches().size());
#else
  ASSERT_EQ(0UL, open_tab_provider().matches().size());
#endif
}

TEST_F(OpenTabProviderTest, KeywordMode) {
  SetUpStarterPack();

  TabMatcher::TabWrapper open_tab(u"google", GURL("http://test.com/"),
                                  base::Time());
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  // In keyword mode, "@tabs google" should match since we're only trying
  // to match "google".
  AutocompleteInput input(u"@tabs google", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prefer_keyword(true);
  input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  open_tab_provider().Start(input, /*minimal_changes=*/false);

  ASSERT_EQ(open_tab_provider().matches().size(), 1UL);
  EXPECT_EQ(open_tab_provider().matches()[0].destination_url,
            GURL("http://test.com/"));

  // Ensure `fill_to_edit` includes the keyword.
  EXPECT_EQ(open_tab_provider().matches()[0].fill_into_edit,
            u"@tabs http://test.com/");
}

TEST_F(OpenTabProviderTest, TestZPS_WeightedByRecency) {
  // Ordering by recency means that the tabs should be ordered:
  // baz -> foo -> bar
  FakeTabMatcher& tab_matcher = static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"foo", GURL("http://foo.com"),
                             base::Time::FromSecondsSinceUnixEpoch(2)));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"bar", GURL("http://bar.com"),
                             base::Time::FromSecondsSinceUnixEpoch(1)));
  tab_matcher.AddOpenTab(
      TabMatcher::TabWrapper(u"baz", GURL("http://baz.com"),
                             base::Time::FromSecondsSinceUnixEpoch(3)));

  AutocompleteInput input(u"",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);

#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(3UL, open_tab_provider().matches().size());
  auto match1 = open_tab_provider().matches()[0];
  auto match2 = open_tab_provider().matches()[1];
  auto match3 = open_tab_provider().matches()[2];
  ASSERT_GT(match1.relevance, match2.relevance);
  ASSERT_GT(match3.relevance, match1.relevance);
#else
  ASSERT_EQ(0UL, open_tab_provider().matches().size());
#endif
}
