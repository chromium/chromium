// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/featured_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <climits>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// Max number of featured enterprise suggestions to show when the user types '@'
// or '@...'. 4 is a good limit because:
// - When the user types '@', the existing 4 starter packs, 1 trivial search,
//   and 4 enterprise suggestions will all fit in the total limit of 9
//   suggestions. This may change if more starter packs are launched.
// - When the user types '@...', the at-most-1 matching starter pack (no
//   starter packs share the same 1st character), 1 trivial search, at least 2
//   non-trivial searches, and 4 enterprise suggestions will fit in the total
//   limit of 8 suggestions.
// This constant can be replaced with a function if we want to show a different
// # of enterprise suggestions in these 2 cases.
constexpr int kMaxEnterpriseSuggestions = 4;

// Scored higher than history URL provider suggestions since inputs like '@b'
// would default 'bing.com' instead (history URL provider seems to ignore '@'
// prefix in the input). Featured Enterprise search ranks higher than "ask
// google" suggestions, which ranks higher than the other starter pack
// suggestions.
constexpr int kFeaturedEnterpriseSearchRelevance = 1470;
constexpr int kGeminiRelevance = 1460;
constexpr int kStarterPackRelevance = 1450;
// IPH suggestions are grouped after all other suggestions. But they still
// need to score within top N suggestions to be shown.
constexpr int kIPHRelevance = 5000;

std::string GetIphDismissedPrefNameFor(IphType iph_type) {
  switch (iph_type) {
    case IphType::kNone:
      NOTREACHED();
    case IphType::kGemini:
      return omnibox::kDismissedGeminiIph;
    case IphType::kFeaturedEnterpriseSearch:
      return omnibox::kDismissedFeaturedEnterpriseSiteSearchIphPrefName;
    case IphType::kHistoryEmbeddingsSettingsPromo:
      return omnibox::kDismissedHistoryEmbeddingsSettingsPromo;
    case IphType::kHistoryEmbeddingsDisclaimer:
      NOTREACHED();  // This is a non-dismissible disclaimer.
    case IphType::kHistoryScopePromo:
      return omnibox::kDismissedHistoryScopePromo;
    case IphType::kHistoryEmbeddingsScopePromo:
      return omnibox::kDismissedHistoryEmbeddingsScopePromo;
  }
}

std::string GetIphShownCountPrefNameFor(IphType iph_type) {
  switch (iph_type) {
    case IphType::kNone:
      NOTREACHED();
    case IphType::kGemini:
      return omnibox::kShownCountGeminiIph;
    case IphType::kFeaturedEnterpriseSearch:
      return omnibox::kShownCountFeaturedEnterpriseSiteSearchIph;
    case IphType::kHistoryEmbeddingsSettingsPromo:
      return omnibox::kShownCountHistoryEmbeddingsSettingsPromo;
    case IphType::kHistoryEmbeddingsDisclaimer:
      NOTREACHED();  // This disclaimer has no show count limit.
    case IphType::kHistoryScopePromo:
      return omnibox::kShownCountHistoryScopePromo;
    case IphType::kHistoryEmbeddingsScopePromo:
      return omnibox::kShownCountHistoryEmbeddingsScopePromo;
  }
}

std::string IphTypeDebugString(IphType iph_type) {
  switch (iph_type) {
    case IphType::kNone:
      NOTREACHED();
    case IphType::kGemini:
      return "gemini";
    case IphType::kFeaturedEnterpriseSearch:
      return "featured enterprise search";
    case IphType::kHistoryEmbeddingsSettingsPromo:
      return "history embeddings settings promo";
    case IphType::kHistoryEmbeddingsDisclaimer:
      return "history embeddings disclaimer";
    case IphType::kHistoryScopePromo:
      return "history scope promo";
    case IphType::kHistoryEmbeddingsScopePromo:
      return "history embeddings scope promo";
  }
}

bool IsEnterpriseSearchTemplateURLEnabled(const TemplateURL& turl,
                                          bool is_incognito) {
  return !(is_incognito && turl.CreatedByEnterpriseSearchAggregatorPolicy());
}

}  // namespace

