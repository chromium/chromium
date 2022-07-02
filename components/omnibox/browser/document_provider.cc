// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Inclusive bounds used to restrict which queries request drive suggestions
// from the backend.
const size_t kMinQueryLength = 4;
const size_t kMaxQueryLength = 200;

// TODO(skare): Pull the enum in search_provider.cc into its .h file, and switch
// this file and zero_suggest_provider.cc to use it.
enum DocumentRequestsHistogramValue {
  DOCUMENT_REQUEST_SENT = 1,
  DOCUMENT_REQUEST_INVALIDATED = 2,
  DOCUMENT_REPLY_RECEIVED = 3,
  DOCUMENT_MAX_REQUEST_HISTOGRAM_VALUE
};

void LogOmniboxDocumentRequest(DocumentRequestsHistogramValue request_value) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.DocumentSuggest.Requests", request_value,
                            DOCUMENT_MAX_REQUEST_HISTOGRAM_VALUE);
}

void LogTotalTime(base::TimeTicks start_time, bool interrupted) {
  DCHECK(!start_time.is_null());
  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("Omnibox.DocumentSuggest.TotalTime", elapsed_time);
  if (interrupted) {
    UMA_HISTOGRAM_TIMES("Omnibox.DocumentSuggest.TotalTime.Interrupted",
                        elapsed_time);
  } else {
    UMA_HISTOGRAM_TIMES("Omnibox.DocumentSuggest.TotalTime.NotInterrupted",
                        elapsed_time);
  }
}

void LogRequestTime(base::TimeTicks start_time, bool interrupted) {
  DCHECK(!start_time.is_null());
  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("Omnibox.DocumentSuggest.RequestTime", elapsed_time);
  if (interrupted) {
    UMA_HISTOGRAM_TIMES("Omnibox.DocumentSuggest.RequestTime.Interrupted",
                        elapsed_time);
  } else {
    UMA_HISTOGRAM_TIMES("Omnibox.DocumentSuggest.RequestTime.NotInterrupted",
                        elapsed_time);
  }
}

// MIME types sent by the server for different document types.
const char kDocumentMimetype[] = "application/vnd.google-apps.document";
const char kFormMimetype[] = "application/vnd.google-apps.form";
const char kSpreadsheetMimetype[] = "application/vnd.google-apps.spreadsheet";
const char kPresentationMimetype[] = "application/vnd.google-apps.presentation";

