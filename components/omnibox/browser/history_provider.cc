// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_provider.h"

#include <string>

#include "base/strings/string_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"

using bookmarks::BookmarkModel;

void HistoryProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(done_);
  DCHECK(client_);
  DCHECK(match.deletable);

  history::HistoryService* const history_service = client_->GetHistoryService();

  // Delete the underlying URL along with all its visits from the history DB.
  // The resulting HISTORY_URLS_DELETED notification will also cause all caches
  // and indices to drop any data they might have stored pertaining to the URL.
  DCHECK(history_service);
  DCHECK(match.destination_url.is_valid());
  history_service->DeleteURLs({match.destination_url});

  DeleteMatchFromMatches(match);
}

// static
ACMatchClassifications HistoryProvider::SpansFromTermMatch(
    const TermMatches& matches,
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style non_match_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  return ClassifyTermMatches(matches, text_length,
                             ACMatchClassification::MATCH | non_match_style,
                             non_match_style);
}

HistoryProvider::HistoryProvider(AutocompleteProvider::Type type,
                                 AutocompleteProviderClient* client)
    : AutocompleteProvider(type), client_(client) {}

HistoryProvider::~HistoryProvider() {}

void HistoryProvider::DeleteMatchFromMatches(const AutocompleteMatch& match) {
  bool found = false;
  BookmarkModel* bookmark_model = client_->GetBookmarkModel();
  for (auto i(matches_.begin()); i != matches_.end(); ++i) {
    if (i->destination_url == match.destination_url && i->type == match.type) {
      found = true;
      if ((i->type == AutocompleteMatchType::URL_WHAT_YOU_TYPED) ||
          (bookmark_model &&
           bookmark_model->IsBookmarked(i->destination_url))) {
        // We can't get rid of What-You-Typed or Bookmarked matches,
        // but we can make them look like they have no backing data.
        i->deletable = false;
        i->description.clear();
        i->description_class.clear();
      } else {
        matches_.erase(i);
      }
      break;
    }
  }
  DCHECK(found) << "Asked to delete a URL that isn't in our set of matches";
}