FeaturedSearchProvider::FeaturedSearchProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_FEATURED_SEARCH),
      client_(client) {
  template_url_service_ = client->GetTemplateURLService();
}

void FeaturedSearchProvider::Start(const AutocompleteInput& input,
                                   bool minimal_changes) {
  matches_.clear();
  if (input.IsZeroSuggest())
    iph_shown_in_omnibox_session_ = false;

  AutocompleteInput keyword_input = input;
  const TemplateURL* keyword_turl =
      AutocompleteInput::GetSubstitutingTemplateURLForInput(
          template_url_service_, &keyword_input);
  bool is_history_scope =
      keyword_turl &&
      keyword_turl->starter_pack_id() == TemplateURLStarterPackData::kHistory;

  if (is_history_scope) {
    if (ShouldShowHistoryEmbeddingsDisclaimerIphMatch()) {
      AddHistoryEmbeddingsDisclaimerIphMatch();
    } else if (ShouldShowHistoryEmbeddingsSettingsPromoIphMatch()) {
      AddHistoryEmbeddingsSettingsPromoIphMatch();
    }
    return;
  }

  if (input.IsZeroSuggest()) {
    if (ShouldShowEnterpriseFeaturedSearchIPHMatch()) {
      AddFeaturedEnterpriseSearchIPHMatch();
    } else if (ShouldShowGeminiIPHMatch()) {
      AddGeminiIPHMatch();
    } else if (ShouldShowHistoryScopePromoIphMatch()) {
      AddHistoryScopePromoIphMatch();
    } else if (ShouldShowHistoryEmbeddingsScopePromoIphMatch()) {
      AddHistoryEmbeddingsScopePromoIphMatch();
    }
  }

  AddFeaturedKeywordMatches(input);
}

void FeaturedSearchProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Only `NULL_RESULT_MESSAGE` types from this provider are deletable.
  CHECK(match.deletable);
  CHECK_EQ(match.type, AutocompleteMatchType::NULL_RESULT_MESSAGE);

  // Set the pref so this provider doesn't continue to offer the suggestion.
  PrefService* prefs = client_->GetPrefs();
  CHECK_NE(match.iph_type, IphType::kNone);
  prefs->SetBoolean(GetIphDismissedPrefNameFor(match.iph_type), true);

  // Delete `match` from `matches_`.
  std::erase_if(matches_, [&match](const auto& i) {
    return i.contents == match.contents;
  });
}

void FeaturedSearchProvider::RegisterDisplayedMatches(
    const AutocompleteResult& result) {
  auto iph_match = std::ranges::find_if(result, [](const auto& match) {
    return match.iph_type != IphType::kNone;
  });
  IphType iph_type =
      iph_match == result.end() ? IphType::kNone : iph_match->iph_type;

  // `kHistoryEmbeddingsDisclaimer` has no shown limit.
  if (!iph_shown_in_omnibox_session_ && iph_type != IphType::kNone &&
      iph_type != IphType::kHistoryEmbeddingsDisclaimer) {
    PrefService* prefs = client_->GetPrefs();
    // `ShouldShowIPH()` shouldn't allow adding IPH matches if there is no
    // `prefs`.
    CHECK(prefs);
    prefs->SetInteger(
        GetIphShownCountPrefNameFor(iph_type),
        prefs->GetInteger(GetIphShownCountPrefNameFor(iph_type)) + 1);
    iph_shown_in_browser_session_count_++;
    iph_shown_in_omnibox_session_ = true;
  }
}

FeaturedSearchProvider::~FeaturedSearchProvider() = default;