// Returns mappings from MIME types to overridden icons.
AutocompleteMatch::DocumentType GetIconForMIMEType(
    const base::StringPiece& mimetype) {
  static const auto kIconMap =
      std::map<base::StringPiece, AutocompleteMatch::DocumentType>{
          {kDocumentMimetype, AutocompleteMatch::DocumentType::DRIVE_DOCS},
          {kFormMimetype, AutocompleteMatch::DocumentType::DRIVE_FORMS},
          {kSpreadsheetMimetype, AutocompleteMatch::DocumentType::DRIVE_SHEETS},
          {kPresentationMimetype,
           AutocompleteMatch::DocumentType::DRIVE_SLIDES},
          {"image/jpeg", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
          {"image/png", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
          {"image/gif", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
          {"application/pdf", AutocompleteMatch::DocumentType::DRIVE_PDF},
          {"video/mp4", AutocompleteMatch::DocumentType::DRIVE_VIDEO},
          {"application/vnd.google-apps.folder",
           AutocompleteMatch::DocumentType::DRIVE_FOLDER},
      };

  const auto& iterator = kIconMap.find(mimetype);
  return iterator != kIconMap.end()
             ? iterator->second
             : AutocompleteMatch::DocumentType::DRIVE_OTHER;
}

String16Vector SplitByColon(const String16Vector& words) {
  return std::accumulate(
      words.begin(), words.end(), String16Vector(),
      [](String16Vector accumulated, const auto& word) {
        const auto split = base::SplitString(
            word, u":", base::WhitespaceHandling::TRIM_WHITESPACE,
            base::SplitResult::SPLIT_WANT_NONEMPTY);
        accumulated.insert(accumulated.end(), split.begin(), split.end());
        return accumulated;
      });
}

struct FieldMatches {
  double weight;
  String16Vector words;
  size_t count;

  FieldMatches(double weight, const std::string* string)
      : FieldMatches(weight, std::vector<const std::string*>{string}) {}

  FieldMatches(double weight, std::vector<const std::string*> strings)
      : weight(weight),
        words(std::accumulate(
            strings.begin(),
            strings.end(),
            String16Vector(),
            [](String16Vector word_vec, const std::string* string) {
              if (string) {
                const auto string_words =
                    SplitByColon(String16VectorFromString16(
                        base::UTF8ToUTF16(string->c_str()), nullptr));
                word_vec.insert(word_vec.end(), string_words.begin(),
                                string_words.end());
              }
              return word_vec;
            })),
        count(0) {}

  // Increments |count| and returns true if |words| includes a word equal to or
  // prefixed by |word|.
  bool Includes(const std::u16string& word) {
    if (std::none_of(words.begin(), words.end(), [word](std::u16string w) {
          return base::StartsWith(w, word,
                                  base::CompareCase::INSENSITIVE_ASCII);
        }))
      return false;
    count += word.size();
    return true;
  }

  // Decreases linearly with respect to |count| for small values, begins at 1,
  // and asymptotically approaches 0.
  double InvScore() { return std::pow(1 - weight, count); }
};

// Extracts a list of pointers to strings from a DictionaryValue containing a
// list of objects containing a string field of interest. Note that pointers may
// be `nullptr` if the value at `field_path` is not found or is not a string.
std::vector<const std::string*> ExtractResultList(
    const base::Value* result,
    const base::StringPiece& list_path,
    const base::StringPiece& field_path) {
  const base::Value* values = result->FindListPath(list_path);
  if (!values)
    return {};

  auto list = values->GetListDeprecated();
  std::vector<const std::string*> extracted(list.size());
  std::transform(list.begin(), list.end(), extracted.begin(),
                 [field_path](const auto& value) {
                   return value.FindStringKey(field_path);
                 });
  return extracted;
}

// Alias for GetFieldTrialParamByFeatureAsDouble for readability.
double FieldWeight(const std::string& param_name, double default_weight) {
  return base::GetFieldTrialParamByFeatureAsDouble(omnibox::kDocumentProvider,
                                                   param_name, default_weight);
}

int CalculateScore(const std::u16string& input, const base::Value* result) {
  // Suggestions scored lower than |raw_score_cutoff| will be discarded.
  double raw_score_cutoff = base::GetFieldTrialParamByFeatureAsDouble(
      omnibox::kDocumentProvider, "RawDocScoreCutoff", .25);
  // Final score will be between |min_score| and |max_score|, not accounting for
  // |raw_score_cutoff|.
  int min_score = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "MinDocScore", 0);
  int max_score = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "MaxDocScore", 1400);

  std::vector<FieldMatches> field_matches_vec = {
      {FieldWeight("TitleWeight", .15), result->FindStringKey("title")},
      {FieldWeight("OwnerNamesWeight", .15),
       ExtractResultList(result, "metadata.owner.personNames", "displayName")},
      {FieldWeight("OwnerEmailsWeight", .15),
       ExtractResultList(result, "metadata.owner.emailAddresses",
                         "emailAddress")},
      {FieldWeight("SnippetWeight", .06),
       result->FindStringPath("snippet.snippet")},
      {FieldWeight("UrlWeight", 0), result->FindStringKey("url")},
      {FieldWeight("MimeWeight", 0),
       result->FindStringPath("metadata.mimeType")},
  };
  std::stable_sort(field_matches_vec.begin(), field_matches_vec.end(),
                   [](const FieldMatches& a, const FieldMatches& b) {
                     return a.weight > b.weight;
                   });

  String16Vector input_words =
      SplitByColon(String16VectorFromString16(input, nullptr));

  for (const auto& word : input_words) {
    for (auto& field_matches : field_matches_vec) {
      // This is calculating the proportion of the user input words that are
      // included in the suggestion, so break after the first match. Otherwise,
      // an input like 'wi' would be scored too highly for the suggestion "will
      // william wilson win the winter windsurfing competition".
      if (field_matches.Includes(word)) {
        break;
      }
    }
  }

  // |score| is computed by subtracting the product of each field's inverse
  // score from 1; |score| begins at 0 and asymptotically approaches 1.
  // Summing each field's score would grossly favor short multi-field matches
  // over long single-field matches due to each fields score increasing faster
  // for small values.
  double score =
      1 -
      std::accumulate(field_matches_vec.begin(), field_matches_vec.end(), 1.0,
                      [](double inv_score_product, FieldMatches field_matches) {
                        return inv_score_product * field_matches.InvScore();
                      });

  if (score > 1)
    score = 1;
  if (score < raw_score_cutoff)
    score = 0;

  return static_cast<int>(min_score + score * (max_score - min_score));
}

int BoostOwned(const int score,
               const std::string& owner,
               const base::Value* result) {
  int promotion = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "OwnedDocPromotion", 0);
  int demotion = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "UnownedDocDemotion", 200);

  std::vector<const std::string*> owner_emails = ExtractResultList(
      result, "metadata.owner.emailAddresses", "emailAddress");

  bool owned = std::any_of(
      owner_emails.begin(), owner_emails.end(),
      [owner](const std::string* email) { return owner == *email; });

  return std::max(score + (owned ? promotion : -demotion), 0);
}

