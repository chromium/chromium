// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_VERBATIM_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_VERBATIM_MATCH_H_

#include <string>

#include "components/omnibox/browser/autocomplete_input.h"
#include "url/gurl.h"

struct AutocompleteMatch;
class AutocompleteProvider;
class AutocompleteProviderClient;

// Returns a verbatim match for input.text() with a relevance of
// |verbatim_relevance|. If |verbatim_relevance| is negative then a default
// value is used. If the desired |destination_url| is already known and a
// |history_url_provider| is also provided, use |destination_description| as the
// description. Providing |history_url_provider| also may be more efficient (see
// implementation for details) than the default code path.
// input.text() must not be empty.
AutocompleteMatch VerbatimMatchForURL(
    AutocompleteProvider* provider,
    AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    const GURL& destination_url,
    const std::u16string& destination_description,
    int verbatim_relevance);

// Returns a match representing a navigation to |destination_url|, highlighted
// appropriately against |input|.  |trim_default_scheme| controls whether the
// match's |fill_into_edit| and |contents| should have the scheme (http or
// https only) stripped off, and should not be set to true if the user's
// original input contains the scheme. The default scheme is https if |input|
// is upgraded to https, otherwise it's http.
// NOTES: This does not set the relevance of the returned match, as different
//        callers want different behavior. Callers must set this manually.
//        This function should only be called on the UI thread.
AutocompleteMatch VerbatimMatchForInput(AutocompleteProvider* provider,
                                        AutocompleteProviderClient* client,
                                        const AutocompleteInput& input,
                                        const GURL& destination_url,
                                        bool trim_http);

#endif  // COMPONENTS_OMNIBOX_BROWSER_VERBATIM_MATCH_H_
