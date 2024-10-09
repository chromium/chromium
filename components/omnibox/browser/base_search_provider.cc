// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_answer_action.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_scoring_signals_annotator.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);
constexpr bool is_ios = !!BUILDFLAG(IS_IOS);

bool MatchTypeAndContentsAreEqual(const AutocompleteMatch& lhs,
                                  const AutocompleteMatch& rhs) {
  return lhs.contents == rhs.contents && lhs.type == rhs.type;
}

std::u16string GetMatchContentsForOnDeviceTailSuggestion(
    const std::u16string& input_text,
    const std::u16string& sanitized_suggestion) {
  std::u16string sanitized_input;

  base::TrimWhitespace(input_text, base::TRIM_TRAILING, &sanitized_input);
  sanitized_input = AutocompleteMatch::SanitizeString(sanitized_input);

  if (!base::StartsWith(sanitized_suggestion, sanitized_input,
                        base::CompareCase::SENSITIVE)) {
    return sanitized_suggestion;
  }

  // If there is no space inside the suggestion, show the entire suggestion in
  // UI. Otherwise replace the completed prefix of the suggestion with tail UI
  // symbols e.g. "...".
  // Examples (input/suggestion -> result):
  // 1. [googl]/[google] -> [google]
  // 2. [google]/[google map] -> [google map]
  // 3. [google ma]/[google map login] -> [...map login]
  // 4. [google map ]/[google map login] -> [...map login]
  size_t suggestion_last_space_index =
      sanitized_suggestion.find_last_of(base::kWhitespaceUTF16);
  size_t input_last_space_index =
      sanitized_input.find_last_of(base::kWhitespaceUTF16);
  if (suggestion_last_space_index == std::u16string::npos ||
      input_last_space_index == std::u16string::npos) {
    return sanitized_suggestion;
  }
  size_t start_index = input_last_space_index + 1;

  return sanitized_suggestion.substr(start_index);
}

}  // namespace

using OEP = metrics::OmniboxEventProto;

BaseSearchProvider::BaseSearchProvider(AutocompleteProvider::Type type,
                                       AutocompleteProviderClient* client)
    : AutocompleteProvider(type), client_(client) {}

// static
bool BaseSearchProvider::ShouldPrefetch(const AutocompleteMatch& match) {
  // TODO (manukh): `GetAdditionalInfoForDebugging()` shouldn't be used for
  //   non-debugging purposes.
  return match.GetAdditionalInfoForDebugging(kShouldPrefetchKey) == kTrue;
}

