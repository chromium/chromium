// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

// SuggestionDeletionHandler -------------------------------------------------

// This class handles making requests to the server in order to delete
// personalized suggestions.
class SuggestionDeletionHandler {
 public:
  typedef base::Callback<void(bool, SuggestionDeletionHandler*)>
      DeletionCompletedCallback;

  SuggestionDeletionHandler(AutocompleteProviderClient* client,
                            const std::string& deletion_url,
                            const DeletionCompletedCallback& callback);

  ~SuggestionDeletionHandler();

 private:
  // Callback from SimpleURLLoader
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> deletion_fetcher_;
  DeletionCompletedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionDeletionHandler);
};

SuggestionDeletionHandler::SuggestionDeletionHandler(
    AutocompleteProviderClient* client,
    const std::string& deletion_url,
    const DeletionCompletedCallback& callback)
    : callback_(callback) {
  GURL url(deletion_url);
  DCHECK(url.is_valid());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_suggest_deletion", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When users attempt to delete server-provided personalized search "
            "or navigation suggestions from the omnibox dropdown, Chrome sends "
            "a message to the server requesting deletion of the suggestion."
          trigger:
            "A user attempt to delete a server-provided omnibox suggestion, "
            "for which the server provided a custom deletion URL."
          data:
            "No user data is explicitly sent with the request, but because the "
            "requested URL is provided by the server for each specific "
            "suggestion, it necessarily uniquely identifies the suggestion the "
            "user is attempting to delete."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Since this can only be triggered on seeing server-provided "
            "suggestions in the omnibox dropdown, whether it is enabled is the "
            "same as whether those suggestions are enabled.\n"
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "setting under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  variations::AppendVariationsHeaderUnknownSignedIn(
      request->url,
      client->IsOffTheRecord() ? variations::InIncognito::kYes
                               : variations::InIncognito::kNo,
      request.get());
  deletion_fetcher_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  deletion_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      client->GetURLLoaderFactory().get(),
      base::BindOnce(&SuggestionDeletionHandler::OnURLLoadComplete,
                     base::Unretained(this), deletion_fetcher_.get()));
}

SuggestionDeletionHandler::~SuggestionDeletionHandler() {
}

void SuggestionDeletionHandler::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(source == deletion_fetcher_.get());
  const bool ok = source->NetError() == net::OK &&
                  (source->ResponseInfo() && source->ResponseInfo()->headers &&
                   source->ResponseInfo()->headers->response_code() == 200);
  callback_.Run(ok, this);
}

// BaseSearchProvider ---------------------------------------------------------

BaseSearchProvider::BaseSearchProvider(AutocompleteProvider::Type type,
                                       AutocompleteProviderClient* client)
    : AutocompleteProvider(type),
      client_(client),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false) {
}

// static
bool BaseSearchProvider::ShouldPrefetch(const AutocompleteMatch& match) {
  return match.GetAdditionalInfo(kShouldPrefetchKey) == kTrue;
}

// static
AutocompleteMatch BaseSearchProvider::CreateSearchSuggestion(
    const base::string16& suggestion,
    AutocompleteMatchType::Type type,
    bool from_keyword,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data) {
  // These calls use a number of default values.  For instance, they assume
  // that if this match is from a keyword provider, then the user is in keyword
  // mode.  They also assume the caller knows what it's doing and we set
  // this match to look as if it was received/created synchronously.
  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, type, /*subtype_identifier=*/0, from_keyword,
      /*relevance=*/0, /*relevance_from_server=*/false,
      /*input_text=*/base::string16());
  suggest_result.set_received_after_last_keystroke(false);
  return CreateSearchSuggestion(nullptr, AutocompleteInput(), from_keyword,
                                suggest_result, template_url, search_terms_data,
                                0, false);
}

