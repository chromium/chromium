// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Inclusive bounds used to restrict which queries request drive suggestions
// from the backend.
const size_t kMaxQueryLength = 200;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Keep up to date with DocumentProviderAllowedReason in
// //tools/metrics/histograms/enums.xml.
enum class DocumentProviderAllowedReason : int {
  kAllowed = 0,
  kUnknown = 1,
  kFeatureDisabled = 2,
  kSuggestSettingDisabled = 3,
  kDriveSettingDisabled = 4,
  kOffTheRecord = 5,
  kNotLoggedIn = 6,
  kNotSyncing = 7,
  kBackoff = 8,
  kDSENotGoogle = 9,
  kInputOnFocusOrEmpty = 10,
  kInputTooShort = 11,
  kInputLooksLikeUrl = 12,
  kMaxValue = kInputLooksLikeUrl
};

void LogOmniboxDocumentRequest(RemoteRequestEvent request_event) {
  base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.Requests",
                                request_event);
}

void LogTotalTime(base::TimeTicks start_time, bool interrupted) {
  DCHECK(!start_time.is_null());
  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes("Omnibox.DocumentSuggest.TotalTime", elapsed_time);
  if (interrupted) {
    base::UmaHistogramTimes("Omnibox.DocumentSuggest.TotalTime.Interrupted",
                            elapsed_time);
  } else {
    base::UmaHistogramTimes("Omnibox.DocumentSuggest.TotalTime.NotInterrupted",
                            elapsed_time);
  }
}

void LogRequestTime(base::TimeTicks start_time, bool interrupted) {
  DCHECK(!start_time.is_null());
  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes("Omnibox.DocumentSuggest.RequestTime", elapsed_time);
  if (interrupted) {
    base::UmaHistogramTimes("Omnibox.DocumentSuggest.RequestTime.Interrupted",
                            elapsed_time);
  } else {
    base::UmaHistogramTimes(
        "Omnibox.DocumentSuggest.RequestTime.NotInterrupted", elapsed_time);
  }
}

// MIME types sent by the server for different document types.
constexpr char kDocumentMimetype[] = "application/vnd.google-apps.document";
constexpr char kFormMimetype[] = "application/vnd.google-apps.form";
constexpr char kSpreadsheetMimetype[] =
    "application/vnd.google-apps.spreadsheet";
constexpr char kPresentationMimetype[] =
    "application/vnd.google-apps.presentation";

