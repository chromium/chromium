// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_tile_provider.h"
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/query_tiles/tile_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

using PageClass = metrics::OmniboxEventProto;
using FocusType = metrics::OmniboxFocusType;
using testing::_;

class MockTileService : public query_tiles::TileService {
 public:
  MOCK_METHOD1(GetQueryTiles, void(query_tiles::GetTilesCallback));
  MOCK_METHOD2(GetTile, void(const std::string&, query_tiles::TileCallback));
  MOCK_METHOD2(StartFetchForTiles,
               void(bool, query_tiles::BackgroundTaskFinishedCallback));
  MOCK_METHOD0(CancelTask, void());
  MOCK_METHOD0(PurgeDb, void());
  MOCK_METHOD1(OnTileClicked, void(const std::string&));
  MOCK_METHOD1(SetServerUrl, void(const std::string&));
  MOCK_METHOD2(OnQuerySelected,
               void(const absl::optional<std::string>&, const std::u16string&));
  MOCK_METHOD0(GetLogger, query_tiles::Logger*());
};

class MockAutocompleteListener : public AutocompleteProviderListener {
 public:
  MOCK_METHOD2(OnProviderUpdate, void(bool, const AutocompleteProvider*));
};

class QueryTileProviderTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<MockTileService> mock_tile_svc =
        std::make_unique<MockTileService>();
    tile_service_ = mock_tile_svc.get();
    client_.set_tile_service(std::move(mock_tile_svc));
    provider_ = base::MakeRefCounted<QueryTileProvider>(&client_, &listener_);
    url_service_ = client_.GetTemplateURLService();
    url_service_->Load();

    // Verify that Google is the default search provider.
    ASSERT_TRUE(search::DefaultSearchProviderIsGoogle(url_service_));

    ON_CALL(client_, SearchSuggestEnabled())
        .WillByDefault(testing::Return(true));
    // Save GetTilesCallback; unfortunately can't use SaveArg<>.
    ON_CALL(*tile_service_, GetQueryTiles(_))
        .WillByDefault(testing::WithArg<0>(testing::Invoke(
            [&](auto callback) { tile_callback_ = std::move(callback); })));
  }

  AutocompleteInput CreateInput(PageClass::PageClassification page_class,
                                FocusType focus_type) {
    AutocompleteInput input(std::u16string(), page_class,
                            TestSchemeClassifier());
    input.set_focus_type(focus_type);
    return input;
  }

  query_tiles::Tile CreateTile(std::string label, bool with_image = true) {
    query_tiles::Tile t;
    t.id = label + " ID";
    t.query_text = label;
    t.display_text = label + " display text";
    t.accessibility_text = label + " accessibility";

    if (with_image) {
      t.image_metadatas.emplace_back(GURL("https://image.site/" + label));
    }

    t.search_params.emplace_back("cgi_param=" + label);
    return t;
  }

 protected:
  FakeAutocompleteProviderClient client_;
  testing::StrictMock<MockAutocompleteListener> listener_;
  raw_ptr<MockTileService> tile_service_;
  raw_ptr<TemplateURLService> url_service_;
  scoped_refptr<QueryTileProvider> provider_;
  query_tiles::GetTilesCallback tile_callback_;
};

TEST_F(QueryTileProviderTest, IsAllowedInContext_ByPageClassAndFocusType) {
  struct {
    std::string_view name;
    PageClass::PageClassification page_class;
    FocusType focus_type;
    bool expected_result;
  } tests[] = {
      {"NTP / Zero Prefix", PageClass::NTP, FocusType::INTERACTION_FOCUS, true},
      {"NTP / Typed", PageClass::NTP, FocusType::INTERACTION_DEFAULT, false},
      {"Widget / Zero Prefix", PageClass::ANDROID_SHORTCUTS_WIDGET,
       FocusType::INTERACTION_FOCUS, false},
      {"Widget / Typed", PageClass::ANDROID_SHORTCUTS_WIDGET,
       FocusType::INTERACTION_DEFAULT, false},
      {"SRP / Zero Prefix",
       PageClass::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
       FocusType::INTERACTION_CLOBBER, false},
      {"SRP / Typed", PageClass::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
       FocusType::INTERACTION_DEFAULT, false},
      {"Web / Zero Prefix", PageClass::OTHER, FocusType::INTERACTION_CLOBBER,
       false},
      {"Web / Typed", PageClass::OTHER, FocusType::INTERACTION_DEFAULT, false},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.name);
    auto input = CreateInput(test.page_class, test.focus_type);
    ASSERT_EQ(test.expected_result, provider_->IsAllowedInContext(input));
  }
}

