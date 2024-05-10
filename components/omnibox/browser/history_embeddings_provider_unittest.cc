// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_embeddings_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
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

 private:
  ~FakeHistoryEmbeddingsProvider() override = default;
};

class HistoryEmbeddingsProviderTest : public testing::Test {
 protected:
  HistoryEmbeddingsProviderTest() {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    history_embeddings_provider_ =
        new FakeHistoryEmbeddingsProvider(client_.get());
  }

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeHistoryEmbeddingsProvider> history_embeddings_provider_;
};

TEST_F(HistoryEmbeddingsProviderTest,
       OnReceivedSearchResult_CreatesAutocompleteMatches) {
  history_embeddings_provider_->OnReceivedSearchResult(
      u"query", {
                    CreateScoredUrlRow(.5, "https://url.com/", u"title"),
                });

  ASSERT_EQ(history_embeddings_provider_->matches_.size(), 1u);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].provider.get(),
            history_embeddings_provider_.get());
  EXPECT_EQ(history_embeddings_provider_->matches_[0].relevance, 500);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].deletable, false);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].type,
            AutocompleteMatchType::HISTORY_BODY);
  EXPECT_EQ(history_embeddings_provider_->matches_[0].destination_url.spec(),
            "https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].contents, u"title");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].description,
            u"https://url.com/");
  EXPECT_EQ(history_embeddings_provider_->matches_[0].keyword, u"");
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      history_embeddings_provider_->matches_[0].transition,
      ui::PAGE_TRANSITION_TYPED));
}