// Returns mappings from MIME types to overridden icons.
AutocompleteMatch::DocumentType GetIconForMIMEType(std::string_view mimetype) {
  constexpr auto kIconMap =
      base::MakeFixedFlatMap<std::string_view, AutocompleteMatch::DocumentType>(
          {
              {kDocumentMimetype, AutocompleteMatch::DocumentType::DRIVE_DOCS},
              {kFormMimetype, AutocompleteMatch::DocumentType::DRIVE_FORMS},
              {kSpreadsheetMimetype,
               AutocompleteMatch::DocumentType::DRIVE_SHEETS},
              {kPresentationMimetype,
               AutocompleteMatch::DocumentType::DRIVE_SLIDES},
              {"image/jpeg", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
              {"image/png", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
              {"image/gif", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
              {"application/pdf", AutocompleteMatch::DocumentType::DRIVE_PDF},
              {"video/mp4", AutocompleteMatch::DocumentType::DRIVE_VIDEO},
              {"application/vnd.google-apps.folder",
               AutocompleteMatch::DocumentType::DRIVE_FOLDER},
          });

  const auto it = kIconMap.find(mimetype);
  return it != kIconMap.end() ? it->second
                              : AutocompleteMatch::DocumentType::DRIVE_OTHER;
}

// Concats `v2` onto `v1`.
template <typename T>
std::vector<T> Concat(std::vector<T>& v1, const std::vector<T>& v2) {
  v1.insert(v1.end(), v2.begin(), v2.end());
  return v1;
}

// Extracts a list of pointers to strings from a DictionaryValue containing a
// list of objects containing a string field of interest. Note that pointers may
// be `nullptr` if the value at `field_path` is not found or is not a string.
std::vector<const std::string*> ExtractResultList(
    const base::Value::Dict& result,
    std::string_view list_path,
    std::string_view field_path) {
  const base::Value::List* list = result.FindListByDottedPath(list_path);
  if (!list) {
    return {};
  }

  std::vector<const std::string*> extracted;
  for (const auto& value : *list) {
    auto* string = value.GetDict().FindString(field_path);
    if (string)
      extracted.push_back(string);
  }
  return extracted;
}

// Return whether `user` owns the doc `result`.
bool IsOwnedByUser(const std::string& user, const base::Value::Dict& result) {
  std::vector<const std::string*> owner_emails = ExtractResultList(
      result, "metadata.owner.emailAddresses", "emailAddress");
  const auto lower_user = base::i18n::ToLower(base::UTF8ToUTF16(user));
  return base::ranges::any_of(
      owner_emails,
      [&](const std::u16string& email) { return lower_user == email; },
      [&](const std::string* email) {
        return base::i18n::ToLower(base::UTF8ToUTF16(*email));
      });
}

// Return whether all words in `input` are contained in either the `result`
// title or owners.
bool IsCompletelyMatchedInTitleOrOwner(const std::u16string& input,
                                       const base::Value::Dict& result) {
  // Accumulate a vector of the title and all owners.
  auto search_strings = ExtractResultList(
      result, "metadata.owner.emailAddresses", "emailAddress");
  Concat(search_strings, ExtractResultList(result, "metadata.owner.personNames",
                                           "displayName"));
  search_strings.push_back(result.FindString("title"));

  // Extract a flat vector of words from the title and owners.
  const auto title_and_owner_words = std::accumulate(
      search_strings.begin(), search_strings.end(), String16Vector(),
      [](String16Vector accumulated, const auto& search_string) {
        Concat(accumulated,
               String16VectorFromString16(
                   base::i18n::ToLower(base::UTF8ToUTF16(*search_string)),
                   nullptr));
        return accumulated;
      });

  // Check if all input words are contained in `title_and_owner_words`.
  String16Vector input_words =
      String16VectorFromString16(base::i18n::ToLower(input), nullptr);
  for (const auto& input_word : input_words) {
    // It's possible `input` contained 'owner' as a word, as opposed to
    // 'owner:...' as an operator. Ignore this rare edge case for simplicity.
    if (input_word != u"owner" &&
        base::ranges::none_of(
            title_and_owner_words, [&](std::u16string title_word) {
              return base::StartsWith(title_word, input_word,
                                      base::CompareCase::INSENSITIVE_ASCII);
            })) {
      return false;
    }
  }

  return true;
}

// Derived from google3/apps/share/util/docs_url_extractor.cc.
std::string ExtractDocIdFromUrl(const std::string& url) {
  static const base::NoDestructor<RE2> docs_url_pattern(
      "\\b("  // The first groups matches the whole URL.
      // Domain.
      "(?:https?://)?(?:"
      // Keep the hosts consistent with `ValidHostPrefix()`.
      "spreadsheets|docs|drive|script|sites|jamboard"
      ")[0-9]?\\.google\\.com"
      "(?::[0-9]+)?\\/"  // Port.
      "(?:\\S*)"         // Non-whitespace chars.
      "(?:"
      // Doc url prefix to match /d/{id}. (?:e/)? deviates from google3.
      "(?:/d/(?:e/)?(?P<path_docid>[0-9a-zA-Z\\-\\_]+))"
      "|"
      // Docs id expr to match a valid id parameter.
      "(?:(?:\\?|&|&amp;)"
      "(?:id|docid|key|docID|DocId)=(?P<query_docid>[0-9a-zA-Z\\-\\_]+))"
      "|"
      // Folder url prefix to match /folders/{folder_id}.
      "(?:/folders/(?P<folder_docid>[0-9a-zA-Z\\-\\_]+))"
      "|"
      // Sites url prefix.
      "(?:/?s/)(?P<sites_docid>[0-9a-zA-Z\\-\\_]+)"
      "(?:/p/[0-9a-zA-Z\\-\\_]+)?/edit"
      "|"
      // Jam url.
      "(?:d/)(?P<jam_docid>[0-9a-zA-Z\\-\\_]+)/(?:edit|viewer)"
      ")"
      // Other valid chars.
      "(?:[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]*)"
      // Summarization details.
      "(?:summarizationDetails=[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/"
      "\\?(?:%5B)(?:%5D)]*)?"
      // Other valid chars.
      "(?:[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]*)"
      "(?:(#[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]+)?)"  // Fragment
      ")");

  std::vector<std::string_view> matched_doc_ids(
      docs_url_pattern->NumberOfCapturingGroups() + 1);
  // ANCHOR_START deviates from google3 which uses UNANCHORED. Using
  // ANCHOR_START prevents incorrectly matching with non-drive URLs but which
  // contain a drive URL; e.g.,
  // url-parser.com/?url=https://docs.google.com/document/d/(id)/edit.
  if (!docs_url_pattern->Match(url, 0, url.size(), RE2::ANCHOR_START,
                               matched_doc_ids.data(),
                               matched_doc_ids.size())) {
    return std::string();
  }
  for (const auto& doc_id_group : docs_url_pattern->NamedCapturingGroups()) {
    std::string_view identified_doc_id = matched_doc_ids[doc_id_group.second];
    if (!identified_doc_id.empty()) {
      return std::string(identified_doc_id);
    }
  }
  return std::string();
}

// Verify if the host could possibly be for a valid doc URL. This is a more
// lightweight check than `ExtractDocIdFromUrl()`. It can be done before
// unescaping the URL as valid hosts don't contain escapable chars; unescaping
// is relatively expensive. E.g., 'docs.google.com' isn't a valid doc URL, but
// it's host looks like it could be, so return true. On the other hand,
// 'google.com' is definitely not a doc URL so return false.
bool ValidHostPrefix(const std::string& host) {
  // There are 66 (5*11) valid, e.g. 'docs5.google.com', so rather than check
  // all 66, we just check the 6 prefixes. Keep these prefixes consistent with
  // those in `ExtractDocIdFromUrl()`.
  constexpr auto kValidHostPrefixes = base::MakeFixedFlatSet<std::string_view>({
      "spreadsheets",
      "docs",
      "drive",
      "script",
      "sites",
      "jamboard",
  });
  for (const auto& valid_host_prefix : kValidHostPrefixes) {
    if (base::StartsWith(host, valid_host_prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

// If `value[key]`, returns it. Otherwise, returns `fallback`.
std::string FindStringKeyOrFallback(const base::Value::Dict& value,
                                    std::string_view key,
                                    std::string fallback = "") {
  auto* ptr = value.FindString(key);
  return ptr ? *ptr : fallback;
}

}  // namespace

// static
DocumentProvider* DocumentProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new DocumentProvider(client, listener);
}

// static
void DocumentProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(omnibox::kDocumentSuggestEnabled, true);
}

bool DocumentProvider::IsDocumentProviderAllowed(
    const AutocompleteInput& input) {
  // Feature must be on.
  if (!base::FeatureList::IsEnabled(omnibox::kDocumentProvider)) {
    base::UmaHistogramEnumeration(
        "Omnibox.DocumentSuggest.ProviderAllowed",
        DocumentProviderAllowedReason::kFeatureDisabled);
    return false;
  }

  // These may seem like search suggestions, so gate on that setting too.
  if (!client_->SearchSuggestEnabled()) {
    base::UmaHistogramEnumeration(
        "Omnibox.DocumentSuggest.ProviderAllowed",
        DocumentProviderAllowedReason::kSuggestSettingDisabled);
    return false;
  }

  // Client-side toggle must be enabled.
  if (!base::FeatureList::IsEnabled(omnibox::kDocumentProviderNoSetting) &&
      !client_->GetPrefs()->GetBoolean(omnibox::kDocumentSuggestEnabled)) {
    base::UmaHistogramEnumeration(
        "Omnibox.DocumentSuggest.ProviderAllowed",
        DocumentProviderAllowedReason::kDriveSettingDisabled);
    return false;
  }

  // No incognito.
  if (client_->IsOffTheRecord()) {
    base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.ProviderAllowed",
                                  DocumentProviderAllowedReason::kOffTheRecord);
    return false;
  }

  // Must be logged in.
  if (!client_->IsAuthenticated()) {
    base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.ProviderAllowed",
                                  DocumentProviderAllowedReason::kNotLoggedIn);
    return false;
  }

  // Sync must be enabled and active.
  if (!base::FeatureList::IsEnabled(
          omnibox::kDocumentProviderNoSyncRequirement) &&
      !client_->IsSyncActive()) {
    base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.ProviderAllowed",
                                  DocumentProviderAllowedReason::kNotSyncing);
    return false;
  }

  // We haven't received a server backoff signal.
  if (backoff_for_session_) {
    base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.ProviderAllowed",
                                  DocumentProviderAllowedReason::kBackoff);
    return false;
  }

  // Google must be set as default search provider.
  auto* template_url_service = client_->GetTemplateURLService();
  if (!search::DefaultSearchProviderIsGoogle(template_url_service)) {
    base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.ProviderAllowed",
                                  DocumentProviderAllowedReason::kDSENotGoogle);
    return false;
  }

  // There should be no document suggestions fetched for on-focus suggestion
  // requests, or if the input is empty.
  if (input.IsZeroSuggest() ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    base::UmaHistogramEnumeration(
        "Omnibox.DocumentSuggest.ProviderAllowed",
        DocumentProviderAllowedReason::kInputOnFocusOrEmpty);
    return false;
  }

  // Don't issue queries for inputs whose lengths aren't in the intended range.
  if (input.text().length() <
          omnibox_feature_configs::DocumentProvider::Get().min_query_length ||
      input.text().length() > kMaxQueryLength) {
    base::UmaHistogramEnumeration(
        "Omnibox.DocumentSuggest.ProviderAllowed",
        DocumentProviderAllowedReason::kInputTooShort);
    return false;
  }

  // Don't issue queries for input likely to be a URL.
  if (IsInputLikelyURL(input)) {
    base::UmaHistogramEnumeration(
        "Omnibox.DocumentSuggest.ProviderAllowed",
        DocumentProviderAllowedReason::kInputLooksLikeUrl);
    return false;
  }

  base::UmaHistogramEnumeration("Omnibox.DocumentSuggest.ProviderAllowed",
                                DocumentProviderAllowedReason::kAllowed);
  return true;
}