void FeaturedSearchProvider::AddFeaturedKeywordMatches(
    const AutocompleteInput& input) {
  size_t enterprise_count = 0;
  if (input.GetFeaturedKeywordMode() !=
      AutocompleteInput::FeaturedKeywordMode::kFalse) {
    TemplateURLService::TemplateURLVector turls;
    template_url_service_->AddMatchingKeywords(input.text(), false, &turls);
    for (TemplateURL* turl : turls) {
      if (turl->starter_pack_id() > 0 &&
          turl->is_active() == TemplateURLData::ActiveStatus::kTrue) {
        // Don't add the expanded set of starter pack engines unless the feature
        // is enabled.
        if ((turl->starter_pack_id() == TemplateURLStarterPackData::kGemini &&
             !OmniboxFieldTrial::IsStarterPackExpansionEnabled()) ||
            (turl->starter_pack_id() == TemplateURLStarterPackData::kPage &&
             !omnibox_feature_configs::ContextualSearch::Get()
                  .starter_pack_page)) {
          continue;
        }
        AddStarterPackMatch(*turl, input);
        // Don't add enterprise search aggregator engines in incognito mode.
      } else if (turl->featured_by_policy() &&
                 IsEnterpriseSearchTemplateURLEnabled(
                     *turl, client_->IsOffTheRecord()) &&
                 enterprise_count < kMaxEnterpriseSuggestions) {
        AddFeaturedEnterpriseSearchMatch(*turl, input);
        enterprise_count++;
      }
    }
  }
}

void FeaturedSearchProvider::AddStarterPackMatch(
    const TemplateURL& template_url,
    const AutocompleteInput& input) {
  // The history starter pack engine is disabled in incognito mode.
  if (client_->IsOffTheRecord() &&
      template_url.starter_pack_id() == TemplateURLStarterPackData::kHistory) {
    return;
  }

  // The starter pack relevance score is currently ranked above
  // search-what-you-typed suggestion to avoid the keyword mode chip attaching
  // to the search suggestion instead of Builtin suggestions.
  // TODO(yoangela): This should be updated so the keyword chip only attaches to
  //  STARTER_PACK type suggestions rather than rely on out-scoring all other
  //  suggestions.
  AutocompleteMatch match(this, kStarterPackRelevance, false,
                          AutocompleteMatchType::STARTER_PACK);

  const std::u16string destination_url =
      TemplateURLStarterPackData::GetDestinationUrlForStarterPackID(
          template_url.starter_pack_id());
  match.fill_into_edit = template_url.keyword();
  if (match.fill_into_edit.starts_with(input.text())) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(input.text().length());
  }
  match.destination_url = GURL(destination_url);
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  if (input.current_page_classification() !=
          metrics::OmniboxEventProto::NTP_REALBOX &&
      template_url.keyword().starts_with(u'@')) {
    // The Gemini provider doesn't follow the "Search X" pattern and should
    // also be ranked first.
    // TODO(b/41494524): Currently templateurlservice returns the keywords in
    //  alphabetical order, which is the order we rank them. There should be a
    //  more sustainable way for specifying the order they should appear in the
    //  omnibox.
    if (OmniboxFieldTrial::IsStarterPackExpansionEnabled() &&
        template_url.starter_pack_id() == TemplateURLStarterPackData::kGemini) {
      match.description = l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_INSTANT_KEYWORD_ASK_TEXT, template_url.keyword(),
          template_url.short_name());
      match.relevance = kGeminiRelevance;
    } else {
      std::u16string short_name = template_url.short_name();
      if (template_url.short_name() == u"Tabs") {
        // Very special request from UX to sentence-case "Tabs" -> "tabs" only
        // in this context. It needs to stay capitalized elsewhere since it's
        // treated like a proper engine name.
        match.description = short_name = u"tabs";
      }
      match.description =
          l10n_util::GetStringFUTF16(IDS_OMNIBOX_INSTANT_KEYWORD_SEARCH_TEXT,
                                     template_url.keyword(), short_name);
    }
    match.description_class = {
        {0, ACMatchClassification::NONE},
        {template_url.keyword().size(), ACMatchClassification::DIM}};
    match.contents.clear();
    match.contents_class = {{}};
    match.allowed_to_be_default_match = false;
    match.keyword = template_url.keyword();
  } else {
    // The Gemini provider should be ranked first.
    // TODO(b/41494524): Currently templateurlservice returns the keywords in
    //  alphabetical order, which is the order we rank them. There should be a
    //  more sustainable way for specifying the order they should appear in the
    //  omnibox.
    if (OmniboxFieldTrial::IsStarterPackExpansionEnabled() &&
        template_url.starter_pack_id() == TemplateURLStarterPackData::kGemini) {
      match.relevance = kGeminiRelevance;
    }
    match.description = template_url.short_name();
    match.description_class.emplace_back(0, ACMatchClassification::NONE);
    match.contents = destination_url;
    match.contents_class.emplace_back(0, ACMatchClassification::URL);
    match.SetAllowedToBeDefault(input);
  }
  matches_.push_back(match);
}