TEST_F(QueryTileProviderTest, IsAllowedInContext_BySearchEngine) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("DSE is Google");
    EXPECT_TRUE(search::DefaultSearchProviderIsGoogle(url_service_));
    EXPECT_TRUE(provider_->IsAllowedInContext(input));
  }

  {
    SCOPED_TRACE("DSE is not Google");
    // Create a new TemplateURL and set it as default.
    auto template_url = std::make_unique<TemplateURL>(TemplateURLData());
    auto* raw_template_url = template_url.get();
    url_service_->Add(std::move(template_url));
    url_service_->SetUserSelectedDefaultSearchProvider(raw_template_url);

    EXPECT_FALSE(search::DefaultSearchProviderIsGoogle(url_service_));
    EXPECT_FALSE(provider_->IsAllowedInContext(input));
  }
}

TEST_F(QueryTileProviderTest, IsAllowedInContext_ByIncognitoState) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Non-Incognito mode");
    EXPECT_CALL(client_, IsOffTheRecord()).WillOnce(testing::Return(false));
    ASSERT_TRUE(provider_->IsAllowedInContext(input));
  }

  {
    SCOPED_TRACE("Incognito mode");
    EXPECT_CALL(client_, IsOffTheRecord()).WillOnce(testing::Return(true));
    ASSERT_FALSE(provider_->IsAllowedInContext(input));
  }
}

TEST_F(QueryTileProviderTest, IsAllowedInContext_BySuggestEnabledState) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Search Suggestions Enabled");
    EXPECT_CALL(client_, SearchSuggestEnabled())
        .WillOnce(testing::Return(true));
    ASSERT_TRUE(provider_->IsAllowedInContext(input));
  }

  {
    SCOPED_TRACE("Search Suggestions Disabled");
    EXPECT_CALL(client_, SearchSuggestEnabled())
        .WillOnce(testing::Return(false));
    ASSERT_FALSE(provider_->IsAllowedInContext(input));
  }
}

TEST_F(QueryTileProviderTest, StartPrefetch) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));

  {
    SCOPED_TRACE(
        "Eligible context: first call to StartPrefetch retrieves tiles");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
  }

  {
    SCOPED_TRACE(
        "Eligible context: subsequent call to StartPrefetch retrieves tiles");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
  }

  input = CreateInput(PageClass::NTP, FocusType::INTERACTION_DEFAULT);
  ASSERT_FALSE(provider_->IsAllowedInContext(input));
  {
    SCOPED_TRACE("Non-eligible context: call to StartPrefetch does nothing");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(0);
    provider_->StartPrefetch(input);
  }
}

TEST_F(QueryTileProviderTest, Start_NonEligibleContext) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_DEFAULT);
  ASSERT_FALSE(provider_->IsAllowedInContext(input));
  EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(0);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
}

