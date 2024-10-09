// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/featured_search_provider.h"

#include <stddef.h>

#include <climits>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
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

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);

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

}  // namespace

// Scored higher than history URL provider suggestions since inputs like '@b'
// would default 'bing.com' instead (history URL provider seems to ignore '@'
// prefix in the input). Featured Enterprise search ranks higher than "ask
// google" suggestions, which ranks higher than the other starter pack
// suggestions.
const int FeaturedSearchProvider::kGeminiRelevance = 1460;
const int FeaturedSearchProvider::kFeaturedEnterpriseSearchRelevance = 1470;
const int FeaturedSearchProvider::kStarterPackRelevance = 1450;

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
      KeywordProvider::GetSubstitutingTemplateURLForInput(template_url_service_,
                                                          &keyword_input);
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

  if (ShouldShowEnterpriseFeaturedSearchIPHMatch(input)) {
    AddFeaturedEnterpriseSearchIPHMatch();
  } else if (ShouldShowGeminiIPHMatch(input)) {
    AddIPHMatch(IphType::kGemini,
                l10n_util::GetStringUTF16(IDS_OMNIBOX_GEMINI_IPH), u"@gemini",
                u"", {}, true);
  } else if (ShouldShowHistoryScopePromoIphMatch(input)) {
    AddHistoryScopePromoIphMatch();
  } else if (ShouldShowHistoryEmbeddingsScopePromoIphMatch(input)) {
    AddHistoryEmbeddingsScopePromoIphMatch();
  }

  if (input.focus_type() != metrics::OmniboxFocusType::INTERACTION_DEFAULT ||
      (input.type() == metrics::OmniboxInputType::EMPTY)) {
    return;
  }

  DoStarterPackAutocompletion(input);
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

FeaturedSearchProvider::~FeaturedSearchProvider() = default;

