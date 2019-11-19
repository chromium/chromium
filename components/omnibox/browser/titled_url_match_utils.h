// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TITLED_URL_MATCH_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_TITLED_URL_MATCH_UTILS_H_

#include "base/strings/string16.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"

class AutocompleteInput;
class AutocompleteProvider;
class AutocompleteSchemeClassifier;
struct AutocompleteMatch;

namespace bookmarks {

// Compose an AutocompleteMatch based on |match| that has the match's URL and
// page title, type |type|, and relevance score |relevance|. |input| is used to
// compute the match's inline_autocompletion. |fixed_up_input_text| is used in
// that way as well; it's passed separately so this function doesn't have to
// compute it.
AutocompleteMatch TitledUrlMatchToAutocompleteMatch(
    const TitledUrlMatch& match,
    AutocompleteMatchType::Type type,
    int relevance,
    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input,
    const base::string16& fixed_up_input_text);

}  // namespace bookmarks

#endif  // COMPONENTS_OMNIBOX_BROWSER_TITLED_URL_MATCH_UTILS_H_
