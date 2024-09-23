// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider.h"

#include <algorithm>
#include <string>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

AutocompleteProvider::AutocompleteProvider(Type type)
    : provider_max_matches_(OmniboxFieldTrial::GetProviderMaxMatches(type)),
      type_(type) {}

// static
const char* AutocompleteProvider::TypeToString(Type type) {
  // When creating a new provider, add the provider type to this function and
  // make sure to also add the appropriate OmniboxProvider variant to the
  // Omnibox.ProviderTime2 histogram (defined in omnibox/histograms.xml) so that
  // the run-time metrics associated with the relevant provider can be properly
  // analyzed.
  switch (type) {
    case TYPE_BOOKMARK:
      return "Bookmark";
    case TYPE_BUILTIN:
      return "Builtin";
    case TYPE_CLIPBOARD:
      return "Clipboard";
    case TYPE_DOCUMENT:
      return "Document";
    case TYPE_HISTORY_QUICK:
      return "HistoryQuick";
    case TYPE_HISTORY_URL:
      return "HistoryURL";
    case TYPE_KEYWORD:
      return "Keyword";
    case TYPE_ON_DEVICE_HEAD:
      return "OnDeviceHead";
    case TYPE_SEARCH:
      return "Search";
    case TYPE_SHORTCUTS:
      return "Shortcuts";
    case TYPE_ZERO_SUGGEST:
      return "ZeroSuggest";
    case TYPE_ZERO_SUGGEST_LOCAL_HISTORY:
      return "LocalHistoryZeroSuggest";
    case TYPE_QUERY_TILE:
      return "QueryTile";
    case TYPE_MOST_VISITED_SITES:
      return "MostVisitedSites";
    case TYPE_VERBATIM_MATCH:
      return "VerbatimMatch";
    case TYPE_VOICE_SUGGEST:
      return "VoiceSuggest";
    case TYPE_HISTORY_FUZZY:
      return "HistoryFuzzy";
    case TYPE_OPEN_TAB:
      return "OpenTab";
    case TYPE_HISTORY_CLUSTER_PROVIDER:
      return "HistoryCluster";
    case TYPE_CALCULATOR:
      return "Calculator";
    case TYPE_FEATURED_SEARCH:
      return "FeaturedSearch";
    case TYPE_HISTORY_EMBEDDINGS:
      return "HistoryEmbeddings";
    default:
      DUMP_WILL_BE_NOTREACHED()
          << "Unhandled AutocompleteProvider::Type " << type;
      return "Unknown";
  }
}

void AutocompleteProvider::AddListener(AutocompleteProviderListener* listener) {
  listeners_.push_back(listener);
}

void AutocompleteProvider::NotifyListeners(bool updated_matches) const {
  for (AutocompleteProviderListener* listener : listeners_) {
    listener->OnProviderUpdate(updated_matches, this);
  }
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

const char* AutocompleteProvider::GetName() const {
  return TypeToString(type_);
}

metrics::OmniboxEventProto_ProviderType
AutocompleteProvider::AsOmniboxEventProviderType() const {
  switch (type_) {
    case TYPE_BOOKMARK:
      return metrics::OmniboxEventProto::BOOKMARK;
    case TYPE_BUILTIN:
      return metrics::OmniboxEventProto::BUILTIN;
    case TYPE_CLIPBOARD:
      return metrics::OmniboxEventProto::CLIPBOARD;
    case TYPE_DOCUMENT:
      return metrics::OmniboxEventProto::DOCUMENT;
    case TYPE_HISTORY_QUICK:
      return metrics::OmniboxEventProto::HISTORY_QUICK;
    case TYPE_HISTORY_URL:
      return metrics::OmniboxEventProto::HISTORY_URL;
    case TYPE_KEYWORD:
      return metrics::OmniboxEventProto::KEYWORD;
    case TYPE_ON_DEVICE_HEAD:
      return metrics::OmniboxEventProto::ON_DEVICE_HEAD;
    case TYPE_SEARCH:
      return metrics::OmniboxEventProto::SEARCH;
    case TYPE_SHORTCUTS:
      return metrics::OmniboxEventProto::SHORTCUTS;
    case TYPE_ZERO_SUGGEST:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case TYPE_ZERO_SUGGEST_LOCAL_HISTORY:
      return metrics::OmniboxEventProto::ZERO_SUGGEST_LOCAL_HISTORY;
    case TYPE_QUERY_TILE:
      return metrics::OmniboxEventProto::QUERY_TILE;
    case TYPE_MOST_VISITED_SITES:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case TYPE_VERBATIM_MATCH:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    case TYPE_VOICE_SUGGEST:
      return metrics::OmniboxEventProto::SEARCH;
    case TYPE_HISTORY_FUZZY:
      return metrics::OmniboxEventProto::HISTORY_FUZZY;
    case TYPE_OPEN_TAB:
      return metrics::OmniboxEventProto::OPEN_TAB;
    case TYPE_HISTORY_CLUSTER_PROVIDER:
      return metrics::OmniboxEventProto::HISTORY_CLUSTER;
    case TYPE_CALCULATOR:
      return metrics::OmniboxEventProto::CALCULATOR;
    case TYPE_FEATURED_SEARCH:
      return metrics::OmniboxEventProto::FEATURED_SEARCH;
    case TYPE_HISTORY_EMBEDDINGS:
      return metrics::OmniboxEventProto::HISTORY_EMBEDDINGS;
    default:
      // TODO(crbug.com/40940012) This was a NOTREACHED that we converted to
      //   help debug crbug.com/1499235 since NOTREACHED's don't log their
      //   message in crash reports. Should be reverted back to a NOTREACHED or
      //   NOTREACHED if their logs eventually begin being logged to
      //   crash reports.
      DUMP_WILL_BE_NOTREACHED()
          << "[NOTREACHED] Unhandled AutocompleteProvider::Type " << type_;
      return metrics::OmniboxEventProto::UNKNOWN_PROVIDER;
  }
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