void FeaturedSearchProvider::DoStarterPackAutocompletion(
    const AutocompleteInput& input) {
  // When the user's input begins with '@', we want to prioritize providing
  // suggestions for all active starter pack search engines.
  bool starts_with_starter_pack_symbol = base::StartsWith(
      input.text(), u"@", base::CompareCase::INSENSITIVE_ASCII);

  if (starts_with_starter_pack_symbol) {
    TemplateURLService::TemplateURLVector matches;
    template_url_service_->AddMatchingKeywords(input.text(), false, &matches);
    for (TemplateURL* match : matches) {
      if (match->starter_pack_id() > 0 &&
          match->is_active() == TemplateURLData::ActiveStatus::kTrue) {
        // Don't add the expanded set of starter pack engines unless the feature
        // is enabled.
        if (!OmniboxFieldTrial::IsStarterPackExpansionEnabled() &&
            match->starter_pack_id() > TemplateURLStarterPackData::kTabs) {
          continue;
        }

        AddStarterPackMatch(*match, input);
      } else if (base::FeatureList::IsEnabled(
                     omnibox::kShowFeaturedEnterpriseSiteSearch) &&
                 match->featured_by_policy()) {
        AddFeaturedEnterpriseSearchMatch(*match, input);
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
  match.inline_autocompletion =
      match.fill_into_edit.substr(input.text().length());
  match.destination_url = GURL(destination_url);
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  if (kIsDesktop &&
      input.current_page_classification() !=
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
          IDS_OMNIBOX_INSTANT_KEYWORD_CHAT_TEXT, template_url.keyword(),
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
                                         bool deletable) {
  // IPH suggestions are grouped after all other suggestions. But they still
  // need to score within top N suggestions to be shown.
  constexpr int kRelevanceScore = 5000;
  AutocompleteMatch match(this, kRelevanceScore, /*deletable=*/deletable,
                          AutocompleteMatchType::NULL_RESULT_MESSAGE);

  // Use this suggestion's contents field to display a message to the user that
  // cannot be acted upon.
  match.contents = iph_contents;
  CHECK_NE(iph_type, IphType::kNone);
  match.iph_type = iph_type;
  match.iph_link_text = iph_link_text;
  match.iph_link_url = iph_link_url;
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

void FeaturedSearchProvider::AddFeaturedEnterpriseSearchMatch(
    const TemplateURL& template_url,
    const AutocompleteInput& input) {
  if (!kIsDesktop || input.current_page_classification() ==
                         metrics::OmniboxEventProto::NTP_REALBOX) {
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

  matches_.push_back(match);
}

bool FeaturedSearchProvider::ShouldShowGeminiIPHMatch(
    const AutocompleteInput& input) const {
  // The IPH suggestion should only be shown in Zero prefix state.
  if (!OmniboxFieldTrial::IsStarterPackIPHEnabled() || !input.IsZeroSuggest() ||
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

bool FeaturedSearchProvider::ShouldShowEnterpriseFeaturedSearchIPHMatch(
    const AutocompleteInput& input) const {
  // Conditions to show the IPH for featured Enterprise search:
  // - The feature is enabled.
  // - This is a Zero prefix state.
  // - There is at least one featured search engine set by policy.
  // - The user has not deleted the IPH suggestion and we have not shown it more
  //   the the accepted limit during this session.
  // - The user has not successfully used at least one featured engine.
  TemplateURLService::TemplateURLVector featured_engines =
      template_url_service_->GetFeaturedEnterpriseSearchEngines();
  return OmniboxFieldTrial::IsFeaturedEnterpriseSearchIPHEnabled() &&
         input.IsZeroSuggest() && !featured_engines.empty() &&
         ShouldShowIPH(IphType::kFeaturedEnterpriseSearch) &&
         base::ranges::all_of(featured_engines, [](auto turl) {
           return turl->usage_count() == 0;
         });
}

bool FeaturedSearchProvider::ShouldShowIPH(IphType iph_type) const {
  PrefService* prefs = client_->GetPrefs();
  // Check the IPH hasn't been dismissed.
  if (!prefs || prefs->GetBoolean(GetIphDismissedPrefNameFor(iph_type)))
    return false;

  // The limit only applies once per session. E.g., when the user types
  // '@history a', the `kHistoryEmbeddingsSettingsPromo` IPH might be shown.
  // When they then type '@history abcdefg', the IPH should continue to be
  // shown, and not disappear at '@history abcd', and only count as 1 shown.
  if (iph_shown_in_omnibox_session_)
    return true;

  // Check the IPH hasn't reached its show limit. Check too many IPHs haven't
  // been shown this session; don't want to show 3 of type 1, then 3 of type 2
  // immediately after.
  size_t iph_shown_count =
      prefs->GetInteger(GetIphShownCountPrefNameFor(iph_type));
  return iph_shown_count < 3 && iph_shown_in_browser_session_count_ < 3;
}

void FeaturedSearchProvider::AddFeaturedEnterpriseSearchIPHMatch() {
  std::vector<std::string> sites;
  base::ranges::transform(
      template_url_service_->GetFeaturedEnterpriseSearchEngines(),
      std::back_inserter(sites), [](auto turl) {
        return url_formatter::StripWWW(GURL(turl->url()).host());
      });
  base::ranges::sort(sites);
  AddIPHMatch(IphType::kFeaturedEnterpriseSearch,
              l10n_util::GetStringFUTF16(
                  IDS_OMNIBOX_FEATURED_ENTERPRISE_SITE_SEARCH_IPH,
                  base::UTF8ToUTF16(base::JoinString(sites, ", "))),
              u"", u"", {}, true);
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
         history_embeddings::kOmniboxScoped.Get() &&
         ShouldShowIPH(IphType::kHistoryEmbeddingsSettingsPromo);
}

void FeaturedSearchProvider::AddHistoryEmbeddingsSettingsPromoIphMatch() {
  std::u16string text = l10n_util::GetStringUTF16(
                            IDS_OMNIBOX_HISTORY_EMBEDDINGS_SETTINGS_PROMO_IPH) +
                        u" ";
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_OMNIBOX_HISTORY_EMBEDDINGS_SETTINGS_PROMO_IPH_LINK_TEXT);
  GURL link_url = GURL("chrome://settings/historySearch");
  AddIPHMatch(IphType::kHistoryEmbeddingsSettingsPromo, text, u"", link_text,
              link_url, true);
}

bool FeaturedSearchProvider::ShouldShowHistoryEmbeddingsDisclaimerIphMatch()
    const {
  // Assumes this is only called when the user is in @history scope. Not limited
  // by `ShouldShowIPH()` (i.e. shown count or dismissal) because this is a
  // disclaimer.
  return client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::kOmniboxScoped.Get();
}

void FeaturedSearchProvider::AddHistoryEmbeddingsDisclaimerIphMatch() {
  std::u16string text =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_EMBEDDINGS_DISCLAIMER_IPH) +
      u" ";
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_OMNIBOX_HISTORY_EMBEDDINGS_DISCLAIMER_IPH_LINK_TEXT);
  GURL link_url = GURL("chrome://settings/historySearch");
  AddIPHMatch(IphType::kHistoryEmbeddingsDisclaimer, text, u"", link_text,
              link_url, false);
}

bool FeaturedSearchProvider::ShouldShowHistoryScopePromoIphMatch(
    const AutocompleteInput& input) const {
  // Shown in the zero state when history embeddings is disabled (not opted-in),
  // but the embeddings is enabled for the omnibox. Doesn't check if the setting
  // is visible. We want to guard this behind some meaningful param but it's not
  // directly related to embeddings so it's ok to show to users who can't opt-in
  // to embeddings.
  return input.IsZeroSuggest() && !client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::kOmniboxScoped.Get() &&
         !client_->IsOffTheRecord() &&
         ShouldShowIPH(IphType::kHistoryScopePromo);
}

void FeaturedSearchProvider::AddHistoryScopePromoIphMatch() {
  std::u16string text =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_SCOPE_PROMO_IPH);
  AddIPHMatch(IphType::kHistoryScopePromo, text, u"@history", u"", {}, true);
}

bool FeaturedSearchProvider::ShouldShowHistoryEmbeddingsScopePromoIphMatch(
    const AutocompleteInput& input) const {
  // Shown in the zero state when history embeddings is enabled (& opted-in) for
  // the omnibox.
  return input.IsZeroSuggest() && client_->IsHistoryEmbeddingsEnabled() &&
         history_embeddings::kOmniboxScoped.Get() &&
         ShouldShowIPH(IphType::kHistoryEmbeddingsScopePromo);
}

void FeaturedSearchProvider::AddHistoryEmbeddingsScopePromoIphMatch() {
  std::u16string text =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_EMBEDDINGS_SCOPE_PROMO_IPH);
  AddIPHMatch(IphType::kHistoryEmbeddingsScopePromo, text, u"@history", u"", {},
              true);
}