// static
bool DocumentProvider::IsInputLikelyURL(const AutocompleteInput& input) {
  if (input.type() == metrics::OmniboxInputType::URL)
    return true;

  // Special cases when the user might be starting to type the most common URL
  // prefixes, but the SchemeClassifier won't have classified them as URLs yet.
  // Note these checks are of the form "(string constant) starts with input."
  if (input.text().length() <= 8) {
    if (StartsWith(u"https://", input.text(),
                   base::CompareCase::INSENSITIVE_ASCII) ||
        StartsWith(u"http://", input.text(),
                   base::CompareCase::INSENSITIVE_ASCII) ||
        StartsWith(u"www.", input.text(),
                   base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }

  return false;
}

void DocumentProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  TRACE_EVENT0("omnibox", "DocumentProvider::Start");
  Stop(true, false);

  // Perform various checks - feature is enabled, user is allowed to use the
  // feature, we're not under backoff, etc.
  if (!IsDocumentProviderAllowed(input))
    return;

  input_ = input;

  // Return cached suggestions synchronously after setting the relevance of any
  // beyond |provider_max_matches_| to 0.
  CopyCachedMatchesToMatches();
  DemoteMatchesBeyondMax();

  if (input.omit_asynchronous_matches()) {
    return;
  }

  done_ = false;  // Set true in callbacks.
  debouncer_->RequestRun(
      base::BindOnce(&DocumentProvider::Run, base::Unretained(this)));
}

void DocumentProvider::Run() {
  time_run_invoked_ = base::TimeTicks::Now();
  client_->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->CreateDocumentSuggestionsRequest(
          input_.text(), client_->IsOffTheRecord(),
          base::BindOnce(
              &DocumentProvider::OnDocumentSuggestionsLoaderAvailable,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &DocumentProvider::OnURLLoadComplete,
              base::Unretained(this) /* this owns SimpleURLLoader */));
}

void DocumentProvider::Stop(bool clear_cached_results,
                            bool due_to_user_inactivity) {
  TRACE_EVENT0("omnibox", "DocumentProvider::Stop");
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  debouncer_->CancelRequest();

  // If the request was sent, then log its duration and that it was invalidated.
  if (loader_) {
    DCHECK(!time_run_invoked_.is_null());
    DCHECK(!time_request_sent_.is_null());
    loader_.reset();
    LogRequestTime(time_request_sent_, true);
    time_request_sent_ = base::TimeTicks();
    LogOmniboxDocumentRequest(RemoteRequestEvent::kRequestInvalidated);
  }

  // If `Run()` has been invoked, log its duration. It's possible `Stop()` is
  // invoked before `Run()` has been invoked if 1) this is the first user input,
  // 2) the previous call was debounced, or 3) the previous request was filtered
  // (e.g. input too short).
  if (!time_run_invoked_.is_null()) {
    LogTotalTime(time_run_invoked_, true);
    time_run_invoked_ = base::TimeTicks();
  }

  if (auto* remote_suggestions_service =
          client_->GetRemoteSuggestionsService(/*create_if_necessary=*/false)) {
    remote_suggestions_service->StopCreatingDocumentSuggestionsRequest();
  }
}

void DocumentProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Not supported by this provider.
  return;
}

void DocumentProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(metrics::OmniboxEventProto::DOCUMENT);
  new_entry.set_provider_done(done_);
}

DocumentProvider::DocumentProvider(AutocompleteProviderClient* client,
                                   AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_DOCUMENT),
      backoff_for_session_(false),
      client_(client),
      matches_cache_(20) {
  AddListener(listener);

  debouncer_ = std::make_unique<AutocompleteProviderDebouncer>(true, 300);
}

DocumentProvider::~DocumentProvider() = default;

void DocumentProvider::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  LogRequestTime(time_request_sent_, false);
  LogOmniboxDocumentRequest(RemoteRequestEvent::kResponseReceived);
  base::UmaHistogramSparse("Omnibox.DocumentSuggest.HttpResponseCode",
                           response_code);

  // The following are codes that we believe indicate non-transient failures,
  // based on experience working with the owners of the API. Since they are
  // expected to be semi-persistent, it does not make sense to continue to issue
  // requests during the current session after receiving one.
  if (response_code == 400 || response_code == 403 || response_code == 499 ||
      (omnibox_feature_configs::DocumentProvider::Get().backoff_on_401 &&
       response_code == 401)) {
    backoff_for_session_ = true;
  }

  const bool results_updated =
      response_code == 200 &&
      UpdateResults(SearchSuggestionParser::ExtractJsonData(
          source, std::move(response_body)));
  LogTotalTime(time_run_invoked_, false);
  loader_.reset();
  done_ = true;
  NotifyListeners(results_updated);
}