// static
AutocompleteMatch BaseSearchProvider::CreateOnDeviceSearchSuggestion(
    AutocompleteProvider* autocomplete_provider,
    const AutocompleteInput& input,
    const base::string16& suggestion,
    int relevance,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    int accepted_suggestion) {
  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, AutocompleteMatchType::SEARCH_SUGGEST,
      /*subtype_identifier=*/271, /*from_keyword_provider=*/false, relevance,
      /*relevance_from_server=*/false,
      base::CollapseWhitespace(input.text(), false));
  // On device providers are asynchronous.
  suggest_result.set_received_after_last_keystroke(true);
  return CreateSearchSuggestion(
      autocomplete_provider, input, /*in_keyword_mode=*/false, suggest_result,
      template_url, search_terms_data, accepted_suggestion,
      /*append_extra_query_params_from_command_line=*/true);
}

// static
void BaseSearchProvider::AppendSuggestClientToAdditionalQueryParams(
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    metrics::OmniboxEventProto::PageClassification page_classification,
    TemplateURLRef::SearchTermsArgs* search_terms_args) {
  // Only append the suggest client query param for Google template URL.
  if (template_url->GetEngineType(search_terms_data) != SEARCH_ENGINE_GOOGLE)
    return;

  if (page_classification == metrics::OmniboxEventProto::CHROMEOS_APP_LIST) {
    if (!search_terms_args->additional_query_params.empty())
      search_terms_args->additional_query_params.append("&");
    search_terms_args->additional_query_params.append("sclient=cros-launcher");
  }
}

// static
bool BaseSearchProvider::IsNTPPage(
    metrics::OmniboxEventProto::PageClassification classification) {
  using OEP = metrics::OmniboxEventProto;
  return (classification == OEP::NTP) ||
         (classification == OEP::OBSOLETE_INSTANT_NTP) ||
         (classification == OEP::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::NTP_REALBOX);
}

void BaseSearchProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  if (!match.GetAdditionalInfo(BaseSearchProvider::kDeletionUrlKey).empty()) {
    deletion_handlers_.push_back(std::make_unique<SuggestionDeletionHandler>(
        client(), match.GetAdditionalInfo(BaseSearchProvider::kDeletionUrlKey),
        base::BindRepeating(&BaseSearchProvider::OnDeletionComplete,
                            base::Unretained(this))));
  }

  const TemplateURL* template_url =
      match.GetTemplateURL(client_->GetTemplateURLService(), false);
  // This may be nullptr if the template corresponding to the keyword has been
  // deleted or there is no keyword set.
  if (template_url != nullptr) {
    client_->DeleteMatchingURLsForKeywordFromHistory(template_url->id(),
                                                     match.contents);
  }

  // Immediately update the list of matches to show the match was deleted,
  // regardless of whether the server request actually succeeds.
  DeleteMatchFromMatches(match);
}

void BaseSearchProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  std::vector<uint32_t> field_trial_hashes;
  OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
  for (size_t i = 0; i < field_trial_hashes.size(); ++i) {
    if (field_trial_triggered_)
      new_entry.mutable_field_trial_triggered()->Add(field_trial_hashes[i]);
    if (field_trial_triggered_in_session_) {
      new_entry.mutable_field_trial_triggered_in_session()->Add(
          field_trial_hashes[i]);
    }
  }
}

// static
const char BaseSearchProvider::kRelevanceFromServerKey[] =
    "relevance_from_server";
const char BaseSearchProvider::kShouldPrefetchKey[] = "should_prefetch";
const char BaseSearchProvider::kSuggestMetadataKey[] = "suggest_metadata";
const char BaseSearchProvider::kDeletionUrlKey[] = "deletion_url";
const char BaseSearchProvider::kTrue[] = "true";
const char BaseSearchProvider::kFalse[] = "false";

BaseSearchProvider::~BaseSearchProvider() {}

