// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_PROVIDER_H_

#include <stddef.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

namespace bookmarks {
class BookmarkModel;
struct TitledUrlMatch;
}

// This class is an autocomplete provider which quickly (and synchronously)
// provides autocomplete suggestions based on the titles of bookmarks. Page
// titles, URLs, and typed and visit counts of the bookmarks are not currently
// taken into consideration as those factors will have been used by the
// HistoryQuickProvider (HQP) while identifying suggestions.
//
// TODO(mrossetti): Propose a way to coordinate with the HQP the taking of typed
// and visit counts and last visit dates, etc. into consideration while scoring.
class BookmarkProvider : public AutocompleteProvider {
 public:
  explicit BookmarkProvider(AutocompleteProviderClient* client);

  BookmarkProvider(const BookmarkProvider&) = delete;
  BookmarkProvider& operator=(const BookmarkProvider&) = delete;

  // When |minimal_changes| is true short circuit any additional searching and
  // leave the previous matches for this provider unchanged, otherwise perform
  // a complete search for |input| across all bookmark titles.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // Sets the BookmarkModel for unit tests.
  void set_bookmark_model_for_testing(
      bookmarks::BookmarkModel* bookmark_model) {
    bookmark_model_ = bookmark_model;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkProviderTest, InlineAutocompletion);

  ~BookmarkProvider() override;

  // Performs the actual matching of |input| over the bookmarks and fills in
  // |matches_|.
  void DoAutocomplete(const AutocompleteInput& input);

  // Calculates the relevance score for |match|.
  int CalculateBookmarkMatchRelevance(
      const bookmarks::TitledUrlMatch& match) const;

  // Removes any URL matches for query parameter keys (if the matching word
  // starts immediately after a '?' or '&').
  void RemoveQueryParamKeyMatches(bookmarks::TitledUrlMatch& match);

  AutocompleteProviderClient* client_;
  bookmarks::BookmarkModel* bookmark_model_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BOOKMARK_PROVIDER_H_