// static
bool BaseSearchProvider::ShouldPrerender(const AutocompleteMatch& match) {
  // TODO (manukh): `GetAdditionalInfoForDebugging()` shouldn't be used for
  //   non-debugging purposes.
  return match.GetAdditionalInfoForDebugging(kShouldPrerenderKey) == kTrue;
}

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
  match.image_dominant_color = suggestion.entity_info().dominant_color();
  match.image_url = GURL(suggestion.entity_info().image_url());
  match.entity_id = suggestion.entity_info().entity_id();
  match.website_uri = suggestion.entity_info().website_uri();

  match.contents = suggestion.match_contents();
  match.contents_class = suggestion.match_contents_class();
  if (OmniboxFieldTrial::kAnswerActionsShowRichCard.Get() &&
      suggestion.answer_template() &&
      suggestion.answer_template()->enhancements().enhancements().size() > 0) {
    match.suggestion_group_id = omnibox::GROUP_MOBILE_RICH_ANSWER;
  } else {
    match.suggestion_group_id = suggestion.suggestion_group_id();
  }
  match.answer = suggestion.answer();
  match.answer_template = suggestion.answer_template();
  match.answer_type = suggestion.answer_type();
  match.suggest_type = suggestion.suggest_type();
  for (const int subtype : suggestion.subtypes()) {
    match.subtypes.insert(SuggestSubtypeForNumber(subtype));
  }
  if (suggestion.type() == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    match.RecordAdditionalInfo(kACMatchPropertySuggestionText,
                               suggestion.suggestion());
    match.RecordAdditionalInfo(kACMatchPropertyContentsPrefix,
                               suggestion.match_contents_prefix());
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsStartIndex,
        static_cast<int>(suggestion.suggestion().length() -
                         match.contents.length()));
  }

  if (!suggestion.annotation().empty()) {
    match.description = suggestion.annotation();
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &match.description_class, 0, ACMatchClassification::NONE);
  }

  const std::u16string input_text = input.IsZeroSuggest() ? u"" : input.text();
  const std::u16string input_lower = base::i18n::ToLower(input_text);
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
      !input.IsZeroSuggest() &&
      base::StartsWith(base::i18n::ToLower(suggestion.suggestion()),
                       input_lower, base::CompareCase::SENSITIVE)) {
    match.inline_autocompletion =
        suggestion.suggestion().substr(input_text.length());
    match.allowed_to_be_default_match = true;
  }

  const TemplateURLRef& search_url = template_url->url_ref();
  DCHECK(search_url.SupportsReplacement(search_terms_data));
  std::u16string query(suggestion.suggestion());
  std::u16string original_query(input_text);
  if (suggestion.type() == AutocompleteMatchType::CALCULATOR) {
    // Use query text, rather than the calculator answer suggestion, to search.
    query = original_query;
    original_query.clear();
  }
  match.fill_into_edit = GetFillIntoEdit(suggestion, template_url);
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(query);
  match.search_terms_args->request_source = input.request_source();
  match.search_terms_args->original_query = original_query;
  match.search_terms_args->accepted_suggestion = accepted_suggestion;
  match.search_terms_args->additional_query_params =
      suggestion.entity_info().suggest_search_parameters();
  match.search_terms_args->append_extra_query_params_from_command_line =
      append_extra_query_params_from_command_line;
  // Must be set for deduplication and navigation. AutocompleteController will
  // ultimately overwrite this with the searchbox stats before navigation.
  match.destination_url = GURL(search_url.ReplaceSearchTerms(
      *match.search_terms_args, search_terms_data));

  // Search results don't look like URLs.
  match.transition = suggestion.from_keyword() ? ui::PAGE_TRANSITION_KEYWORD
                                               : ui::PAGE_TRANSITION_GENERATED;

  bool is_google = search::TemplateURLIsGoogle(template_url, search_terms_data);
  // Attach Actions in Suggest to the newly created match on Android if Google
  // is the default search engine.
  if ((is_android || is_ios) && is_google) {
    for (const omnibox::ActionInfo& action_info :
         suggestion.entity_info().action_suggestions()) {
      match.actions.emplace_back(CreateActionInSuggest(action_info, search_url,
                                                       *match.search_terms_args,
                                                       search_terms_data));
    }
  }

  if (is_android && is_google && suggestion.answer_template()) {
    base::ranges::transform(
        suggestion.answer_template()->enhancements().enhancements(),
        std::back_inserter(match.actions),
        [&](const omnibox::SuggestionEnhancement& enhancement) {
          return CreateAnswerAction(enhancement, *match.search_terms_args,
                                    suggestion.answer_type());
        });
  }

  match.navigational_intent = suggestion.navigational_intent();

  return match;
}

scoped_refptr<OmniboxAction> BaseSearchProvider::CreateActionInSuggest(
    omnibox::ActionInfo action_info,
    const TemplateURLRef& search_url,
    const TemplateURLRef::SearchTermsArgs& original_search_terms_args,
    const SearchTermsData& search_terms_data) {
  std::optional<TemplateURLRef::SearchTermsArgs> action_search_terms_args;
  // If the Action's URL is empty, but the Action supplies additional search
  // parameters, compute new URL based on the base URL (that is specific to
  // the entire suggestion).
  if (action_info.action_uri().empty() &&
      !action_info.search_parameters().empty()) {
    action_search_terms_args = original_search_terms_args;
    std::string query_params;
    for (const auto& param : action_info.search_parameters()) {
      // Supply additional Query Parameters as instructed by the provider.
      if (!query_params.empty()) {
        query_params += '&';
      }
      query_params += param.first + "=" + param.second;
    }
    action_search_terms_args->additional_query_params = query_params;
  }

  return base::MakeRefCounted<OmniboxActionInSuggest>(
      std::move(action_info), std::move(action_search_terms_args));
}