bool DocumentProvider::UpdateResults(const std::string& json_data) {
  std::optional<base::Value> response =
      base::JSONReader::Read(json_data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!response)
    return false;

  // 1) Fill |matches_| with the new server matches.
  matches_ = ParseDocumentSearchResults(*response);

  // 2) Limit matches to |provider_max_matches_| unless used for deduping; i.e.
  // set the scores of matches beyond the limit to 0.
  DemoteMatchesBeyondMax();
  // 3) Clear cached matches' scores to ensure cached matches for all but the
  // previous input can only be shown if deduped. E.g., this allows matches for
  // the input 'pari' to be displayed synchronously for the input 'paris', but
  // be hidden if the user clears their input and starts anew 'london'.
  SetCachedMatchesScoresTo0();
  // 4) Copy the cached matches to |matches_|.
  CopyCachedMatchesToMatches();
  // 5) Push the new matches to the cache. Keep their scores so that later
  // inputs continue showing them until the new doc response returns.
  for (const AutocompleteMatch& match : base::Reversed(matches_))
    matches_cache_.Put(match.stripped_destination_url, match);

  return !matches_.empty();
}

void DocumentProvider::OnDocumentSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  time_request_sent_ = base::TimeTicks::Now();
  loader_ = std::move(loader);
  LogOmniboxDocumentRequest(RemoteRequestEvent::kRequestSent);
}

