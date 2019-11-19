// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/verbatim_match.h"

#include "base/logging.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "url/gurl.h"

AutocompleteMatch VerbatimMatchForURL(
    AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    const GURL& destination_url,
    const base::string16& destination_description,
    HistoryURLProvider* history_url_provider,
    int verbatim_relevance) {
  AutocompleteMatch match;
  // If the caller already knows where the verbatim match should go and has
  // provided a HistoryURLProvider to aid in its construction, construct the
  // match directly, don't call Classify() on the input.  Classify() runs all
  // providers' synchronous passes.  Some providers such as HistoryQuick can
  // have a slow synchronous pass on some inputs.
  if (history_url_provider && destination_url.is_valid()) {
    match = history_url_provider->SuggestExactInput(
        input,
        destination_url,
        !AutocompleteInput::HasHTTPScheme(input.text()));
    match.description = destination_description;
    if (!match.description.empty())
      match.description_class.push_back({0, ACMatchClassification::NONE});
  } else {
    client->Classify(input.text(), false, true,
                     input.current_page_classification(), &match, nullptr);
  }
  match.allowed_to_be_default_match = true;
  // The default relevance to use for relevance match. Should be greater than
  // all relevance matches returned by the ZeroSuggest server.
  const int kDefaultVerbatimRelevance = 1300;
  match.relevance =
      verbatim_relevance >= 0 ? verbatim_relevance : kDefaultVerbatimRelevance;
  return match;
}
