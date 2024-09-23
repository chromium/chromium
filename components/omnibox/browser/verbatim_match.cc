// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/verbatim_match.h"

#include "base/containers/fixed_flat_set.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/common/url_constants.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
#if BUILDFLAG(IS_ANDROID)
// Note: explicitly exclude schemes that may be used to execute Javascript code
// snippet in the context of the current page on mobile devices.
constexpr auto kNavigableSchemes = base::MakeFixedFlatSet<std::string_view>(
    {url::kHttpScheme, url::kHttpsScheme, url::kAboutScheme,
     content::kChromeUIScheme});
#endif
}  // namespace

AutocompleteMatch VerbatimMatchForURL(
    AutocompleteProvider* provider,
    AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    const GURL& destination_url,
    const std::u16string& destination_description,
    int verbatim_relevance) {
  AutocompleteMatch match;
  // If the caller is a provider and already knows where the verbatim match
  // should go, construct the match directly, don't call Classify() on the
  // input. Classify() runs all providers' synchronous passes. Some providers
  // such as HistoryQuick can have a slow synchronous pass on some inputs.
  if (provider != nullptr && destination_url.is_valid()) {
    match =
        VerbatimMatchForInput(provider, client, input, destination_url,
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

AutocompleteMatch VerbatimMatchForInput(AutocompleteProvider* provider,
                                        AutocompleteProviderClient* client,
                                        const AutocompleteInput& input,
                                        const GURL& destination_url,
                                        bool trim_default_scheme) {
  AutocompleteMatch match(provider, 0, false,
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED);

  if (destination_url.is_valid()) {
    match.destination_url = destination_url;
    // If the input explicitly contains "http://" or "https://", callers must
    // set |trim_default_scheme| to false. Otherwise, |trim_default_scheme| may
    // be either true or false.
    if (input.added_default_scheme_to_typed_url()) {
      DCHECK(!(trim_default_scheme &&
               AutocompleteInput::HasHTTPSScheme(input.text())));
    } else {
      DCHECK(!(trim_default_scheme &&
               AutocompleteInput::HasHTTPScheme(input.text())));
    }
    const url_formatter::FormatUrlType format_type =
        input.added_default_scheme_to_typed_url()
            ? url_formatter::kFormatUrlOmitHTTPS
            : url_formatter::kFormatUrlOmitHTTP;

    std::u16string display_string(url_formatter::FormatUrl(
        destination_url, url_formatter::kFormatUrlOmitDefaults & ~format_type,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    if (trim_default_scheme) {
      AutocompleteProvider::TrimSchemePrefix(
          &display_string, input.added_default_scheme_to_typed_url());
    }
    match.fill_into_edit =
        AutocompleteInput::FormattedStringWithEquivalentMeaning(
            destination_url, display_string, client->GetSchemeClassifier(),
            nullptr);
    // The what-you-typed match is generally only allowed to be default for
    // URL inputs or when there is no default search provider.  (It's also
    // allowed to be default for UNKNOWN inputs where the destination is a known
    // intranet site.  In this case, |allowed_to_be_default_match| is revised in
    // FixupExactSuggestion().)
    const bool has_default_search_provider =
        client->GetTemplateURLService() &&
        client->GetTemplateURLService()->GetDefaultSearchProvider();
    match.allowed_to_be_default_match =
        (input.type() == metrics::OmniboxInputType::URL) ||
        !has_default_search_provider;
#if BUILDFLAG(IS_ANDROID)
    // Disallow non-navigable schemes to be default. This prevents javascript:
    // snippets from being accidentally executed upon paste, refine, edit, etc.
    match.allowed_to_be_default_match &=
        kNavigableSchemes.contains(destination_url.scheme());
#endif

    // NOTE: Don't set match.inline_autocompletion to something non-empty here;
    // it's surprising and annoying.

    // Try to highlight "innermost" match location.  If we fix up "w" into
    // "www.w.com", we want to highlight the fifth character, not the first.
    // This relies on match.destination_url being the non-prefix-trimmed version
    // of match.contents.
    match.contents = display_string;

    TermMatches termMatches = {{0, 0, input.text().length()}};
    match.contents_class = ClassifyTermMatches(
        termMatches, match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);

    // Only set scoring signals for eligible matches.
    if (match.IsMlSignalLoggingEligible()) {
      if (!match.scoring_signals) {
        match.scoring_signals =
            std::make_optional<::metrics::OmniboxScoringSignals>();
      }
      match.scoring_signals->set_is_verbatim(true);
    }
  }

  return match;
}