// static
std::u16string DocumentProvider::GenerateLastModifiedString(
    const std::string& modified_timestamp_string,
    base::Time now) {
  if (modified_timestamp_string.empty())
    return std::u16string();
  base::Time modified_time;
  if (!base::Time::FromString(modified_timestamp_string.c_str(),
                              &modified_time))
    return std::u16string();

  // Use shorthand if the times fall on the same day or in the same year.
  base::Time::Exploded exploded_modified_time;
  base::Time::Exploded exploded_now;
  modified_time.LocalExplode(&exploded_modified_time);
  now.LocalExplode(&exploded_now);
  if (exploded_modified_time.year == exploded_now.year) {
    if (exploded_modified_time.month == exploded_now.month &&
        exploded_modified_time.day_of_month == exploded_now.day_of_month) {
      // Same local calendar day - use localized time.
      return base::TimeFormatTimeOfDay(modified_time);
    }
    // Same year but not the same day: use abbreviated month/day ("Jan 1").
    return base::LocalizedTimeFormatWithPattern(modified_time, "MMMd");
  }

  // No shorthand; display full MM/DD/YYYY.
  return base::TimeFormatShortDateNumeric(modified_time);
}

// static
std::u16string DocumentProvider::GetProductDescriptionString(
    const std::string& mimetype) {
  if (mimetype == kDocumentMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_DOCUMENT);
  if (mimetype == kFormMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_FORM);
  if (mimetype == kSpreadsheetMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_SPREADSHEET);
  if (mimetype == kPresentationMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_PRESENTATION);
  // Fallback to "Drive" for other filetypes.
  return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_GENERAL);
}

// static
std::u16string DocumentProvider::GetMatchDescription(
    const std::string& update_time,
    const std::string& mimetype,
    const std::string& owner) {
  std::u16string mime_desc = GetProductDescriptionString(mimetype);
  if (!update_time.empty()) {
    std::u16string date_desc =
        GenerateLastModifiedString(update_time, base::Time::Now());
    return owner.empty()
               ? l10n_util::GetStringFUTF16(
                     IDS_DRIVE_SUGGESTION_DESCRIPTION_TEMPLATE_WITHOUT_OWNER,
                     date_desc, mime_desc)
               : l10n_util::GetStringFUTF16(
                     IDS_DRIVE_SUGGESTION_DESCRIPTION_TEMPLATE, date_desc,
                     base::UTF8ToUTF16(owner), mime_desc);
  }
  return owner.empty()
             ? mime_desc
             : l10n_util::GetStringFUTF16(
                   IDS_DRIVE_SUGGESTION_DESCRIPTION_TEMPLATE_WITHOUT_DATE,
                   base::UTF8ToUTF16(owner), mime_desc);
}

