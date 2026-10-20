// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/recently_closed_tabs_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/core/mock_tab_restore_service.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/tab_restore_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "url/gurl.h"

namespace {

using OEP = metrics::OmniboxEventProto;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;
using testing::ReturnRef;
using testing::SizeIs;
using testing::UnorderedElementsAre;

class MockAutocompleteProviderListener : public AutocompleteProviderListener {
 public:
  MOCK_METHOD(void,
              OnProviderUpdate,
              (bool updated_matches, const AutocompleteProvider* provider),
              (override));
};

class RecentlyClosedTabsProviderTest : public testing::Test {
 public:
  RecentlyClosedTabsProviderTest() {
    feature_list_.InitAndEnableFeature(omnibox::kAndroidTabSearchRecentTabs);
    client_.set_tab_restore_service(&tab_restore_service_);
    ON_CALL(tab_restore_service_, IsLoaded()).WillByDefault(Return(true));
    ON_CALL(tab_restore_service_, entries()).WillByDefault(ReturnRef(entries_));
    provider_ =
        base::MakeRefCounted<RecentlyClosedTabsProvider>(&client_, &listener_);
  }

 protected:
  // Appends a recently closed tab, i.e. less recently closed than the tabs
  // already added.
  void AddClosedTab(const std::string& url, const std::u16string& title) {
    auto tab = std::make_unique<sessions::tab_restore::Tab>();
    sessions::SerializedNavigationEntry navigation;
    navigation.set_virtual_url(GURL(url));
    navigation.set_title(title);
    tab->navigations.push_back(navigation);
    tab->current_navigation_index = 0;
    entries_.push_back(std::move(tab));
  }

  static AutocompleteInput CreateInput(
      const std::u16string& text,
      OEP::PageClassification page_classification =
          OEP::ANDROID_TAB_SEARCH_OVERLAY) {
    return AutocompleteInput(text, page_classification, TestSchemeClassifier());
  }

