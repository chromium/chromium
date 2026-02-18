// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/core/history_embeddings_search.h"

#include <algorithm>
#include <tuple>

#include "base/check.h"

namespace history_embeddings {

size_t CountWords(const std::string& s) {
  if (s.empty()) {
    return 0;
  }
  size_t word_count = (s[0] == ' ') ? 0 : 1;
  for (size_t i = 1; i < s.length(); i++) {
    if (s[i] != ' ' && s[i - 1] == ' ') {
      word_count++;
    }
  }
  return word_count;
}

ScoredUrlRow::ScoredUrlRow(ScoredUrl scored_url)
    : scored_url(std::move(scored_url)),
      passages_embeddings(this->scored_url.url_id,
                          this->scored_url.visit_id,
                          this->scored_url.visit_time) {}
ScoredUrlRow::ScoredUrlRow(const ScoredUrlRow&) = default;
ScoredUrlRow::ScoredUrlRow(ScoredUrlRow&&) = default;
ScoredUrlRow::~ScoredUrlRow() = default;
ScoredUrlRow& ScoredUrlRow::operator=(const ScoredUrlRow&) = default;
ScoredUrlRow& ScoredUrlRow::operator=(ScoredUrlRow&&) = default;

std::string ScoredUrlRow::GetBestPassage() const {
  CHECK(passages_embeddings.passages.passages_size() != 0);
  size_t best_index = GetBestScoreIndices(1, 0).front();
  CHECK_LT(best_index,
           static_cast<size_t>(passages_embeddings.passages.passages_size()));
  return passages_embeddings.passages.passages(best_index);
}

std::vector<size_t> ScoredUrlRow::GetBestScoreIndices(
    size_t min_count,
    size_t min_word_count) const {
  using ScoreWordsIndex =
      std::tuple</*score=*/float, /*word_count=*/size_t, /*index=*/size_t>;
  std::vector<ScoreWordsIndex> data;
  data.reserve(scores.size());
  for (size_t i = 0; i < scores.size(); i++) {
    // The word count could be calculated from the passage directly, but
    // since it has already been calculated before, use the value stored
    // with the embedding for efficiency.
    data.emplace_back(
        scores[i], passages_embeddings.embeddings[i].GetPassageWordCount(), i);
  }

  // Sort tuples naturally, descending, so that highest scores come first.
  // Note that if scores are exactly equal, the longer passage is preferred,
  // and the index comes last to break any remaining ties.
  std::sort(data.begin(), data.end(), std::greater());

  size_t word_sum = 0;
  std::vector<size_t> indices;
  indices.reserve(min_count);
  for (const ScoreWordsIndex& item : data) {
    if (indices.size() >= min_count && word_sum >= min_word_count) {
      break;
    }
    indices.push_back(std::get<2>(item));
    word_sum += std::get<1>(item);
  }
  return indices;
}

SearchResult::SearchResult() = default;
SearchResult::SearchResult(SearchResult&&) = default;
SearchResult::~SearchResult() = default;
SearchResult& SearchResult::operator=(SearchResult&&) = default;

SearchResult SearchResult::Clone() {
  // Cannot copy `answerer_result`; it should not have substance.
  CHECK(!answerer_result.log_entry);

  SearchResult clone;
  clone.session_id = session_id;
  clone.query = query;
  clone.time_range_start = time_range_start;
  clone.count = count;
  clone.search_params = search_params;
  clone.scored_url_rows = scored_url_rows;
  return clone;
}

bool SearchResult::IsContinuationOf(const SearchResult& other) {
  return session_id == other.session_id && query == other.query;
}

const std::string& SearchResult::AnswerText() const {
  return answerer_result.answer.text();
}

size_t SearchResult::AnswerIndex() const {
  for (size_t i = 0; i < scored_url_rows.size(); i++) {
    // Note, the spec isn't used because there may be minor differences between
    // the strings, for example "http://other.com" versus "http://other.com/".
    if (scored_url_rows[i].row.url() == GURL(answerer_result.url)) {
      return i;
    }
  }
  return 0;
}

}  // namespace history_embeddings