// static
AutocompleteMatch BaseSearchProvider::CreateSearchSuggestion(
    AutocompleteProvider* autocomplete_provider,
    const AutocompleteInput& input,
    const bool in_keyword_mode,
    const SearchSuggestionParser::SuggestResult& suggestion,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    int accepted_suggestion,
    bool append_extra_query_params_from_command_line) {
  AutocompleteMatch match(autocomplete_provider, suggestion.relevance(), false,
                          suggestion.type());

  if (!template_url)
    return match;
  match.keyword = template_url->keyword();
  match.image_dominant_color = suggestion.image_dominant_color();
  match.image_url = suggestion.image_url();
  match.contents = suggestion.match_contents();
  match.contents_class = suggestion.match_contents_class();
  match.answer = suggestion.answer();
  match.subtype_identifier = suggestion.subtype_identifier();
  if (suggestion.type() == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    match.RecordAdditionalInfo(kACMatchPropertySuggestionText,
                               base::UTF16ToUTF8(suggestion.suggestion()));
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsPrefix,
        base::UTF16ToUTF8(suggestion.match_contents_prefix()));
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsStartIndex,
        static_cast<int>(
            suggestion.suggestion().length() - match.contents.length()));
  }

  if (!suggestion.annotation().empty()) {
    match.description = suggestion.annotation();
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &match.description_class, 0, ACMatchClassification::NONE);
  }

  const base::string16 input_lower = base::i18n::ToLower(input.text());
  // suggestion.match_contents() should have already been collapsed.
  match.allowed_to_be_default_match =
      (!in_keyword_mode || suggestion.from_keyword()) &&
      (base::CollapseWhitespace(input_lower, false) ==
       base::i18n::ToLower(suggestion.match_contents()));

  if (suggestion.from_keyword())
    match.from_keyword = true;

  // We only allow inlinable navsuggestions that were received before the
  // last keystroke because we don't want asynchronous inline autocompletions.
  if (!input.prevent_inline_autocomplete() &&
      !suggestion.received_after_last_keystroke() &&
      (!in_keyword_mode || suggestion.from_keyword()) &&
      base::StartsWith(
          base::i18n::ToLower(suggestion.suggestion()), input_lower,
          base::CompareCase::SENSITIVE)) {
    match.inline_autocompletion =
        suggestion.suggestion().substr(input.text().length());
    match.allowed_to_be_default_match = true;
  }

  const TemplateURLRef& search_url = template_url->url_ref();
  DCHECK(search_url.SupportsReplacement(search_terms_data));
  base::string16 query(suggestion.suggestion());
  base::string16 original_query(input.text());
  if (suggestion.type() == AutocompleteMatchType::CALCULATOR) {
    // Use query text, rather than the calculator answer suggestion, to search.
    query = original_query;
    original_query.clear();
  }
  match.fill_into_edit = GetFillIntoEdit(suggestion, template_url);
  match.search_terms_args.reset(new TemplateURLRef::SearchTermsArgs(query));
  match.search_terms_args->original_query = original_query;
  match.search_terms_args->accepted_suggestion = accepted_suggestion;
  match.search_terms_args->additional_query_params =
      suggestion.additional_query_params();
  match.search_terms_args->append_extra_query_params_from_command_line =
      append_extra_query_params_from_command_line;
  // This is the destination URL sans assisted query stats.  This must be set
  // so the AutocompleteController can properly de-dupe; the controller will
  // eventually overwrite it before it reaches the user.
  match.destination_url = GURL(search_url.ReplaceSearchTerms(
      *match.search_terms_args, search_terms_data));

  // Search results don't look like URLs.
  match.transition = suggestion.from_keyword() ? ui::PAGE_TRANSITION_KEYWORD
                                               : ui::PAGE_TRANSITION_GENERATED;

  return match;
}

// static
base::string16 BaseSearchProvider::GetFillIntoEdit(
    const SearchSuggestionParser::SuggestResult& suggest_result,
    const TemplateURL* template_url) {
  base::string16 fill_into_edit;

  if (suggest_result.from_keyword())
    fill_into_edit.append(template_url->keyword() + base::char16(' '));

  fill_into_edit.append(suggest_result.suggestion());

  return fill_into_edit;
}