  std::vector<GURL> GetMatchUrls() const {
    std::vector<GURL> urls;
    for (const auto& match : provider_->matches()) {
      urls.push_back(match.destination_url);
    }
    return urls;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockTabRestoreService> tab_restore_service_;
  sessions::TabRestoreService::Entries entries_;
  MockAutocompleteProviderClient client_;
  testing::NiceMock<MockAutocompleteProviderListener> listener_;
  scoped_refptr<RecentlyClosedTabsProvider> provider_;
};

TEST_F(RecentlyClosedTabsProviderTest, ZeroSuggest) {
  for (int i = 0; i < 7; ++i) {
    AddClosedTab("https://example.com/" + base::NumberToString(i),
                 u"Title " + base::NumberToString16(i));
  }

  provider_->Start(CreateInput(u""), /*minimal_changes=*/false);

  // The 5 most recently closed tabs are suggested in order.
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(
      GetMatchUrls(),
      ElementsAre(GURL("https://example.com/0"), GURL("https://example.com/1"),
                  GURL("https://example.com/2"), GURL("https://example.com/3"),
                  GURL("https://example.com/4")));
  for (size_t i = 0; i < provider_->matches().size(); ++i) {
    const AutocompleteMatch& match = provider_->matches()[i];
    EXPECT_EQ(
        omnibox::kRecentlyClosedTabZeroSuggestRelevance - static_cast<int>(i),
        match.relevance);
    EXPECT_EQ(omnibox::AutocompleteMatchType::kRecentlyClosedTab, match.type);
    EXPECT_EQ(omnibox::GROUP_RECENTLY_CLOSED_TABS, match.suggestion_group_id);
    EXPECT_TRUE(match.allowed_to_be_default_match);
    EXPECT_EQ(u"Title " + base::NumberToString16(i), match.description);
    EXPECT_EQ(false, match.has_tab_match);
  }
}

TEST_F(RecentlyClosedTabsProviderTest, Typed_MatchesTitleOrUrl) {
  AddClosedTab("https://foo.com/", u"Unrelated");
  AddClosedTab("https://bar.com/", u"Foo article");
  AddClosedTab("https://baz.com/", u"Unrelated");

  provider_->Start(CreateInput(u"foo"), /*minimal_changes=*/false);

  EXPECT_THAT(GetMatchUrls(), UnorderedElementsAre(GURL("https://foo.com/"),
                                                   GURL("https://bar.com/")));
  EXPECT_TRUE(std::ranges::is_sorted(provider_->matches(),
                                     std::ranges::greater(),
                                     &AutocompleteMatch::relevance));
  EXPECT_LE(provider_->matches().front().relevance, 1000);
}

TEST_F(RecentlyClosedTabsProviderTest, Typed_CappedAtMaxMatches) {
  for (int i = 0; i < 7; ++i) {
    AddClosedTab("https://example.com/" + base::NumberToString(i), u"Foo");
  }

  provider_->Start(CreateInput(u"foo"), /*minimal_changes=*/false);

  EXPECT_THAT(provider_->matches(), SizeIs(5));
}

TEST_F(RecentlyClosedTabsProviderTest, SkipsUnsupportedAndDuplicateEntries) {
  entries_.push_back(std::make_unique<sessions::tab_restore::Window>());
  entries_.push_back(std::make_unique<sessions::tab_restore::Tab>());
  AddClosedTab("invalid url", u"Invalid");
  AddClosedTab("chrome://history/", u"History");
  AddClosedTab("https://example.com/", u"Example");
  AddClosedTab("https://example.com/", u"Older duplicate");

  provider_->Start(CreateInput(u""), /*minimal_changes=*/false);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(GetMatchUrls(), ElementsAre(GURL("https://example.com/")));
#else
  EXPECT_THAT(GetMatchUrls(), ElementsAre(GURL("chrome://history/"),
                                          GURL("https://example.com/")));
#endif
  // The most recently closed tab is kept for a duplicate URL.
  EXPECT_EQ(u"Example", provider_->matches().back().description);
}

TEST_F(RecentlyClosedTabsProviderTest, NoMatchesWhenUnavailable) {
  AddClosedTab("https://example.com/", u"Example");
  {
    SCOPED_TRACE("Feature disabled");
    base::test::ScopedFeatureList disabled_features;
    disabled_features.InitAndDisableFeature(
        omnibox::kAndroidTabSearchRecentTabs);
    provider_->Start(CreateInput(u""), /*minimal_changes=*/false);
    EXPECT_THAT(provider_->matches(), IsEmpty());
  }
  {
    SCOPED_TRACE("Incognito");
    ON_CALL(client_, IsOffTheRecord()).WillByDefault(Return(true));
    provider_->Start(CreateInput(u""), /*minimal_changes=*/false);
    EXPECT_THAT(provider_->matches(), IsEmpty());
    ON_CALL(client_, IsOffTheRecord()).WillByDefault(Return(false));
  }
  {
    SCOPED_TRACE("No TabRestoreService");
    client_.set_tab_restore_service(nullptr);
    provider_->Start(CreateInput(u""), /*minimal_changes=*/false);
    EXPECT_THAT(provider_->matches(), IsEmpty());
    client_.set_tab_restore_service(&tab_restore_service_);
  }
  {
    SCOPED_TRACE("Asynchronous matches omitted while loading");
    ON_CALL(tab_restore_service_, IsLoaded()).WillByDefault(Return(false));
    // The service is still loaded for later requests, but is not waited on.
    EXPECT_CALL(tab_restore_service_, LoadTabsFromLastSession());
    EXPECT_CALL(tab_restore_service_, AddObserver).Times(0);
    AutocompleteInput input = CreateInput(u"");
    input.set_omit_asynchronous_matches(true);
    provider_->Start(input, /*minimal_changes=*/false);
    EXPECT_TRUE(provider_->done());
    EXPECT_THAT(provider_->matches(), IsEmpty());
  }
}

TEST_F(RecentlyClosedTabsProviderTest, OtherPageClassifications) {
  AddClosedTab("https://example.com/", u"Example");
  EXPECT_CALL(tab_restore_service_, entries()).Times(0);

  for (OEP::PageClassification page_classification :
       {OEP::OTHER, OEP::ANDROID_HUB}) {
    SCOPED_TRACE(page_classification);
    provider_->Start(CreateInput(u"", page_classification),
                     /*minimal_changes=*/false);
#if BUILDFLAG(IS_ANDROID)
    // Only the Tab Search overlay is supported on Android.
    EXPECT_THAT(provider_->matches(), IsEmpty());
#else
    // The desktop placeholder match is unchanged.
    EXPECT_THAT(GetMatchUrls(), ElementsAre(GURL("https://google.com")));
#endif
  }
}

TEST_F(RecentlyClosedTabsProviderTest, WaitsForTabRestoreServiceToLoad) {
  AddClosedTab("https://example.com/", u"Example");
  ON_CALL(tab_restore_service_, IsLoaded()).WillByDefault(Return(false));
  // Restarting while the service loads observes it only once, and the latest
  // input is used once it loads.
  EXPECT_CALL(tab_restore_service_, AddObserver(provider_.get()));
  EXPECT_CALL(tab_restore_service_, LoadTabsFromLastSession()).Times(2);

  provider_->Start(CreateInput(u"no match"), /*minimal_changes=*/false);
  provider_->Start(CreateInput(u""), /*minimal_changes=*/false);
  EXPECT_FALSE(provider_->done());
  EXPECT_THAT(provider_->matches(), IsEmpty());
  testing::Mock::VerifyAndClearExpectations(&tab_restore_service_);

  EXPECT_CALL(tab_restore_service_, RemoveObserver(provider_.get()));
  EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get()));
  provider_->TabRestoreServiceLoaded(&tab_restore_service_);

  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatchUrls(), ElementsAre(GURL("https://example.com/")));
}

TEST_F(RecentlyClosedTabsProviderTest, PendingLoadCancelled) {
  AddClosedTab("https://example.com/", u"Example");
  ON_CALL(tab_restore_service_, IsLoaded()).WillByDefault(Return(false));
  EXPECT_CALL(listener_, OnProviderUpdate).Times(0);
  {
    SCOPED_TRACE("Stopped");
    provider_->Start(CreateInput(u""), /*minimal_changes=*/false);
    provider_->Stop(AutocompleteStopReason::kClobbered);
    EXPECT_TRUE(provider_->done());
    provider_->TabRestoreServiceLoaded(&tab_restore_service_);
    EXPECT_THAT(provider_->matches(), IsEmpty());
  }
  {
    SCOPED_TRACE("Service destroyed");
    provider_->Start(CreateInput(u""), /*minimal_changes=*/false);
    EXPECT_CALL(tab_restore_service_, RemoveObserver(provider_.get()));
    provider_->TabRestoreServiceDestroyed(&tab_restore_service_);
    EXPECT_TRUE(provider_->done());
    EXPECT_THAT(provider_->matches(), IsEmpty());
  }
}

}  // namespace