// Derived from google3/apps/share/util/docs_url_extractor.cc.
std::string ExtractDocIdFromUrl(const std::string& url) {
  static const RE2 docs_url_pattern_(
      "\\b("  // The first groups matches the whole URL.
      // Domain.
      "(?:https?://)?(?:"
      "spreadsheets|docs|drive|script|sites|jamboard"
      ")[0-9]?.google.com"
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
      // Pther valid chars.
      "(?:[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]*)"
      "(?:(#[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]+)?)"  // Fragment
      ")");

  std::vector<re2::StringPiece> matched_doc_ids(
      docs_url_pattern_.NumberOfCapturingGroups() + 1);
  // ANCHOR_START deviates from google3 which uses UNANCHORED. Using
  // ANCHOR_START prevents incorrectly matching with non-drive URLs but which
  // contain a drive URL; e.g.,
  // url-parser.com/?url=https://docs.google.com/document/d/(id)/edit.
  if (!docs_url_pattern_.Match(url, 0, url.size(), RE2::ANCHOR_START,
                               matched_doc_ids.data(),
                               matched_doc_ids.size())) {
    return std::string();
  }
  for (const auto& doc_id_group : docs_url_pattern_.NamedCapturingGroups()) {
    re2::StringPiece identified_doc_id = matched_doc_ids[doc_id_group.second];
    if (!identified_doc_id.empty()) {
      return std::string(identified_doc_id);
    }
  }
  return std::string();
}

std::string FindStringKeyOrEmpty(const base::Value& value, std::string key) {
  auto* ptr = value.FindStringKey(key);
  return ptr ? *ptr : "";
}

}  // namespace

// static
DocumentProvider* DocumentProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener,
    size_t cache_size) {
  return new DocumentProvider(client, listener, cache_size);
}

// static
void DocumentProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(omnibox::kDocumentSuggestEnabled, true);
}