void FeaturedSearchProvider::AddIPHMatch(IphType iph_type,
                                         const std::u16string& iph_contents,
                                         const std::u16string& matched_term,
                                         const std::u16string& iph_link_text,
                                         const GURL& iph_link_url,
                                         int relevance,
                                         bool deletable) {
  AutocompleteMatch match(this, relevance, deletable,
                          AutocompleteMatchType::NULL_RESULT_MESSAGE);

  // Use this suggestion's contents field to display a message to the user that
  // cannot be acted upon.
  match.contents = iph_contents;
  CHECK_NE(iph_type, IphType::kNone);
  match.iph_type = iph_type;
  match.iph_link_text = iph_link_text;
  match.iph_link_url = iph_link_url;
  match.suggestion_group_id = omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP;
  match.RecordAdditionalInfo("iph type", IphTypeDebugString(iph_type));
  match.RecordAdditionalInfo("trailing iph link text", iph_link_text);
  match.RecordAdditionalInfo("trailing iph link url", iph_link_url.spec());

  // Bolds just the portion of the IPH string corresponding to `matched_terms`.
  // The rest of the string is dimmed.
  TermMatches term_matches =
      matched_term.empty() ? TermMatches()
                           : MatchTermInString(matched_term, match.contents, 0);
  match.contents_class = ClassifyTermMatches(
      term_matches, match.contents.size(), ACMatchClassification::MATCH,
      ACMatchClassification::DIM);

  matches_.push_back(match);
}

void FeaturedSearchProvider::AddFeaturedEnterpriseSearchMatch(
    const TemplateURL& template_url,
    const AutocompleteInput& input) {
  if (input.current_page_classification() ==
      metrics::OmniboxEventProto::NTP_REALBOX) {
    return;
  }
  if (!IsEnterpriseSearchTemplateURLEnabled(template_url,
                                            client_->IsOffTheRecord())) {
    return;
  }

  AutocompleteMatch match(this, kFeaturedEnterpriseSearchRelevance, false,
                          AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH);

  match.fill_into_edit = template_url.keyword();
  match.inline_autocompletion =
      match.fill_into_edit.substr(input.text().length());
  match.destination_url = GURL(template_url.url());
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  match.description = l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_INSTANT_KEYWORD_SEARCH_TEXT, template_url.keyword(),
      template_url.short_name());
  match.description_class = {
      {0, ACMatchClassification::NONE},
      {template_url.keyword().size(), ACMatchClassification::DIM}};
  match.contents.clear();
  match.contents_class = {{}};
  match.allowed_to_be_default_match = false;
  match.keyword = template_url.keyword();
  if (template_url.CreatedByEnterpriseSearchAggregatorPolicy()) {
    match.icon_url = template_url.favicon_url();
  }

  matches_.push_back(match);
}

bool FeaturedSearchProvider::ShouldShowIPH(IphType iph_type) const {
  PrefService* prefs = client_->GetPrefs();
  // Check the IPH hasn't been dismissed.
  if (!prefs || prefs->GetBoolean(GetIphDismissedPrefNameFor(iph_type))) {
    return false;
  }

  // The limit only applies once per session. E.g., when the user types
  // '@history a', the `kHistoryEmbeddingsSettingsPromo` IPH might be shown.
  // When they then type '@history abcdefg', the IPH should continue to be
  // shown, and not disappear at '@history abcd', and only count as 1 shown.
  if (iph_shown_in_omnibox_session_) {
    return true;
  }

  // Check the IPH hasn't reached its show limit. Check too many IPHs haven't
  // been shown this session; don't want to show 3 of type 1, then 3 of type 2
  // immediately after.
  size_t iph_shown_count =
      prefs->GetInteger(GetIphShownCountPrefNameFor(iph_type));
  return iph_shown_count < 3 && iph_shown_in_browser_session_count_ < 3;
}

