// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider.h"

#include <algorithm>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_provider_type.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

AutocompleteProvider::AutocompleteProvider(AutocompleteProviderType type)
    : provider_max_matches_(OmniboxFieldTrial::GetProviderMaxMatches(type)),
      type_(type) {}

void AutocompleteProvider::AddListener(AutocompleteProviderListener* listener) {
  listeners_.push_back(listener);
}

void AutocompleteProvider::NotifyListeners(bool updated_matches) const {
  for (auto* listener : listeners_)
    listener->OnProviderUpdate(updated_matches, this);
}

void AutocompleteProvider::StartPrefetch(const AutocompleteInput& input) {
  DCHECK(!input.omit_asynchronous_matches());
}

void AutocompleteProvider::Stop(bool clear_cached_results,
                                bool due_to_user_inactivity) {
  done_ = true;
  if (clear_cached_results) {
    matches_.clear();
    suggestion_groups_map_.clear();
  }
}

// static
ACMatchClassifications AutocompleteProvider::ClassifyAllMatchesInString(
    const std::u16string& find_text,
    const std::u16string& text,
    const bool text_is_search_query,
    const ACMatchClassifications& original_class) {
  // TODO (manukh) Move this function to autocomplete_match_classification
  DCHECK(!find_text.empty());

  if (text.empty())
    return original_class;

  TermMatches term_matches = FindTermMatches(find_text, text);

  ACMatchClassifications classifications;
  if (text_is_search_query) {
    classifications = ClassifyTermMatches(term_matches, text.size(),
                                          ACMatchClassification::NONE,
                                          ACMatchClassification::MATCH);
  } else
    classifications = ClassifyTermMatches(term_matches, text.size(),
                                          ACMatchClassification::MATCH,
                                          ACMatchClassification::NONE);

  return AutocompleteMatch::MergeClassifications(original_class,
                                                 classifications);
}

void AutocompleteProvider::DeleteMatch(const AutocompleteMatch& match) {
  DLOG(WARNING) << "The AutocompleteProvider '" << GetName()
                << "' has not implemented DeleteMatch.";
}

void AutocompleteProvider::DeleteMatchElement(const AutocompleteMatch& match,
                                              size_t element_index) {
  DLOG(WARNING) << "The AutocompleteProvider '" << GetName()
                << "' has not implemented DeleteMatchElement.";
}

void AutocompleteProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
}

size_t AutocompleteProvider::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(matches_);
}

const char* AutocompleteProvider::GetName() const {
  return AutocompleteProviderTypeToString(type_);
}

metrics::OmniboxEventProto_ProviderType
AutocompleteProvider::AsOmniboxEventProviderType() const {
  return AutocompleteProviderTypeToOmniboxEventProviderType(type_);
}

AutocompleteProvider::~AutocompleteProvider() {
  Stop(false, false);
}

