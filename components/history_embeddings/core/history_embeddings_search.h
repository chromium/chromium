// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_CORE_HISTORY_EMBEDDINGS_SEARCH_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_CORE_HISTORY_EMBEDDINGS_SEARCH_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/core/answerer.h"
#include "components/history_embeddings/core/vector_database.h"

namespace history_embeddings {

// Counts the # of ' ' vanilla-space characters in `s`.
size_t CountWords(const std::string& s);

// A single item that forms part of a search result; combines metadata found in
// the history embeddings database with additional info from history database.
struct ScoredUrlRow {
  explicit ScoredUrlRow(ScoredUrl scored_url);
  ScoredUrlRow(const ScoredUrlRow&);
  ScoredUrlRow(ScoredUrlRow&&);
  ~ScoredUrlRow();
  ScoredUrlRow& operator=(const ScoredUrlRow&);
  ScoredUrlRow& operator=(ScoredUrlRow&&);

  // Returns the highest scored passage in `passages_embeddings`.
  std::string GetBestPassage() const;

  // Finds the indices of the top scores, ordered descending by score.
  // This is useful for selecting a subset of `passages_embeddings` for use as
  // answerer context. The size of the returned vector will be at least
  // `min_count` provided there is sufficient data available. The
  // `min_word_count` parameter will also be used to ensure the
  // passages for returned indices have word counts adding up to at
  // least this minimum.
  std::vector<size_t> GetBestScoreIndices(size_t min_count,
                                          size_t min_word_count) const;

  // Basic scoring and history data for this URL.
  ScoredUrl scored_url;
  history::URLRow row;
  bool is_url_known_to_sync = false;

  // All passages and embeddings for this URL (i.e. not a partial set).
  UrlData passages_embeddings;

  // All scores against the query for `passages_embeddings`.
  std::vector<float> scores;
};

struct SearchResult {
  SearchResult();
  SearchResult(SearchResult&&);
  ~SearchResult();
  SearchResult& operator=(SearchResult&&);

  // Explicit copy only, since the `answerer_result` contains a log entry.
  // This should only be called if `answerer_result` is not populated with
  // a log entry yet, for example after initial search and before answering.
  SearchResult Clone();

  // Returns true if this search result is related to the given `other`
  // result returned by HistoryEmbeddingsService::Search (same session/query).
  bool IsContinuationOf(const SearchResult& other);

  // Gets the answer text from within the `answerer_result`.
  const std::string& AnswerText() const;

  // Finds the index in `scored_url_rows` that has the URL selected by the
  // `answerer_result`, indicating where the answer came from.
  size_t AnswerIndex() const;

  // Session ID to associate query with answers.
  std::string session_id;

  // Keep context for search parameters requested, to make logging easier.
  std::string query;
  std::optional<base::Time> time_range_start;
  size_t count = 0;
  SearchParams search_params;

  // The actual search result data. Note that the size of this vector will
  // not necessarily match the above requested `count`.
  std::vector<ScoredUrlRow> scored_url_rows;

  // This may be empty for initial embeddings search results, as the answer
  // isn't ready yet. When the answerer finishes work, a second search
  // result is provided with this answer filled.
  AnswererResult answerer_result;
};

using SearchResultCallback = base::RepeatingCallback<void(SearchResult)>;

class HistoryEmbeddingsSearch {
 public:
  virtual ~HistoryEmbeddingsSearch() = default;

  // Finds the top `count` URL visit info entries nearest to `query`. Passes the
  // results to `callback` when search completes, whether successfully or not.
  // Search will be narrowed to a time range if `time_range_start` is provided.
  // In that case, the start of the time range is inclusive and the end is
  // unbounded. Practically, this can be thought of as [start, now) but now
  // isn't fixed.
  // The `callback` may be called a second time with another search result
  // containing an answer, only if `skip_answering` is false and an answer is
  // successfully generated. This two-phase result callback scheme lets callers
  // receive initial search results without having to wait longer for answers.
  // The `previous_search_result` may be nullptr to signal the beginning of a
  // completely new search session; if it is non-null and the session_id is set,
  // the new session_id is set based on the previous to indicate a continuing
  // search session.
  // Returns a stub result that can be used to detect if a later published
  // SearchResult instance is related to this search.
  virtual SearchResult Search(SearchResult* previous_search_result,
                              std::string query,
                              std::optional<base::Time> time_range_start,
                              size_t count,
                              bool skip_answering,
                              SearchResultCallback callback) = 0;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_CORE_HISTORY_EMBEDDINGS_SEARCH_H_
