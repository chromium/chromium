// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_tile_provider.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

class QueryTileProviderTest : public testing::Test,
                              public AutocompleteProviderListener {
 public:
  QueryTileProviderTest();
  QueryTileProviderTest(const QueryTileProviderTest&) = delete;
  QueryTileProviderTest& operator=(const QueryTileProviderTest&) = delete;

  void SetUp() override {
    TemplateURLService* turl_model = client_->GetTemplateURLService();
    turl_model->Load();

    // Verify that Google is the default search provider.
    ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
              turl_model->GetDefaultSearchProvider()->GetEngineType(
                  turl_model->search_terms_data()));

    EXPECT_CALL(*client_, SearchSuggestEnabled())
        .WillRepeatedly(testing::Return(true));
  }

  AutocompleteInput CreateInput(const std::string& text) {
    AutocompleteInput input(base::ASCIIToUTF16(text),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    return input;
  }

 protected:
  // AutocompleteProviderListener overrides.
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {}

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<QueryTileProvider> provider_;
};

QueryTileProviderTest::QueryTileProviderTest()
    : client_(new FakeAutocompleteProviderClient()),
      provider_(new QueryTileProvider(client_.get(), this)) {}

// The following tests use FakeTileService which responds synchronously to the
// tile requests. That's why none of these tests wait for the callbacks to
// finish.
TEST_F(QueryTileProviderTest, TopLevelTilesShowOnZeroSuggest) {
  auto input = CreateInput(std::string());
  provider_->Start(input, false);
  RunUntilIdle();

  EXPECT_EQ(provider_->matches().size(), 1u);
  auto match = provider_->matches().front();
  EXPECT_EQ(match.type, AutocompleteMatchType::TILE_SUGGESTION);
  EXPECT_EQ(match.query_tiles.size(), 2u);
  EXPECT_EQ(match.query_tiles[0].display_text, "News");
  EXPECT_EQ(match.query_tiles[1].display_text, "Films");
  EXPECT_TRUE(provider_->matches().front().allowed_to_be_default_match);
}

TEST_F(QueryTileProviderTest, SubTilesForSelectedTile) {
  auto input = CreateInput("News");
  input.set_query_tile_id("1");
  provider_->Start(input, false);
  RunUntilIdle();

  EXPECT_EQ(provider_->matches().size(), 1u);
  auto match = provider_->matches().front();
  EXPECT_EQ(match.type, AutocompleteMatchType::TILE_SUGGESTION);
  EXPECT_EQ(match.query_tiles.size(), 1u);
  EXPECT_EQ(match.query_tiles[0].display_text, "India");
  EXPECT_TRUE(provider_->matches().front().allowed_to_be_default_match);
}

TEST_F(QueryTileProviderTest, OmniboxTextNotMatchingSelectedTile) {
  auto input = CreateInput("some text");
  input.set_query_tile_id("1");
  provider_->Start(input, false);
  RunUntilIdle();
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(QueryTileProviderTest, QueryTilesAreShownForURLInput) {
  std::string url("http://www.cnn.com");
  auto input = CreateInput(url);
  input.set_current_url(GURL(url));
  input.UpdateText(base::UTF8ToUTF16(url), 0, {});
  provider_->Start(input, false);
  RunUntilIdle();
  EXPECT_EQ(provider_->matches().size(), 1u);
  EXPECT_FALSE(provider_->matches().front().allowed_to_be_default_match);
}

TEST_F(QueryTileProviderTest, DefaultSearchProviderIsNotGoogle) {
  // Set a non-Google default search engine. We should disallow query tile
  // suggestions.
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  auto* google_search_provider = turl_model->GetDefaultSearchProvider();
  TemplateURLData data;
  data.SetURL("https://www.example.com/?q={searchTerms}");
  data.suggestions_url = "https://www.example.com/suggest/?q={searchTerms}";
  auto* other_search_provider =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(other_search_provider);

  auto input = CreateInput("");
  provider_->Start(input, false);
  RunUntilIdle();
  EXPECT_TRUE(provider_->matches().empty());

  // Restore Google as the default search provider. Now query tile suggestions
  // should be allowed.
  turl_model->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_search_provider));
  provider_->Start(input, false);
  RunUntilIdle();
  EXPECT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(provider_->matches().front().query_tiles.size(), 2u);
}