bool DocumentProvider::IsDocumentProviderAllowed(
    AutocompleteProviderClient* client,
    const AutocompleteInput& input) {
  // Feature must be on.
  if (!base::FeatureList::IsEnabled(omnibox::kDocumentProvider))
    return false;

  // These may seem like search suggestions, so gate on that setting too.
  if (!client->SearchSuggestEnabled())
    return false;

  // Client-side toggle must be enabled.
  if (!client->GetPrefs()->GetBoolean(omnibox::kDocumentSuggestEnabled))
    return false;

  // No incognito.
  if (client->IsOffTheRecord())
    return false;

  // Check sync's status and proceed if active.
  bool authenticated_and_syncing =
      client->IsAuthenticated() && client->IsSyncActive();
  if (!authenticated_and_syncing)
    return false;

  // We haven't received a server backoff signal.
  if (backoff_for_session_)
    return false;

  // Google must be set as default search provider.
  auto* template_url_service = client->GetTemplateURLService();
  if (template_url_service == nullptr)
    return false;
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (default_provider == nullptr ||
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) != SEARCH_ENGINE_GOOGLE) {
    return false;
  }

  if (OmniboxFieldTrial::IsExperimentalKeywordModeEnabled() &&
      input.prefer_keyword()) {
    // If a keyword provider matches, and we're explicitly in keyword mode,
    // then the keyword provider must match the default, or the document
    // provider.
    AutocompleteInput keyword_input = input;
    const TemplateURL* keyword_provider =
        KeywordProvider::GetSubstitutingTemplateURLForInput(
            template_url_service, &keyword_input);
    if (keyword_provider &&
        IsExplicitlyInKeywordMode(input, keyword_provider->keyword()) &&
        !base::StartsWith(input.text(), u"drive.google.com",
                          base::CompareCase::SENSITIVE)) {
      return false;
    }
  }

  // There should be no document suggestions fetched for on-focus suggestion
  // requests, or if the input is empty.
  if (input.focus_type() != OmniboxFocusType::DEFAULT ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return false;
  }

  // Experiment: don't issue queries for inputs under some length.
  if (input.text().length() < kMinQueryLength ||
      input.text().length() > kMaxQueryLength) {
    return false;
  }

  // Don't issue queries for input likely to be a URL.
  if (IsInputLikelyURL(input))
    return false;

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
  field_trial_triggered_ = false;

  // Perform various checks - feature is enabled, user is allowed to use the
  // feature, we're not under backoff, etc.
  if (!IsDocumentProviderAllowed(client_, input))
    return;

  input_ = input;

  // Return cached suggestions synchronously after setting the relevance of any
  // beyond |provider_max_matches_| to 0.
  CopyCachedMatchesToMatches();
  DemoteMatchesBeyondMax();

  if (!input.want_asynchronous_matches()) {
    return;
  }

  done_ = false;  // Set true in callbacks.
  debouncer_->RequestRun(
      base::BindOnce(&DocumentProvider::Run, base::Unretained(this)));
}