bool FeaturedSearchProvider::ShouldShowGeminiIPHMatch() const {
  if (!OmniboxFieldTrial::IsStarterPackIPHEnabled() ||
      !ShouldShowIPH(IphType::kGemini)) {
    return false;
  }

  // The @gemini IPH should no longer be shown once a user has successfully
  // used @gemini.
  TemplateURL* gemini_turl = template_url_service_->FindStarterPackTemplateURL(
      TemplateURLStarterPackData::kGemini);
  if (gemini_turl && gemini_turl->usage_count() > 0) {
    return false;
  }

  return true;
}

void FeaturedSearchProvider::AddGeminiIPHMatch() {
  AddIPHMatch(
      IphType::kGemini,
      /*iph_contents=*/l10n_util::GetStringUTF16(IDS_OMNIBOX_GEMINI_IPH),
      /*matched_term=*/u"@gemini",
      /*iph_link_text=*/u"",
      /*iph_link_url=*/{},
      /*relevance=*/omnibox::kIPHZeroSuggestRelevance,
      /*deletable=*/true);
}

bool FeaturedSearchProvider::ShouldShowEnterpriseFeaturedSearchIPHMatch()
    const {
  // Conditions to show the IPH for featured Enterprise search:
  // - The feature is enabled.
  // - There is at least one featured search engine set by policy, excluding
  //   featured search engines created by enterprise search aggregator policy
  //   when in incognito mode.
  // - The user has not deleted the IPH suggestion and we have not shown it more
  //   than the accepted limit during this session.
  // - The user has not successfully used at least one featured engine.
  TemplateURLService::TemplateURLVector featured_engines;
  for (TemplateURL* turl :
       template_url_service_->GetFeaturedEnterpriseSearchEngines()) {
    if (IsEnterpriseSearchTemplateURLEnabled(*turl,
                                             client_->IsOffTheRecord())) {
      featured_engines.push_back(turl);
    }
  }
  return !featured_engines.empty() &&
         ShouldShowIPH(IphType::kFeaturedEnterpriseSearch) &&
         std::ranges::all_of(featured_engines, [](auto turl) {
           return turl->usage_count() == 0;
         });
}

void FeaturedSearchProvider::AddFeaturedEnterpriseSearchIPHMatch() {
  std::vector<std::string> sites;
  for (const TemplateURL* turl :
       template_url_service_->GetFeaturedEnterpriseSearchEngines()) {
    if (IsEnterpriseSearchTemplateURLEnabled(*turl,
                                             client_->IsOffTheRecord())) {
      sites.push_back(url_formatter::StripWWW(GURL(turl->url()).host()));
    }
  }
  std::ranges::sort(sites);
  AddIPHMatch(IphType::kFeaturedEnterpriseSearch,
              /*iph_contents=*/
              l10n_util::GetStringFUTF16(
                  IDS_OMNIBOX_FEATURED_ENTERPRISE_SITE_SEARCH_IPH,
                  base::UTF8ToUTF16(base::JoinString(sites, ", "))),
              /*matched_term=*/u"",
              /*iph_link_text=*/u"",
              /*iph_link_url=*/{},
              /*relevance=*/omnibox::kIPHZeroSuggestRelevance,
              /*deletable=*/true);
}

bool FeaturedSearchProvider::ShouldShowHistoryEmbeddingsSettingsPromoIphMatch()
    const {
  // Assumes this is only called when the user is in @history scope.
  // Additional conditions:
  // - The settings is available - no need to ask the user to enable a setting
  //   that doesn't exist.
  // - The setting isn't already enabled - no need to the user to enable a
  //   setting that's already enabled.
  // - The feature is allowed in the omnibox.
  // - The user has not deleted the IPH suggestion.
  return client_->IsHistoryEmbeddingsSettingVisible() &&
         !client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::GetFeatureParameters().omnibox_scoped &&
         ShouldShowIPH(IphType::kHistoryEmbeddingsSettingsPromo);
}