TEST_F(QueryTileProviderTest, StartPrefetch_Start) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));
  EXPECT_EQ(0u, provider_->matches().size());
  EXPECT_EQ(0u, provider_->tiles_.size());

  {
    SCOPED_TRACE("StartPrefetch caches top level tiles asynchronously.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
    // Emit two tiles. Expect cached tiles but no matches and no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    std::move(tile_callback_).Run({CreateTile("News"), CreateTile("Movies")});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    SCOPED_TRACE("Start serves prefetched tiles synchronously.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(0);
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(1);
    provider_->Start(input, false);
    EXPECT_EQ(2u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }
}

TEST_F(QueryTileProviderTest, StartPrefetch_CacheExpirationTest) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));
  EXPECT_EQ(0u, provider_->matches().size());
  EXPECT_EQ(0u, provider_->tiles_.size());
  base::ScopedMockClockOverride clock;

  {
    // This scenario should request new QueryTiles because we have not requested
    // them before.
    SCOPED_TRACE("StartPrefetch with no previous QueryTile result.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
    // Emit two tiles. Expect cached tiles but no matches and no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    // Report no tiles.
    std::move(tile_callback_).Run({});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(0u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    // This scenario should re-request QueryTiles.
    // We have made an attempt previously, but it yielded no results.
    SCOPED_TRACE("StartPrefetch with Empty previous QueryTile result.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
    // Emit two tiles. Expect cached tiles but no matches and no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    // Report no tiles.
    std::move(tile_callback_).Run({CreateTile("News"), CreateTile("Movies")});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    // This scenario should not request QueryTiles.
    // We have just received a reply from the server and it's not empty.
    SCOPED_TRACE("StartPrefetch with Empty previous QueryTile result.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(0);
    provider_->StartPrefetch(input);
    // Emit two tiles. Expect cached tiles but no matches and no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    // Observe that we still have our tiles.
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    // This scenario should re-request QueryTiles.
    // We push clock forward, and the logic should detect expiration.
    SCOPED_TRACE("StartPrefetch with Default Expired previous result.");

    // Advance the clock by 9 hours. This should land us past the default
    // expiration age.
    clock.Advance(base::Hours(9));

    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
    // Emit two tiles. Expect cached tiles but no matches and no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    // Report no tiles.
    EXPECT_TRUE(tile_callback_.MaybeValid());
    std::move(tile_callback_).Run({CreateTile("Coffee")});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(1u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    SCOPED_TRACE("StartPrefetch with Custom Expired previous result.");

    base::test::ScopedFeatureList features;
    features.InitAndEnableFeatureWithParameters(
        omnibox::kQueryTilesInZPSOnNTP, {{"QueryTilesMaxCacheAgeHours", "2"}});

    // Advance the clock by 9 hours. This should land us past the default
    // expiration age.
    clock.Advance(base::Hours(2));
    clock.Advance(base::Minutes(1));

    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->StartPrefetch(input);
    // Emit two tiles. Expect cached tiles but no matches and no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    // Report no tiles.
    ASSERT_TRUE(tile_callback_);
    std::move(tile_callback_).Run({CreateTile("Rick"), CreateTile("Morty")});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }
}

TEST_F(QueryTileProviderTest, Start_Stop_Start) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));
  EXPECT_EQ(0u, provider_->matches().size());
  EXPECT_EQ(0u, provider_->tiles_.size());

  {
    SCOPED_TRACE("First call to Start retrieves tiles asynchrnously.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    // Expect no matches, and no callbacks. The request is in-flight.
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(0);
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
  }

  {
    SCOPED_TRACE("Tiles retrieved asynchronously notify listener.");
    // Fulfill the request. Expect notification.
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(1);
    std::move(tile_callback_).Run({CreateTile("News"), CreateTile("Movies")});
    EXPECT_EQ(2u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    SCOPED_TRACE("Stop clears AutocompleteMatches, but keeps cached Tiles");
    provider_->Stop(true, false);
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }

  {
    SCOPED_TRACE("Subsequent calls to Start serve cached tiles synchronously.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(0);
    // Expect results to be served synchronously this time.
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(1);
    provider_->Start(input, false);
    EXPECT_EQ(2u, provider_->matches().size());
    EXPECT_EQ(2u, provider_->tiles_.size());
    EXPECT_TRUE(provider_->done());
  }
}

TEST_F(QueryTileProviderTest, Start_PreviousResultsAreRejected) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));
  EXPECT_EQ(0u, provider_->matches().size());
  query_tiles::GetTilesCallback callback1;
  query_tiles::GetTilesCallback callback2;

  {
    SCOPED_TRACE("Start(1) generates no matches before callback.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->Start(input, false);
    callback1 = std::move(tile_callback_);
    // Emit no tiles, expect no matches.
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_FALSE(provider_->done());
  }

  {
    SCOPED_TRACE("Start(2) generates no matches before callback.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->Start(input, false);
    callback2 = std::move(tile_callback_);
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_FALSE(provider_->done());
  }

  {
    SCOPED_TRACE("Callback(1) results should be discarded.");
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    std::move(callback1).Run({CreateTile("News")});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_FALSE(provider_->done());
  }

  {
    SCOPED_TRACE("Callback(2) results should be accepted.");
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(1);
    std::move(callback2).Run({CreateTile("Movies")});
    EXPECT_EQ(1u, provider_->matches().size());
    EXPECT_TRUE(provider_->done());
  }
}

TEST_F(QueryTileProviderTest, Start_LateResultsAreRejected) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));
  EXPECT_EQ(0u, provider_->matches().size());

  {
    SCOPED_TRACE("Start generates no matches.");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->Start(input, false);
    // Emit no tiles, expect no matches.
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_FALSE(provider_->done());
  }

  // Simulate expiration.
  provider_->Stop(false, true);
  EXPECT_TRUE(provider_->done());

  {
    SCOPED_TRACE("Late Tile response generates no matches.");
    // Observe no notifications.
    EXPECT_CALL(listener_, OnProviderUpdate(_, provider_.get())).Times(0);
    std::move(tile_callback_).Run({CreateTile("News")});
    EXPECT_EQ(0u, provider_->matches().size());
    EXPECT_TRUE(provider_->done());
  }
}