// static
AutocompleteProvider::FixupReturn AutocompleteProvider::FixupUserInput(
    const AutocompleteInput& input) {
  const std::u16string& input_text = input.text();
  const FixupReturn failed(false, input_text);

  // Fixup and canonicalize user input.
  const GURL canonical_gurl(
      url_formatter::FixupURL(base::UTF16ToUTF8(input_text), std::string()));
  std::string canonical_gurl_str(canonical_gurl.possibly_invalid_spec());
  if (canonical_gurl_str.empty()) {
    // This probably won't happen, but there are no guarantees.
    return failed;
  }

  // If the user types a number, GURL will convert it to a dotted quad.
  // However, if the parser did not mark this as a URL, then the user probably
  // didn't intend this interpretation.  Since this can break history matching
  // for hostname beginning with numbers (e.g. input of "17173" will be matched
  // against "0.0.67.21" instead of the original "17173", failing to find
  // "17173.com"), swap the original hostname in for the fixed-up one.
  if ((input.type() != metrics::OmniboxInputType::URL) &&
      canonical_gurl.HostIsIPAddress()) {
    std::string original_hostname = base::UTF16ToUTF8(
        input_text.substr(input.parts().host.begin, input.parts().host.len));
    const url::Parsed& parts =
        canonical_gurl.parsed_for_possibly_invalid_spec();
    // parts.host must not be empty when HostIsIPAddress() is true.
    DCHECK(parts.host.is_nonempty());
    canonical_gurl_str.replace(parts.host.begin, parts.host.len,
                               original_hostname);
  }
  std::u16string output(base::UTF8ToUTF16(canonical_gurl_str));
  // Don't prepend a scheme when the user didn't have one.  Since the fixer
  // upper only prepends the "http" scheme that's all we need to check for.
  // Note that even if Defaulting Typed Omnibox Navigations to HTTPS feature is
  // enabled, the https upgrade is done in AutocompleteInput::Parse() and not
  // in the fixer upper, so we don't need to check for that case.
  if (!AutocompleteInput::HasHTTPScheme(input_text))
    TrimSchemePrefix(&output, /*trim_https=*/false);

  // Make the number of trailing slashes on the output exactly match the input.
  // Examples of why not doing this would matter:
  // * The user types "a" and has this fixed up to "a/".  Now no other sites
  //   beginning with "a" will match.
  // * The user types "file:" and has this fixed up to "file://".  Now inline
  //   autocomplete will append too few slashes, resulting in e.g. "file:/b..."
  //   instead of "file:///b..."
  // * The user types "http:/" and has this fixed up to "http:".  Now inline
  //   autocomplete will append too many slashes, resulting in e.g.
  //   "http:///c..." instead of "http://c...".
  // NOTE: We do this after calling TrimHttpPrefix() since that can strip
  // trailing slashes (if the scheme is the only thing in the input).  It's not
  // clear that the result of fixup really matters in this case, but there's no
  // harm in making sure.
  const size_t last_input_nonslash = input_text.find_last_not_of(u"/\\");
  size_t num_input_slashes =
      (last_input_nonslash == std::u16string::npos)
          ? input_text.length()
          : (input_text.length() - 1 - last_input_nonslash);
  // If we appended text, user slashes are irrelevant.
  if (output.length() > input_text.length() &&
      base::StartsWith(output, input_text, base::CompareCase::SENSITIVE))
    num_input_slashes = 0;
  const size_t last_output_nonslash = output.find_last_not_of(u"/\\");
  const size_t num_output_slashes =
      (last_output_nonslash == std::u16string::npos)
          ? output.length()
          : (output.length() - 1 - last_output_nonslash);
  if (num_output_slashes < num_input_slashes)
    output.append(num_input_slashes - num_output_slashes, '/');
  else if (num_output_slashes > num_input_slashes)
    output.erase(output.length() - num_output_slashes + num_input_slashes);
  if (output.empty())
    return failed;

  return FixupReturn(true, output);
}

// static
size_t AutocompleteProvider::TrimSchemePrefix(std::u16string* url,
                                              bool trim_https) {
  // Find any "http:" or "https:".
  if (trim_https && !AutocompleteInput::HasHTTPSScheme(*url))
    return 0;
  if (!trim_https && !AutocompleteInput::HasHTTPScheme(*url))
    return 0;
  const char* scheme = trim_https ? url::kHttpsScheme : url::kHttpScheme;
  size_t scheme_pos = url->find(base::ASCIIToUTF16(scheme) + u':');
  DCHECK_NE(std::u16string::npos, scheme_pos);

  // Erase scheme plus up to two slashes.
  size_t prefix_end = scheme_pos + strlen(scheme) + 1;
  const size_t after_slashes = std::min(url->length(), prefix_end + 2);
  while ((prefix_end < after_slashes) && ((*url)[prefix_end] == '/'))
    ++prefix_end;
  url->erase(scheme_pos, prefix_end - scheme_pos);
  return (scheme_pos == 0) ? prefix_end : 0;
}

// static
bool AutocompleteProvider::InKeywordMode(const AutocompleteInput& input) {
  return input.keyword_mode_entry_method() !=
         metrics::OmniboxEventProto::INVALID;
}

void AutocompleteProvider::ResizeMatches(size_t max_matches,
                                         bool ml_scoring_enabled) {
  if (matches_.size() <= max_matches) {
    return;
  }

  // When ML Scoring is not enabled, simply resize the `matches_` list.
  if (!ml_scoring_enabled) {
    matches_.resize(max_matches);
    return;
  }

  // The provider should pass all match candidates to the controller if ML
  // scoring is enabled. Mark any matches over `max_matches` with zero relevance
  // and `culled_by_provider` set to true to simulate the resizing.
  base::ranges::for_each(std::next(matches_.begin(), max_matches),
                         matches_.end(), [&](auto& match) {
                           match.relevance = 0;
                           match.culled_by_provider = true;
                         });
}