void FeaturedSearchProvider::AddHistoryEmbeddingsSettingsPromoIphMatch() {
  std::u16string text = l10n_util::GetStringUTF16(
                            IDS_OMNIBOX_HISTORY_EMBEDDINGS_SETTINGS_PROMO_IPH) +
                        u" ";
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_OMNIBOX_HISTORY_EMBEDDINGS_SETTINGS_PROMO_IPH_LINK_TEXT);
  AddIPHMatch(IphType::kHistoryEmbeddingsSettingsPromo,
              /*iph_contents=*/text,
              /*matched_term=*/u"",
              /*iph_link_text=*/link_text,
              /*iph_link_url=*/GURL("chrome://settings/ai/historySearch"),
              /*relevance=*/kIPHRelevance,
              /*deletable=*/true);
}

bool FeaturedSearchProvider::ShouldShowHistoryEmbeddingsDisclaimerIphMatch()
    const {
  // Assumes this is only called when the user is in @history scope. Not limited
  // by `ShouldShowIPH()` (i.e. shown count or dismissal) because this is a
  // disclaimer.
  return client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::GetFeatureParameters().omnibox_scoped;
}

void FeaturedSearchProvider::AddHistoryEmbeddingsDisclaimerIphMatch() {
  std::u16string text =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_EMBEDDINGS_DISCLAIMER_IPH) +
      u" ";
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_OMNIBOX_HISTORY_EMBEDDINGS_DISCLAIMER_IPH_LINK_TEXT);
  AddIPHMatch(IphType::kHistoryEmbeddingsDisclaimer,
              /*iph_contents=*/text,
              /*matched_term=*/u"",
              /*iph_link_text=*/link_text,
              /*iph_link_url=*/GURL("chrome://settings/ai/historySearch"),
              /*relevance=*/kIPHRelevance,
              /*deletable=*/false);
}

bool FeaturedSearchProvider::ShouldShowHistoryScopePromoIphMatch() const {
  // Shown in the zero state when history embeddings is disabled (not opted-in),
  // but the embeddings is enabled for the omnibox. Doesn't check if the setting
  // is visible. We want to guard this behind some meaningful param but it's not
  // directly related to embeddings so it's ok to show to users who can't opt-in
  // to embeddings.
  return !client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::GetFeatureParameters().omnibox_scoped &&
         !client_->IsOffTheRecord() &&
         ShouldShowIPH(IphType::kHistoryScopePromo);
}

void FeaturedSearchProvider::AddHistoryScopePromoIphMatch() {
  AddIPHMatch(IphType::kHistoryScopePromo,
              /*iph_contents=*/
              l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_SCOPE_PROMO_IPH),
              /*matched_term=*/u"@history",
              /*iph_link_text=*/u"",
              /*iph_link_url=*/{},
              /*relevance=*/omnibox::kIPHZeroSuggestRelevance,
              /*deletable=*/true);
}

bool FeaturedSearchProvider::ShouldShowHistoryEmbeddingsScopePromoIphMatch()
    const {
  // Shown when history embeddings is enabled (& opted-in) for the omnibox.
  return client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::GetFeatureParameters().omnibox_scoped &&
         ShouldShowIPH(IphType::kHistoryEmbeddingsScopePromo);
}

void FeaturedSearchProvider::AddHistoryEmbeddingsScopePromoIphMatch() {
  AddIPHMatch(
      IphType::kHistoryEmbeddingsScopePromo,
      /*iph_contents=*/
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_EMBEDDINGS_SCOPE_PROMO_IPH),
      /*matched_term=*/u"@history",
      /*iph_link_text=*/u"",
      /*iph_link_url=*/{},
      /*relevance=*/omnibox::kIPHZeroSuggestRelevance,
      /*deletable=*/true);
}
