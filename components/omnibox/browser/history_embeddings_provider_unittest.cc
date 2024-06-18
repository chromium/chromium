// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_embeddings_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/history_embeddings/mock_history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/template_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
history_embeddings::ScoredUrlRow CreateScoredUrlRow(
    float score,
    const std::string& url,
    const std::u16string& title) {
  history_embeddings::ScoredUrlRow scored_url_row(
      history_embeddings::ScoredUrl(0, 0, {}, score, 0u, {}));
  scored_url_row.row = history::URLRow{GURL{url}};
  scored_url_row.row.set_title(title);
  return scored_url_row;
}
}  // namespace

class FakeHistoryEmbeddingsProvider : public HistoryEmbeddingsProvider {
 public:
  using HistoryEmbeddingsProvider::HistoryEmbeddingsProvider;
  using HistoryEmbeddingsProvider::OnReceivedSearchResult;

  using HistoryEmbeddingsProvider::matches_;
  using HistoryEmbeddingsProvider::starter_pack_engine_;

 private:
  ~FakeHistoryEmbeddingsProvider() override = default;
};

class HistoryEmbeddingsProviderTest : public testing::Test {
 protected:
  HistoryEmbeddingsProviderTest() {
    history_embeddings_service_ =
        std::make_unique<history_embeddings::MockHistoryEmbeddingsService>();

    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    client_->set_history_embeddings_service(history_embeddings_service_.get());

    history_embeddings_provider_ =
        new FakeHistoryEmbeddingsProvider(client_.get());
  }

  std::unique_ptr<history_embeddings::MockHistoryEmbeddingsService>
      history_embeddings_service_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeHistoryEmbeddingsProvider> history_embeddings_provider_;
};

TEST_F(HistoryEmbeddingsProviderTest, Start) {
  AutocompleteInput input(u"query query query",
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  // When the feature is disabled, should early exit.
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      history_embeddings::kHistoryEmbeddings);
  EXPECT_CALL(*history_embeddings_service_,
              Search(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  history_embeddings_provider_->Start(input, false);

  base::test::ScopedFeatureList enabled_feature{
      history_embeddings::kHistoryEmbeddings};

  // When the feature is enabled, should call `Search()`.
  EXPECT_CALL(
      *history_embeddings_service_,
      Search("query query query", std::optional<base::Time>{}, 3u, testing::_))
      .Times(1);
  history_embeddings_provider_->Start(input, false);
}

TEST_F(HistoryEmbeddingsProviderTest, Stop) {
  history_embeddings_provider_->Stop(false, false);
  // `Stop()` is not implemented yet. Just expect it to not crash.
}

TEST_F(HistoryEmbeddingsProviderTest, DeleteMatch) {
  history_embeddings_provider_->DeleteMatch({});
  // `DeleteMatch()` is not implemented yet. Just expect it to not crash.
}

TEST_F(HistoryEmbeddingsProviderTest,
       OnReceivedSearchResult_CreatesAutocompleteMatches) {
  history_embeddings::SearchResult result;
  result.scored_url_rows = {
      CreateScoredUrlRow(.5, "https://url.com/", u"title"),
  };
  history_embeddings_provider_->OnReceivedSearchResult(u"query", result);

  ASSERT_EQ(history_embeddings_provider_->matches_.size(), 1u);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].provider.get(),
            history_embeddings_provider_.get());
  EXPECT_EQ(history_embeddings_provider_->matches_[0].relevance, 500);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].deletable, false);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].type,
            AutocompleteMatchType::HISTORY_EMBEDDINGS);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].destination_url.spec(),
            "https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].description, u"title");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].contents,
            u"https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].keyword, u"");
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      history_embeddings_provider_->matches_[0].transition,
      ui::PAGE_TRANSITION_TYPED));
}

TEST_F(HistoryEmbeddingsProviderTest,
       OnReceivedSearchResult_CreatesScopedAutocompleteMatches) {
  // Verifies autocomplete match is created correctly when the user is in
  // keyword mode.
  TemplateURLData template_url_data;
  template_url_data.SetShortName(u"shortname");
  template_url_data.SetKeyword(u"keyword");
  template_url_data.SetURL("https://url.com");
  TemplateURL template_url{template_url_data};
  history_embeddings_provider_->starter_pack_engine_ = &template_url;

  history_embeddings::SearchResult result;
  result.scored_url_rows = {
      CreateScoredUrlRow(.5, "https://url.com/", u"title"),
  };
  history_embeddings_provider_->OnReceivedSearchResult(u"query", result);

  ASSERT_EQ(history_embeddings_provider_->matches_.size(), 1u);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].provider.get(),
            history_embeddings_provider_.get());
  EXPECT_EQ(history_embeddings_provider_->matches_[0].relevance, 500);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].deletable, false);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].type,
            AutocompleteMatchType::HISTORY_EMBEDDINGS);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].destination_url.spec(),
            "https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].description, u"title");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].contents,
            u"https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].keyword, u"keyword");
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      history_embeddings_provider_->matches_[0].transition,
      ui::PAGE_TRANSITION_KEYWORD));
}