void DocumentProvider::Run() {
  time_run_invoked_ = base::TimeTicks::Now();
  client_->GetDocumentSuggestionsService(/*create_if_necessary=*/true)
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
  debouncer_->CancelRequest();

  // If the request was sent, then log its duration and that it was invalidated.
  if (loader_) {
    DCHECK(!time_run_invoked_.is_null());
    DCHECK(!time_request_sent_.is_null());
    loader_.reset();
    LogRequestTime(time_request_sent_, true);
    time_request_sent_ = base::TimeTicks();
    LogOmniboxDocumentRequest(DOCUMENT_REQUEST_INVALIDATED);
  }

  // If `Run()` has been invoked, log its duration. It's possible `Stop()` is
  // invoked before `Run()` has been invoked if 1) this is the first user input,
  // 2) the previous call was debounced, or 3) the previous request was filtered
  // (e.g. input too short).
  if (!time_run_invoked_.is_null()) {
    LogTotalTime(time_run_invoked_, true);
    time_run_invoked_ = base::TimeTicks();
  }

  auto* document_suggestions_service =
      client_->GetDocumentSuggestionsService(/*create_if_necessary=*/false);
  if (document_suggestions_service != nullptr) {
    document_suggestions_service->StopCreatingDocumentSuggestionsRequest();
  }

  done_ = true;

  if (clear_cached_results) {
    matches_.clear();
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

  if (field_trial_triggered_ || field_trial_triggered_in_session_) {
    std::vector<uint32_t> field_trial_hashes;
    OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
    for (uint32_t trial : field_trial_hashes) {
      if (field_trial_triggered_) {
        new_entry.mutable_field_trial_triggered()->Add(trial);
      }
      if (field_trial_triggered_in_session_) {
        new_entry.mutable_field_trial_triggered_in_session()->Add(trial);
      }
    }
  }
}

void DocumentProvider::ResetSession() {
  field_trial_triggered_in_session_ = false;
  field_trial_triggered_ = false;
}

DocumentProvider::DocumentProvider(AutocompleteProviderClient* client,
                                   AutocompleteProviderListener* listener,
                                   size_t cache_size)
    : AutocompleteProvider(AutocompleteProvider::TYPE_DOCUMENT),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false),
      backoff_for_session_(false),
      client_(client),
      cache_size_(cache_size),
      matches_cache_(MatchesCache::NO_AUTO_EVICT) {
  AddListener(listener);

  if (base::FeatureList::IsEnabled(omnibox::kDebounceDocumentProvider)) {
    bool from_last_run = base::GetFieldTrialParamByFeatureAsBool(
        omnibox::kDebounceDocumentProvider,
        "DebounceDocumentProviderFromLastRun", true);
    int delay_ms = base::GetFieldTrialParamByFeatureAsInt(
        omnibox::kDebounceDocumentProvider, "DebounceDocumentProviderDelayMs",
        300);
    debouncer_ = std::make_unique<AutocompleteProviderDebouncer>(from_last_run,
                                                                 delay_ms);
  } else
    debouncer_ = std::make_unique<AutocompleteProviderDebouncer>(false, 0);
}

DocumentProvider::~DocumentProvider() = default;

void DocumentProvider::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  LogRequestTime(time_request_sent_, false);
  LogOmniboxDocumentRequest(DOCUMENT_REPLY_RECEIVED);

  int httpStatusCode = source->ResponseInfo() && source->ResponseInfo()->headers
                           ? source->ResponseInfo()->headers->response_code()
                           : 0;

  if (httpStatusCode == 400 || httpStatusCode == 499)
    backoff_for_session_ = true;

  const bool results_updated =
      response_body && source->NetError() == net::OK && httpStatusCode == 200 &&
      UpdateResults(SearchSuggestionParser::ExtractJsonData(
          source, std::move(response_body)));
  LogTotalTime(time_run_invoked_, false);
  loader_.reset();
  done_ = true;
  NotifyListeners(results_updated);
}

bool DocumentProvider::UpdateResults(const std::string& json_data) {
  absl::optional<base::Value> response =
      base::JSONReader::Read(json_data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!response)
    return false;

  // 1) Fill |matches_| with <N> new server matches.
  matches_ = ParseDocumentSearchResults(*response);
  // 2) Clear cached matches' scores to ensure cached matches for all but the
  // previous input can only be shown if deduped. E.g., this allows matches for
  // the input 'pari' to be displayed synchronously for the input 'paris', but
  // be hidden if the user clears their input and starts anew 'london'.
  SetCachedMatchesScoresTo0();
  // 3) Push the <N> new matches to the cache.
  for (const AutocompleteMatch& match : base::Reversed(matches_))
    matches_cache_.Put(match.stripped_destination_url, match);
  // 4) Copy the cached matches to |matches_|, skipping the most recent <N>
  // cached matches since they were already added in step (1). Pass
  // |set_scores_to_0| as true as we don't trust cached scores since they may no
  // longer match the current input; if the cached matches were still relevant,
  // they would have been returned from the server again.
  CopyCachedMatchesToMatches(matches_.size());
  // 5) Only now can we shrink the cache to |cache_size_|. Doing this
  // automatically when pushing the new matches to the cache would reduce it's
  // effective size, especially if the server returns close to |cache_size_|
  // matches.
  matches_cache_.ShrinkToSize(cache_size_);
  // 6) Limit matches to |provider_max_matches_| unless used for deduping; i.e.
  // set the scores of matches beyond the limit to 0.
  DemoteMatchesBeyondMax();

  return !matches_.empty();
}