ACMatches DocumentProvider::ParseDocumentSearchResults(
    const base::Value& root_val) {
  ACMatches matches;

  // Parse the results.
  const base::Value::List* results = root_val.GetDict().FindList("results");
  if (!results) {
    return matches;
  }
  size_t num_results = results->size();
  base::UmaHistogramCounts1M("Omnibox.DocumentSuggest.ResultCount",
                             num_results);

  // Ensure server's suggestions are added with monotonically decreasing scores.
  int previous_score = INT_MAX;

  // Number of matches that are neither owned nor a complete title or owner
  // match.
  int low_quality_match_count = 0;

  for (size_t i = 0; i < num_results; i++) {
    const base::Value& result_value = (*results)[i];
    if (!result_value.is_dict()) {
      return matches;
    }

    const base::Value::Dict& result = result_value.GetDict();
    const std::string title = FindStringKeyOrFallback(result, "title");
    const std::string url = FindStringKeyOrFallback(result, "url");
    if (title.empty() || url.empty()) {
      continue;
    }

    int score = result.FindInt("score").value_or(0);

    // Decrement scores if necessary to ensure suggestion order is preserved.
    // Don't decrement client scores which don't necessarily rank suggestions
    // the same order as the server.
    if (score >= previous_score)
      score = std::max(previous_score - 1, 0);
    previous_score = score;

    // Only allow up to 1 doc that is neither owned nor a complete title or
    // owner match.
    bool is_owned = IsOwnedByUser(client_->ProfileUserName(), result);
    bool is_completely_matched_in_title_and_owner =
        IsCompletelyMatchedInTitleOrOwner(input_.text(), result);
    if (!is_owned && !is_completely_matched_in_title_and_owner &&
        ++low_quality_match_count > 1) {
      score = 0;
    }

    AutocompleteMatch match(this, score, false,
                            AutocompleteMatchType::DOCUMENT_SUGGESTION);
    // Use full URL for navigation. If present, use "originalUrl" for display &
    // deduping, as it's shorter.
    const std::string short_url =
        FindStringKeyOrFallback(result, "originalUrl", url);
    match.fill_into_edit = base::UTF8ToUTF16(short_url);
    match.destination_url = GURL(url);
    // `AutocompleteMatch::GURLToStrippedGURL()` will try to use
    // `GetURLForDeduping()` to extract a doc ID and generate a canonical doc
    // URL; this is ideal as it handles different URL formats pointing to the
    // same doc. Otherwise, it'll resort to the typical stripped URL generation
    // that can still be used for generic deduping and as a key to
    // `matches_cache_`.
    match.stripped_destination_url = AutocompleteMatch::GURLToStrippedGURL(
        GURL(short_url), input_, client_->GetTemplateURLService(),
        std::u16string(), /*keep_search_intent_params=*/false,
        /*normalize_search_terms=*/false);

    match.contents =
        AutocompleteMatch::SanitizeString(base::UTF8ToUTF16(title));
    match.contents_class = Classify(match.contents, input_.text());
    const base::Value::Dict* metadata = result.FindDict("metadata");
    if (metadata) {
      const std::string update_time =
          FindStringKeyOrFallback(*metadata, "updateTime");
      const std::string mimetype =
          FindStringKeyOrFallback(*metadata, "mimeType");
      if (metadata->FindString("mimeType")) {
        match.document_type = GetIconForMIMEType(mimetype);
        match.RecordAdditionalInfo(
            "document type",
            AutocompleteMatch::DocumentTypeString(match.document_type));
      }
      auto owners = ExtractResultList(result, "metadata.owner.personNames",
                                      "displayName");
      const std::string owner = !owners.empty() ? *owners[0] : "";
      if (!owner.empty())
        match.RecordAdditionalInfo("document owner", owner);
      match.description = GetMatchDescription(update_time, mimetype, owner);
      AutocompleteMatch::AddLastClassificationIfNecessary(
          &match.description_class, 0, ACMatchClassification::DIM);
      // Exclude date & owner from description_for_shortcut to avoid showing
      // stale data from the shortcuts provider.
      match.description_for_shortcuts = GetMatchDescription("", mimetype, "");
      AutocompleteMatch::AddLastClassificationIfNecessary(
          &match.description_class_for_shortcuts, 0,
          ACMatchClassification::DIM);
      match.RecordAdditionalInfo("description_for_shortcuts",
                                 match.description_for_shortcuts);
    }

    match.TryRichAutocompletion(base::UTF8ToUTF16(match.destination_url.spec()),
                                match.contents, input_);
    match.transition = ui::PAGE_TRANSITION_GENERATED;
    match.RecordAdditionalInfo("owned", is_owned);
    match.RecordAdditionalInfo("completely matched in title and owner",
                               is_completely_matched_in_title_and_owner);
    if (matches.size() >= provider_max_matches_)
      match.RecordAdditionalInfo("for deduping only", "true");
    const std::string* snippet =
        result.FindStringByDottedPath("snippet.snippet");
    if (snippet)
      match.RecordAdditionalInfo("snippet", *snippet);
    matches.push_back(match);
  }
  return matches;
}

