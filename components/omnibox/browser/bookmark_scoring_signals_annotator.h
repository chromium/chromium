// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_SCORING_SIGNALS_ANNOTATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_SCORING_SIGNALS_ANNOTATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class AutocompleteResult;

namespace bookmarks {
class BookmarkModel;
}  //  namespace bookmarks

// Annotates URL suggestions in the autocomplete result with signals derived
// from bookmarks, including: `total_bookmark_title_match_length`,
// `first_bookmark_title_match_position`, and `num_bookmarks_of_url`.
//
// Synchronously looks up bookmarks for URL suggestions from the in-memory
// bookmark model db.
class BookmarkScoringSignalsAnnotator
    : public AutocompleteScoringSignalsAnnotator {
 public:
  explicit BookmarkScoringSignalsAnnotator(AutocompleteProviderClient* client);
  BookmarkScoringSignalsAnnotator(const BookmarkScoringSignalsAnnotator&) =
      delete;
  BookmarkScoringSignalsAnnotator& operator=(
      const BookmarkScoringSignalsAnnotator&) = delete;
  ~BookmarkScoringSignalsAnnotator() override = default;

  // Annotate the URL suggestions in the autocomplete result.
  void AnnotateResult(const AutocompleteInput& input,
                      AutocompleteResult* result) override;

 private:
  // Not owned. Null when `AutocompleteProviderClient` is null.
  //
  // If null, the annotator does not annotate any suggestions.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_SCORING_SIGNALS_ANNOTATOR_H_