void DocumentProvider::OnDocumentSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  time_request_sent_ = base::TimeTicks::Now();
  loader_ = std::move(loader);
  LogOmniboxDocumentRequest(DOCUMENT_REQUEST_SENT);
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
    return base::TimeFormatWithPattern(modified_time, "MMMd");
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
  const base::Value* results = root_val.FindListKey("results");
  if (!results) {
    return matches;
  }
  size_t num_results = results->GetListDeprecated().size();
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.DocumentSuggest.ResultCount", num_results);

  // During development/quality iteration we may wish to defeat server scores.
  // If both |use_server_score| and |use_client_score| are true, the min of the
  // two scores will be used.
  // If both are false, the server score will be used.
  bool use_client_score = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kDocumentProvider, "DocumentUseClientScore", false);
  bool use_server_score = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kDocumentProvider, "DocumentUseServerScore", true);

  // Cap scores for each suggestion.
  bool cap_score_per_rank = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kDocumentProvider, "DocumentCapScorePerRank", false);
  std::vector<int> score_caps = {
      base::GetFieldTrialParamByFeatureAsInt(omnibox::kDocumentProvider,
                                             "DocumentCapScoreRank1", 1200),
      base::GetFieldTrialParamByFeatureAsInt(omnibox::kDocumentProvider,
                                             "DocumentCapScoreRank2", 1100),
      base::GetFieldTrialParamByFeatureAsInt(omnibox::kDocumentProvider,
                                             "DocumentCapScoreRank3", 900),
  };

  // Promotes owned documents and/or demotes unowned documents.
  bool boost_owned = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kDocumentProvider, "DocumentBoostOwned", false);

  // Ensure server's suggestions are added with monotonically decreasing scores.
  int previous_score = INT_MAX;
  for (size_t i = 0; i < num_results; i++) {
    const base::Value& result = results->GetListDeprecated()[i];
    if (!result.is_dict()) {
      return matches;
    }
    const std::string title = FindStringKeyOrEmpty(result, "title");
    const std::string url = FindStringKeyOrEmpty(result, "url");
    if (title.empty() || url.empty()) {
      continue;
    }

    // Both client and server scores are calculated regardless of usage in order
    // to log them with |AutocompleteMatch::RecordAdditionalInfo| below.
    int client_score = CalculateScore(input_.text(), &result);
    int server_score = result.FindIntKey("score").value_or(0);
    int score = 0;

    if (use_client_score && use_server_score)
      score = std::min(client_score, server_score);
    else
      score = use_client_score ? client_score : server_score;

    if (cap_score_per_rank) {
      int score_cap = i < score_caps.size() ? score_caps[i] : score_caps.back();
      score = std::min(score, score_cap);
    }

    if (boost_owned)
      score = BoostOwned(score, client_->ProfileUserName(), &result);

    // Decrement scores if necessary to ensure suggestion order is preserved.
    // Don't decrement client scores which don't necessarily rank suggestions
    // the same order as the server.
    if (!use_client_score && score >= previous_score)
      score = std::max(previous_score - 1, 0);
    previous_score = score;

    AutocompleteMatch match(this, score, false,
                            AutocompleteMatchType::DOCUMENT_SUGGESTION);
    // Use full URL for displayed text and navigation. Use "originalUrl" for
    // deduping if present.
    match.fill_into_edit = base::UTF8ToUTF16(url);
    match.destination_url = GURL(url);
    const std::string* original_url = result.FindStringKey("originalUrl");
    if (original_url) {
      // |AutocompleteMatch::GURLToStrippedGURL()| will try to use
      // |GetURLForDeduping()| to extract a doc ID and generate a canonical doc
      // URL; this is ideal as it handles different URL formats pointing to the
      // same doc. Otherwise, it'll resort to the typical stripped URL
      // generation that can still be used for generic deduping and as a key to
      // |matches_cache_|.
      match.stripped_destination_url = AutocompleteMatch::GURLToStrippedGURL(
          GURL(*original_url), input_, client_->GetTemplateURLService(),
          std::u16string());
    }

    match.contents =
        AutocompleteMatch::SanitizeString(base::UTF8ToUTF16(title));
    match.contents_class = Classify(match.contents, input_.text());
    const base::Value* metadata = result.FindDictKey("metadata");
    if (metadata) {
      const std::string update_time =
          FindStringKeyOrEmpty(*metadata, "updateTime");
      const std::string mimetype = FindStringKeyOrEmpty(*metadata, "mimeType");
      if (metadata->FindStringKey("mimeType")) {
        match.document_type = GetIconForMIMEType(mimetype);
        match.RecordAdditionalInfo(
            "document type",
            AutocompleteMatch::DocumentTypeString(match.document_type));
      }
      auto owners = ExtractResultList(&result, "metadata.owner.personNames",
                                      "displayName");
      const std::string owner = !owners.empty() && owners[0] ? *owners[0] : "";
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
    match.RecordAdditionalInfo("client score", client_score);
    match.RecordAdditionalInfo("server score", server_score);
    if (matches.size() >= provider_max_matches_)
      match.RecordAdditionalInfo("for deduping only", "true");
    const std::string* snippet = result.FindStringPath("snippet.snippet");
    if (snippet)
      match.RecordAdditionalInfo("snippet", *snippet);
    matches.push_back(match);
    field_trial_triggered_ = true;
    field_trial_triggered_in_session_ = true;
  }
  return matches;
}