// static
scoped_refptr<OmniboxAction> BaseSearchProvider::CreateAnswerAction(
    omnibox::SuggestionEnhancement enhancement,
    TemplateURLRef::SearchTermsArgs search_terms_args,
    omnibox::AnswerType answer_type) {
  // Define actions destination URL.
  std::string query_params;
  for (const auto& param : enhancement.query_cgi_params()) {
    if (!query_params.empty()) {
      query_params += '&';
    }
    query_params += param.first + "=" + param.second;
  }
  search_terms_args.additional_query_params = query_params;
  search_terms_args.search_terms = base::UTF8ToUTF16(enhancement.query());

  return base::MakeRefCounted<OmniboxAnswerAction>(
      std::move(enhancement), search_terms_args, answer_type);
}

// static
AutocompleteMatch BaseSearchProvider::CreateShortcutSearchSuggestion(
    const std::u16string& suggestion,
    AutocompleteMatchType::Type type,
    bool from_keyword,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data) {
  // These calls use a number of default values.  For instance, they assume
  // that if this match is from a keyword provider, then the user is in keyword
  // mode.  They also assume the caller knows what it's doing and we set
  // this match to look as if it was received/created synchronously.
  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, type, /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, from_keyword,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/0, /*relevance_from_server=*/false,
      /*input_text=*/std::u16string());
  suggest_result.set_received_after_last_keystroke(false);
  return CreateSearchSuggestion(nullptr, AutocompleteInput(), from_keyword,
                                suggest_result, template_url, search_terms_data,
                                0, false);
}

// static
AutocompleteMatch BaseSearchProvider::CreateOnDeviceSearchSuggestion(
    AutocompleteProvider* autocomplete_provider,
    const AutocompleteInput& input,
    const std::u16string& suggestion,
    int relevance,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    int accepted_suggestion,
    bool is_tail_suggestion) {
  AutocompleteMatchType::Type match_type;
  omnibox::SuggestType suggest_type = omnibox::TYPE_NATIVE_CHROME;
  std::u16string match_contents, match_contents_prefix;

  if (is_tail_suggestion) {
    match_type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
    suggest_type = omnibox::TYPE_TAIL;
    std::u16string sanitized_suggestion =
        AutocompleteMatch::SanitizeString(suggestion);
    match_contents = GetMatchContentsForOnDeviceTailSuggestion(
        input.text(), sanitized_suggestion);

    DCHECK_GE(sanitized_suggestion.size(), match_contents.size());
    match_contents_prefix = sanitized_suggestion.substr(
        0, sanitized_suggestion.size() - match_contents.size());
  } else {
    match_type = AutocompleteMatchType::SEARCH_SUGGEST;
    suggest_type = omnibox::TYPE_QUERY;
    match_contents = suggestion;
  }

  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, match_type, suggest_type,
      /*subtypes=*/{omnibox::SUBTYPE_SUGGEST_2G_LITE}, match_contents,
      match_contents_prefix,
      /*annotation=*/std::u16string(),
      /*entity_info=*/omnibox::EntityInfo(),
      /*deletion_url=*/"",
      /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, relevance,
      /*relevance_from_server=*/false,
      /*should_prefetch=*/false,
      /*should_prerender=*/false,
      base::CollapseWhitespace(input.text(), false));
  // On device providers are asynchronous.
  suggest_result.set_received_after_last_keystroke(true);
  return CreateSearchSuggestion(
      autocomplete_provider, input, /*in_keyword_mode=*/false, suggest_result,
      template_url, search_terms_data, accepted_suggestion,
      /*append_extra_query_params_from_command_line=*/true);
}

// static
bool BaseSearchProvider::PageURLIsEligibleForSuggestRequest(
    const GURL& page_url) {
  return page_url.is_valid() && page_url.SchemeIsHTTPOrHTTPS();
}

// static
bool BaseSearchProvider::CanSendSuggestRequestWithoutPageURL(
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    const AutocompleteProviderClient* client) {
  // Make sure we are sending the suggest request through a cryptographically
  // secure channel to prevent exposing the current page URL or personalized
  // results without encryption.
  const GURL& suggest_url =
      template_url->GenerateSuggestionURL(search_terms_data);
  if (!suggest_url.is_valid() || !suggest_url.SchemeIsCryptographic()) {
    return false;
  }

  // Don't make a suggest request if in incognito mode.
  if (client->IsOffTheRecord()) {
    return false;
  }

  // Don't make a suggest request if suggest is not enabled.
  if (!client->SearchSuggestEnabled()) {
    return false;
  }

  // Don't make a suggest request if Google is not the default search engine.
  // Note that currently only the pre-populated Google search provider supports
  // zero-prefix suggestions. If other pre-populated search engines decide to
  // support it, revise this test accordingly.
  if (!search::TemplateURLIsGoogle(template_url, search_terms_data)) {
    return false;
  }

  return true;
}