TEST_F(QueryTileProviderTest, BuildSuggestions_WithTiles) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));

  {
    SCOPED_TRACE("Create AutocompleteMatch objects from Query Tiles");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->Start(input, false);

    // Emit two tiles. Expect two matches and a notification about new matches.
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(1);
    std::move(tile_callback_).Run({CreateTile("News"), CreateTile("Movies")});
    EXPECT_EQ(2u, provider_->matches().size());
  }

  {
    SCOPED_TRACE("Verify constructed AutocompleteMatch for Tile#0");
    const auto& match = provider_->matches()[0];
    EXPECT_EQ(u"News display text", match.contents);
    EXPECT_EQ(u"News", match.fill_into_edit);
    EXPECT_EQ("https://image.site/News", match.image_url.spec());
    EXPECT_EQ(u"google.com", match.keyword);
    EXPECT_EQ("cgi_param=News",
              match.search_terms_args->additional_query_params);
    EXPECT_TRUE(url_service_->IsSearchResultsPageFromDefaultSearchProvider(
        match.destination_url));
    auto terms = url_service_->ExtractSearchMetadata(match.destination_url);
    EXPECT_EQ(u"news", terms->search_terms);
  }

  {
    SCOPED_TRACE("Verify constructed AutocompleteMatch for Tile#1");
    const auto& match = provider_->matches()[1];
    EXPECT_EQ(u"Movies display text", match.contents);
    EXPECT_EQ(u"Movies", match.fill_into_edit);
    EXPECT_EQ("https://image.site/Movies", match.image_url.spec());
    EXPECT_EQ(u"google.com", match.keyword);
    EXPECT_EQ("cgi_param=Movies",
              match.search_terms_args->additional_query_params);
    EXPECT_TRUE(url_service_->IsSearchResultsPageFromDefaultSearchProvider(
        match.destination_url));
    auto terms = url_service_->ExtractSearchMetadata(match.destination_url);
    EXPECT_EQ(u"movies", terms->search_terms);
  }
}

TEST_F(QueryTileProviderTest, BuildSuggestions_WithoutTiles) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));

  EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
  provider_->Start(input, false);

  // Emit no tiles. Expect no matches and notification about no new matches.
  EXPECT_CALL(listener_, OnProviderUpdate(false, provider_.get())).Times(1);
  std::move(tile_callback_).Run({});
  EXPECT_EQ(0u, provider_->matches().size());
}

TEST_F(QueryTileProviderTest, BuildSuggestions_WithNoImageURL) {
  auto input = CreateInput(PageClass::NTP, FocusType::INTERACTION_FOCUS);
  ASSERT_TRUE(provider_->IsAllowedInContext(input));

  {
    SCOPED_TRACE("Create AutocompleteMatch object from Query Tiles");
    EXPECT_CALL(*tile_service_, GetQueryTiles(_)).Times(1);
    provider_->Start(input, false);

    // Emit tile with no image metadata.
    EXPECT_CALL(listener_, OnProviderUpdate(true, provider_.get())).Times(1);
    std::move(tile_callback_).Run({CreateTile("News", false)});
    EXPECT_EQ(1u, provider_->matches().size());
  }

  {
    SCOPED_TRACE("Verify constructed AutocompleteMatch for tile with no Image");
    const auto& match = provider_->matches()[0];
    EXPECT_TRUE(match.image_url.is_empty());
  }
}