void DocumentProvider::CopyCachedMatchesToMatches() {
  base::ranges::transform(
      matches_cache_, std::back_inserter(matches_),
      [this](auto match) {
        match.allowed_to_be_default_match = false;
        match.TryRichAutocompletion(
            base::UTF8ToUTF16(match.destination_url.spec()), match.contents,
            input_);
        match.contents_class =
            DocumentProvider::Classify(match.contents, input_.text());
        match.RecordAdditionalInfo("from cache", "true");
        return match;
      },
      &MatchesCache::value_type::second);
}

void DocumentProvider::SetCachedMatchesScoresTo0() {
  base::ranges::for_each(matches_cache_, [&](auto& cache_key_match_pair) {
    cache_key_match_pair.second.relevance = 0;
  });
}

void DocumentProvider::DemoteMatchesBeyondMax() {
  // Allow all matches to retain their scores if unlimited matches param is
  // enabled.
  if (OmniboxFieldTrial::IsMlUrlScoringUnlimitedNumCandidatesEnabled()) {
    return;
  }

  for (size_t i = provider_max_matches_; i < matches_.size(); ++i)
    matches_[i].relevance = 0;
}

// static
ACMatchClassifications DocumentProvider::Classify(
    const std::u16string& text,
    const std::u16string& input_text) {
  TermMatches term_matches = FindTermMatches(input_text, text);
  return ClassifyTermMatches(term_matches, text.size(),
                             ACMatchClassification::MATCH,
                             ACMatchClassification::NONE);
}

// static
const GURL DocumentProvider::GetURLForDeduping(const GURL& url) {
  if (!url.is_valid())
    return GURL();

  // A memoization cache. Only updated if `ExtractDocIdFromUrl()` was attempted.
  // That's the most expensive part of this algorithm, and memoizing the earlier
  // trivial checks would worsen performance by pushing out more useful cache
  // entries.
  static base::NoDestructor<base::LRUCache<GURL, GURL>> cache(10);
  const auto& cached = cache->Get(url);
  if (cached != cache->end()) {
    return cached->second;
  }

  // Early exit to avoid unnecessary and more involved checks. Don't update the
  // cache for trivial cases to avoid pushing out a more useful entry.
  if (!url.DomainIs("google.com"))
    return GURL();

  // We aim to prevent duplicate Drive URLs to appear between the Drive document
  // search provider and history/bookmark entries.
  // All URLs are canonicalized to a GURL form only used for deduplication and
  // not guaranteed to be usable for navigation.

  // Drive redirects are already handled by the regex in |ExtractDocIdFromUrl|.
  // The below logic handles google.com redirects; e.g., google.com/url/q=<url>
  std::string url_str;
  std::string url_str_host;
  if (url.host() == "www.google.com" && url.path() == "/url") {
    if ((!net::GetValueForKeyInQuery(url, "q", &url_str) || url_str.empty()) &&
        (!net::GetValueForKeyInQuery(url, "url", &url_str) || url_str.empty()))
      return GURL();
    url_str_host = GURL(url_str).host();
  } else {
    url_str = url.spec();
    url_str_host = url.host();
  }

  // Recheck the domain, since a google URL could redirect to a non-google URL
  if (!base::EndsWith(url_str_host, "google.com",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return GURL();
  }

  // Filter out non-doc hosts. Do this before unescaping the URL below, as
  // unescaping can be expensive and valid hosts don't contain escapable chars.
  // Do this after simplifying the google.com redirect above, as that changes
  // the host.
  if (!ValidHostPrefix(url_str_host))
    return GURL();

  // Unescape |url_str|
  url_str = base::UnescapeURLComponent(
      url_str,
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

  const std::string id = ExtractDocIdFromUrl(url_str);

  // Canonicalize to the /open form without any extra args.
  // This is similar to what we expect from the server.
  GURL deduping_url =
      id.empty() ? GURL() : GURL("https://drive.google.com/open?id=" + id);
  cache->Put(url, deduping_url);
  return deduping_url;
}