// static
bool BaseSearchProvider::CanSendSuggestRequestWithPageURL(
    const GURL& current_page_url,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    const AutocompleteProviderClient* client) {
  if (!CanSendSuggestRequestWithoutPageURL(template_url, search_terms_data,
                                           client)) {
    return false;
  }

  // Forbid sending the current page URL to the suggest endpoint if personalized
  // URL data collection is off; unless the current page is the provider's
  // Search Results Page.
  return template_url->IsSearchURL(current_page_url, search_terms_data) ||
         client->IsPersonalizedUrlDataCollectionActive();
}

void BaseSearchProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  // TODO (manukh): `GetAdditionalInfoForDebugging()` shouldn't be used for
  //   non-debugging purposes.
  if (!match.GetAdditionalInfoForDebugging(BaseSearchProvider::kDeletionUrlKey)
           .empty()) {
    deletion_loaders_.push_back(
        client()
            ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
            ->StartDeletionRequest(
                match.GetAdditionalInfoForDebugging(
                    BaseSearchProvider::kDeletionUrlKey),
                base::BindOnce(&BaseSearchProvider::OnDeletionComplete,
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
}

// static
const char BaseSearchProvider::kRelevanceFromServerKey[] =
    "relevance_from_server";
const char BaseSearchProvider::kShouldPrefetchKey[] = "should_prefetch";
const char BaseSearchProvider::kShouldPrerenderKey[] = "should_prerender";
const char BaseSearchProvider::kDeletionUrlKey[] = "deletion_url";
const char BaseSearchProvider::kTrue[] = "true";
const char BaseSearchProvider::kFalse[] = "false";

BaseSearchProvider::~BaseSearchProvider() = default;

// static
std::u16string BaseSearchProvider::GetFillIntoEdit(
    const SearchSuggestionParser::SuggestResult& suggest_result,
    const TemplateURL* template_url) {
  std::u16string fill_into_edit;

  if (suggest_result.from_keyword())
    fill_into_edit.append(template_url->keyword() + u' ');

  fill_into_edit.append(suggest_result.suggestion());

  return fill_into_edit;
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
  url = url.DeprecatedGetOriginAsURL().Resolve(deletion_url);
  if (url.is_valid()) {
    match->RecordAdditionalInfo(BaseSearchProvider::kDeletionUrlKey,
                                url.spec());
    match->deletable = true;
  }
}

void BaseSearchProvider::AddMatchToMap(
    const SearchSuggestionParser::SuggestResult& result,
    const AutocompleteInput& input,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    int accepted_suggestion,
    bool mark_as_deletable,
    bool in_keyword_mode,
    MatchMap* map) {
  AutocompleteMatch match = CreateSearchSuggestion(
      this, input, in_keyword_mode, result, template_url, search_terms_data,
      accepted_suggestion, ShouldAppendExtraParams(result));
  if (!match.destination_url.is_valid())
    return;
  match.RecordAdditionalInfo(kRelevanceFromServerKey,
                             result.relevance_from_server() ? kTrue : kFalse);
  match.RecordAdditionalInfo(kShouldPrefetchKey,
                             result.should_prefetch() ? kTrue : kFalse);
  match.RecordAdditionalInfo(kShouldPrerenderKey,
                             result.should_prerender() ? kTrue : kFalse);
  SetDeletionURL(result.deletion_url(), &match);
  if (mark_as_deletable)
    match.deletable = true;

  // Only set scoring signals for eligible matches.
  if (match.IsMlSignalLoggingEligible()) {
    // Initialize the ML scoring signals for this suggestion if needed.
    if (!match.scoring_signals) {
      match.scoring_signals = std::make_optional<ScoringSignals>();
    }

    if (result.relevance_from_server()) {
      match.scoring_signals->set_search_suggest_relevance(result.relevance());
    }
    SearchScoringSignalsAnnotator::UpdateMatchTypeScoringSignals(match,
                                                                 input.text());
  }

  // Try to add `match` to `map`.
  // NOTE: Keep this ToLower() call in sync with url_database.cc.
  MatchKey match_key(
      std::make_tuple(base::i18n::ToLower(result.suggestion()),
                      match.search_terms_args->additional_query_params));
  const std::pair<MatchMap::iterator, bool> i(
      map->insert(std::make_pair(match_key, match)));
  if (i.second) {
    auto& added_match = i.first->second;
    // If the newly added match has non-empty additional query params and
    // another match with the same search terms and a unique non-empty
    // additional query params is already present in the map, proactively set
    // `stripped_destination_url` to be the same as `destination_url`.
    // Otherwise, `stripped_destination_url` will later be set by
    // `AutocompleteResult::ComputeStrippedDestinationURL()` which strips away
    // the additional query params from `destination_url` leaving only the
    // search terms. That would result in these matches to be erroneously
    // deduped despite having unique additional query params.
    // Note that the match previously added to the map will continue to get the
    // typical `stripped_destination_url` allowing it to be deduped with the
    // plain-text matches (i.e., with no additional query params) as expected.
    const auto& added_match_query = std::get<0>(match_key);
    const auto& added_match_query_params = std::get<1>(match_key);
    if (!added_match_query_params.empty()) {
      for (const auto& entry : *map) {
        const auto& existing_match_query = std::get<0>(entry.first);
        const auto& existing_match_query_params = std::get<1>(entry.first);
        if (existing_match_query == added_match_query &&
            !existing_match_query_params.empty() &&
            existing_match_query_params != added_match_query_params) {
          added_match.stripped_destination_url = added_match.destination_url;
          break;
        }
      }
    }
  } else {
    auto& existing_match = i.first->second;
    // If a duplicate match is already in the map, replace it with `match` if it
    // is more relevant.
    // NOTE: We purposefully do a direct relevance comparison here instead of
    // using AutocompleteMatch::MoreRelevant(), so that we'll prefer "items
    // added first" rather than "items alphabetically first" when the scores
    // are equal. The only case this matters is when a user has results with
    // the same score that differ only by capitalization; because the history
    // system returns results sorted by recency, this means we'll pick the most
    // recent such result even if the precision of our relevance score is too
    // low to distinguish the two.
    if (match.relevance > existing_match.relevance) {
      match.duplicate_matches.insert(match.duplicate_matches.end(),
                                     existing_match.duplicate_matches.begin(),
                                     existing_match.duplicate_matches.end());
      existing_match.duplicate_matches.clear();
      match.duplicate_matches.push_back(existing_match);
      existing_match = std::move(match);
    } else {
      if (match.keyword == existing_match.keyword) {
        // Old and new matches are from the same search provider. It is okay to
        // record one match's prefetch/prerender data onto a different match
        // (for the same query string) for the following reasons:
        // 1. Because the suggest server only sends down a query string from
        // which we construct a URL, rather than sending a full URL, and because
        // we construct URLs from query strings in the same way every time, the
        // URLs for the two matches will be the same. Therefore, we won't end up
        // prefetching/prerendering something the server didn't intend.
        // 2. Presumably the server sets the prefetch/prerender bit on a match
        // it thinks is sufficiently relevant that the user is likely to choose
        // it. Surely setting the prefetch/prerender bit on a match of even
        // higher relevance won't violate this assumption.
        const bool should_prefetch =
            result.should_prefetch() || ShouldPrefetch(existing_match);
        existing_match.RecordAdditionalInfo(kShouldPrefetchKey,
                                            should_prefetch ? kTrue : kFalse);
        const bool should_prerender =
            result.should_prerender() || ShouldPrerender(existing_match);
        existing_match.RecordAdditionalInfo(kShouldPrerenderKey,
                                            should_prerender ? kTrue : kFalse);
      }
      existing_match.duplicate_matches.push_back(std::move(match));
    }

    // Copy over necessary fields from the lower-ranking duplicate. Note that
    // this requires the lower-ranking duplicate being added last. See the use
    // of push_back above:

    // This is to avoid losing the Answers in Suggest information.
    const auto& less_relevant_duplicate_match =
        existing_match.duplicate_matches.back();
    if (less_relevant_duplicate_match.answer && !existing_match.answer) {
      existing_match.answer = less_relevant_duplicate_match.answer;
      existing_match.answer_type = less_relevant_duplicate_match.answer_type;
      if (OmniboxFieldTrial::kAnswerActionsShowRichCard.Get()) {
        existing_match.suggestion_group_id =
            less_relevant_duplicate_match.suggestion_group_id;
      }
    }
    if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled &&
        less_relevant_duplicate_match.answer_template &&
        !existing_match.answer_template) {
      existing_match.actions = less_relevant_duplicate_match.actions;
      existing_match.answer_template =
          less_relevant_duplicate_match.answer_template;
      existing_match.answer_type = less_relevant_duplicate_match.answer_type;
      if (OmniboxFieldTrial::kAnswerActionsShowRichCard.Get()) {
        existing_match.suggestion_group_id =
            less_relevant_duplicate_match.suggestion_group_id;
      }
    }
    // This is to avoid having shopping categorical queries lose their images to
    // higher-relevance local history and verbatim matches. This works for the
    // shopping categorical queries because they only provide images at the
    // moment. That assumption may not hold in the future.
    // Ideally the entire `entity_info`, when available on a suggestion, should
    // be copied over. However `entity_info` is broken down to its constituents
    // in the constructor of SearchSuggestionParser::SuggestResult and used to
    // set individual fields on the AutocompleteMatch. This is in contrast to
    // Answers in Suggest which is kept on the match in its entirety. This is
    // partly because the entity name is used to set and classify the match
    // contents. Ideally `entity_info` should also be kept on the match in its
    // entirety so it can be carried over when deduplicating the matches here or
    // later in the Autocomplete process.
    // TODO(crbug.com/40276602): rework how `entity_info` is used in the match.
    if (base::FeatureList::IsEnabled(omnibox::kCategoricalSuggestions)) {
      if (!less_relevant_duplicate_match.image_url.is_empty() &&
          existing_match.image_url.is_empty()) {
        existing_match.image_url = less_relevant_duplicate_match.image_url;
      }
    }
    // This is to avoid having shopping categorical queries lose their subtypes
    // to higher-relevance local history and verbatim matches. The subtypes are
    // sent to the backend in the ChromeSearchboxStats proto via the gs_lcrp=
    // param when the user selects a suggestion. The subtypes may be used to
    // identify what the user selected so they can be suggested the next time,
    // i.e., if the user selects a decorated suggestion - which is accompanied
    // by specific subtypes - we want to show a decorated suggestion next time.
    if (base::FeatureList::IsEnabled(omnibox::kCategoricalSuggestions) &&
        base::FeatureList::IsEnabled(omnibox::kMergeSubtypes)) {
      existing_match.subtypes.insert(
          less_relevant_duplicate_match.subtypes.begin(),
          less_relevant_duplicate_match.subtypes.end());
    }
    // This is to avoid having `stripped_destination_url` being later set by
    // `AutocompleteResult::ComputeStrippedDestinationURL()` which strips away
    // the additional query params from `destination_url` leaving only the
    // search terms. That would result in these matches to be erroneously
    // deduped despite having unique additional query params.
    if (!less_relevant_duplicate_match.stripped_destination_url.is_empty() &&
        existing_match.stripped_destination_url.is_empty()) {
      existing_match.stripped_destination_url =
          less_relevant_duplicate_match.stripped_destination_url;
    }
  }
}

void BaseSearchProvider::DeleteMatchFromMatches(
    const AutocompleteMatch& match) {
  for (auto i(matches_.begin()); i != matches_.end(); ++i) {
    // Find the desired match to delete by checking the type and contents.
    // We can't check the destination URL, because the autocomplete controller
    // may have reformulated that. Not that while checking for matching
    // contents works for personalized suggestions, if more match types gain
    // deletion support, this algorithm may need to be re-examined.

    if (MatchTypeAndContentsAreEqual(match, *i)) {
      matches_.erase(i);
      break;
    }

    // Handle the case where the deleted match is only found within the
    // duplicate_matches sublist.
    std::vector<AutocompleteMatch>& duplicates = i->duplicate_matches;
    auto it =
        std::remove_if(duplicates.begin(), duplicates.end(),
                       [&match](const AutocompleteMatch& duplicate) {
                         return MatchTypeAndContentsAreEqual(match, duplicate);
                       });
    if (it != duplicates.end()) {
      duplicates.erase(it, duplicates.end());
      break;
    }
  }
}

void BaseSearchProvider::OnDeletionComplete(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::unique_ptr<std::string> response_body) {
  RecordDeletionResult(response_code == 200);
  std::erase_if(
      deletion_loaders_,
      [source](const std::unique_ptr<network::SimpleURLLoader>& loader) {
        return loader.get() == source;
      });
}