// static
bool BaseSearchProvider::CanSendURL(
    const GURL& current_page_url,
    const GURL& suggest_url,
    const TemplateURL* template_url,
    metrics::OmniboxEventProto::PageClassification page_classification,
    const SearchTermsData& search_terms_data,
    AutocompleteProviderClient* client,
    bool sending_search_terms) {
  // Make sure we are sending the suggest request through a cryptographically
  // secure channel to prevent exposing the current page URL or personalized
  // results without encryption.
  if (!suggest_url.SchemeIsCryptographic())
    return false;

  // Don't run if in incognito mode.
  if (client->IsOffTheRecord())
    return false;

  // Don't run if we can't get preferences or search suggest is not enabled.
  if (!client->SearchSuggestEnabled())
    return false;

  // Only make the request if we know that the provider supports sending zero
  // suggest. (Currently only the prepopulated Google provider supports it.)
  if (template_url == nullptr ||
      !template_url->SupportsReplacement(search_terms_data) ||
      template_url->GetEngineType(search_terms_data) != SEARCH_ENGINE_GOOGLE)
    return false;

  if (!current_page_url.is_valid())
    return false;

  // Don't bother sending the URL of an NTP page; it's not useful.  The server
  // already gets equivalent information in the form of the current page
  // classification.
  if (IsNTPPage(page_classification))
    return false;

  // Only allow HTTP URLs or HTTPS URLs.
  const bool scheme_allowed = (current_page_url.scheme() == url::kHttpScheme) ||
                              (current_page_url.scheme() == url::kHttpsScheme);
  if (!scheme_allowed)
    return false;

  // If URL data collection is off, forbid sending the current page URL to the
  // suggest endpoint - unless both of these hold:
  //  * The suggest endpoint and current page must be same-origin. In that
  //    case, the suggest endpoint could have already logged the current URL
  //    when the user accessed it from the server.
  //  * The search terms must be empty. When the user is typing new search
  //    terms, Chrome should not leak to the endpoint which tab the user is
  //    looking at. On-focus suggest requests don't contain a query.
  if (!client->IsPersonalizedUrlDataCollectionActive()) {
    bool safe_to_send_url_without_data_collection_active =
        url::IsSameOriginWith(current_page_url, suggest_url) &&
        !sending_search_terms;

    if (!safe_to_send_url_without_data_collection_active)
      return false;
  }

  return true;
}

void BaseSearchProvider::SetDeletionURL(const std::string& deletion_url,
                                        AutocompleteMatch* match) {
  if (deletion_url.empty())
    return;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider())
    return;
  GURL url =
      template_url_service->GetDefaultSearchProvider()->GenerateSearchURL(
          template_url_service->search_terms_data());
  url = url.GetOrigin().Resolve(deletion_url);
  if (url.is_valid()) {
    match->RecordAdditionalInfo(BaseSearchProvider::kDeletionUrlKey,
                                url.spec());
    match->deletable = true;
  }
}