void DocumentProvider::CopyCachedMatchesToMatches(
    size_t skip_n_most_recent_matches) {
  std::for_each(std::next(matches_cache_.begin(), skip_n_most_recent_matches),
                matches_cache_.end(), [&](const auto& cache_key_match_pair) {
                  auto match = cache_key_match_pair.second;
                  match.allowed_to_be_default_match = false;
                  match.TryRichAutocompletion(
                      base::UTF8ToUTF16(match.destination_url.spec()),
                      match.contents, input_);
                  match.contents_class =
                      DocumentProvider::Classify(match.contents, input_.text());
                  match.RecordAdditionalInfo("from cache", "true");
                  matches_.push_back(match);
                });
}

void DocumentProvider::SetCachedMatchesScoresTo0() {
  std::for_each(matches_cache_.begin(), matches_cache_.end(),
                [&](auto& cache_key_match_pair) {
                  cache_key_match_pair.second.relevance = 0;
                });
}

void DocumentProvider::DemoteMatchesBeyondMax() {
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
  // Early exit to avoid unnecessary and more involved checks.
  if (!url.DomainIs("google.com"))
    return GURL();

  // We aim to prevent duplicate Drive URLs to appear between the Drive document
  // search provider and history/bookmark entries.
  // All URLs are canonicalized to a GURL form only used for deduplication and
  // not guaranteed to be usable for navigation.

  // Drive redirects are already handled by the regex in |ExtractDocIdFromUrl|.
  // The below logic handles google.com redirects; e.g., google.com/url/q=<url>
  std::string url_str;
  if (url.host() == "www.google.com" && url.path() == "/url") {
    if ((!net::GetValueForKeyInQuery(url, "q", &url_str) || url_str.empty()) &&
        (!net::GetValueForKeyInQuery(url, "url", &url_str) || url_str.empty()))
      return GURL();
  } else {
    url_str = url.spec();
  }

  // Unescape |url_str|
  url_str = base::UnescapeURLComponent(
      url_str,
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

  const std::string id = ExtractDocIdFromUrl(url_str);

  // Canonicalize to the /open form without any extra args.
  // This is similar to what we expect from the server.
  return id.empty() ? GURL() : GURL("https://drive.google.com/open?id=" + id);
}