void BaseSearchProvider::AddMatchToMap(
    const SearchSuggestionParser::SuggestResult& result,
    const std::string& metadata,
    int accepted_suggestion,
    bool mark_as_deletable,
    bool in_keyword_mode,
    MatchMap* map) {
  AutocompleteMatch match = CreateSearchSuggestion(
      this, GetInput(result.from_keyword()), in_keyword_mode, result,
      GetTemplateURL(result.from_keyword()),
      client_->GetTemplateURLService()->search_terms_data(),
      accepted_suggestion, ShouldAppendExtraParams(result));
  if (!match.destination_url.is_valid())
    return;
  match.RecordAdditionalInfo(kRelevanceFromServerKey,
                             result.relevance_from_server() ? kTrue : kFalse);
  match.RecordAdditionalInfo(kShouldPrefetchKey,
                             result.should_prefetch() ? kTrue : kFalse);
  SetDeletionURL(result.deletion_url(), &match);
  if (mark_as_deletable)
    match.deletable = true;
  // Metadata is needed only for prefetching queries.
  if (result.should_prefetch())
    match.RecordAdditionalInfo(kSuggestMetadataKey, metadata);

  // Try to add |match| to |map|.  If a match for this suggestion is
  // already in |map|, replace it if |match| is more relevant.
  // NOTE: Keep this ToLower() call in sync with url_database.cc.
  MatchKey match_key(
      std::make_pair(base::i18n::ToLower(result.suggestion()),
                     match.search_terms_args->additional_query_params));
  const std::pair<MatchMap::iterator, bool> i(
       map->insert(std::make_pair(match_key, match)));

  bool should_prefetch = result.should_prefetch();
  if (!i.second) {
    // NOTE: We purposefully do a direct relevance comparison here instead of
    // using AutocompleteMatch::MoreRelevant(), so that we'll prefer "items
    // added first" rather than "items alphabetically first" when the scores
    // are equal. The only case this matters is when a user has results with
    // the same score that differ only by capitalization; because the history
    // system returns results sorted by recency, this means we'll pick the most
    // recent such result even if the precision of our relevance score is too
    // low to distinguish the two.
    if (match.relevance > i.first->second.relevance) {
      match.duplicate_matches.insert(match.duplicate_matches.end(),
                                     i.first->second.duplicate_matches.begin(),
                                     i.first->second.duplicate_matches.end());
      i.first->second.duplicate_matches.clear();
      match.duplicate_matches.push_back(i.first->second);
      i.first->second = std::move(match);
    } else {
      if (match.keyword == i.first->second.keyword) {
        // Old and new matches are from the same search provider. It is okay to
        // record one match's prefetch data onto a different match (for the same
        // query string) for the following reasons:
        // 1. Because the suggest server only sends down a query string from
        // which we construct a URL, rather than sending a full URL, and because
        // we construct URLs from query strings in the same way every time, the
        // URLs for the two matches will be the same. Therefore, we won't end up
        // prefetching something the server didn't intend.
        // 2. Presumably the server sets the prefetch bit on a match it things
        // is sufficiently relevant that the user is likely to choose it.
        // Surely setting the prefetch bit on a match of even higher relevance
        // won't violate this assumption.
        should_prefetch |= ShouldPrefetch(i.first->second);
        i.first->second.RecordAdditionalInfo(kShouldPrefetchKey,
                                             should_prefetch ? kTrue : kFalse);
        if (should_prefetch)
          i.first->second.RecordAdditionalInfo(kSuggestMetadataKey, metadata);
      }
      i.first->second.duplicate_matches.push_back(std::move(match));
    }
    // Copy over answer data from lower-ranking item, if necessary.
    // This depends on the lower-ranking item always being added last - see
    // use of push_back above.
    AutocompleteMatch& more_relevant_match = i.first->second;
    const AutocompleteMatch& less_relevant_match =
        more_relevant_match.duplicate_matches.back();
    if (less_relevant_match.answer && !more_relevant_match.answer) {
      more_relevant_match.answer = less_relevant_match.answer;
    }
  }
}

bool BaseSearchProvider::ParseSuggestResults(
    const base::Value& root_val,
    int default_result_relevance,
    bool is_keyword_result,
    SearchSuggestionParser::Results* results) {
  if (!SearchSuggestionParser::ParseSuggestResults(
          root_val, GetInput(is_keyword_result), client_->GetSchemeClassifier(),
          default_result_relevance, is_keyword_result, results))
    return false;

  field_trial_triggered_ |= results->field_trial_triggered;
  field_trial_triggered_in_session_ |= results->field_trial_triggered;
  return true;
}

void BaseSearchProvider::DeleteMatchFromMatches(
    const AutocompleteMatch& match) {
  for (auto i(matches_.begin()); i != matches_.end(); ++i) {
    // Find the desired match to delete by checking the type and contents.
    // We can't check the destination URL, because the autocomplete controller
    // may have reformulated that. Not that while checking for matching
    // contents works for personalized suggestions, if more match types gain
    // deletion support, this algorithm may need to be re-examined.
    if (i->contents == match.contents && i->type == match.type) {
      matches_.erase(i);
      break;
    }
  }
}

void BaseSearchProvider::OnDeletionComplete(
    bool success, SuggestionDeletionHandler* handler) {
  RecordDeletionResult(success);
  base::EraseIf(
      deletion_handlers_,
      [handler](const std::unique_ptr<SuggestionDeletionHandler>& elem) {
        return elem.get() == handler;
      });
}
