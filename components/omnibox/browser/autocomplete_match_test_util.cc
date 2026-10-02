// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_test_util.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

AutocompleteMatch CreateHistoryURLMatch(std::string destination_url,
                                        bool is_zero_prefix) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kHistoryUrl;
  match.destination_url = GURL(destination_url);
  if (is_zero_prefix) {
    match.subtypes.insert(omnibox::SUBTYPE_ZERO_PREFIX);
  }
  return match;
}

AutocompleteMatch CreateCompanyEntityMatch(std::string website_uri) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggestEntity;
  omnibox::SuggestTemplateInfo suggest_template;
  suggest_template.set_website_uri(website_uri);
  match.suggest_template = std::move(suggest_template);
  match.image_url = GURL("https://url");
  match.image_dominant_color = "#000000";
  return match;
}

AutocompleteMatch CreateSearchMatch(std::u16string contents) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggest;
  match.contents = contents;
  return match;
}

AutocompleteMatch CreateContextualSearchMatch(std::u16string contents) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggest;
  match.contents = contents;
  match.relevance = 195;
  match.contents_class = {{0, 1}};
  match.keyword = u"contextual";
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
  return match;
}

AutocompleteMatch CreateZeroSuggestContextualSearchMatch(
    std::u16string contents) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggest;
  match.contents = contents;
  match.relevance = 195;
  match.contents_class = {{0, 1}};
  match.keyword = u"contextual";
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
  match.subtypes.insert(omnibox::SUBTYPE_ZERO_PREFIX);
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(std::u16string());
  match.search_terms_args->searchbox_stats.set_client_name("chrome");
  match.destination_url =
      GURL{"https://google.com/" + base::UTF16ToUTF8(contents)};
  return match;
}

AutocompleteMatch CreateLensActionMatch(std::u16string contents) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kPedal;
  match.contents = contents;
  match.contents_class = {{0, 1}};
  match.keyword = u"lens";
  match.relevance = omnibox::kContextualActionZeroSuggestRelevance;
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH_ACTION;
  match.takeover_action =
      base::MakeRefCounted<ContextualSearchOpenLensAction>();
  return match;
}

AutocompleteMatch CreateZeroPrefixSearchMatch(std::u16string contents) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggest;
  match.contents = contents;
  match.subtypes.insert(omnibox::SUBTYPE_ZERO_PREFIX);
  return match;
}

AutocompleteMatch CreateStarterPackMatch(std::u16string keyword) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kStarterPack;
  match.contents = keyword;
  match.keyword = keyword;
  match.associated_keyword = keyword;
  return match;
}

AutocompleteMatch CreateFeaturedEnterpriseSearch(std::u16string keyword) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kFeaturedEnterpriseSearch;
  match.contents = keyword;
  match.keyword = keyword;
  match.associated_keyword = keyword;
  return match;
}

AutocompleteMatch CreateActionInSuggestMatch(
    std::u16string description,
    std::vector<omnibox::SuggestTemplateInfo::TemplateAction::ActionType>
        action_types) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggestEntity;
  match.description = description;
  for (auto action_type : action_types) {
    omnibox::SuggestTemplateInfo::TemplateAction template_action;
    template_action.set_action_type(action_type);
    match.actions.push_back(base::MakeRefCounted<OmniboxActionInSuggest>(
        std::move(template_action), std::nullopt));
  }
  return match;
}

AutocompleteMatch CreateSearchMatch(std::string name,
                                    bool allowed_to_be_default_match,
                                    int traditional_relevance) {
  auto match = CreateAutocompleteMatch(
      name, omnibox::AutocompleteMatchType::kSearchSuggest,
      allowed_to_be_default_match, false, traditional_relevance, std::nullopt);
  match.keyword = u"keyword";
  match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
      base::UTF8ToUTF16(name));
  return match;
}

AutocompleteMatch CreatePersonalizedZeroPrefixMatch(std::string name,
                                                    int traditional_relevance) {
  auto match = CreateAutocompleteMatch(
      name, omnibox::AutocompleteMatchType::kSearchSuggestPersonalized, false,
      false, traditional_relevance, std::nullopt);
  match.keyword = u"keyword";
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(std::u16string());
  match.suggestion_group_id = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  match.subtypes.emplace(omnibox::SUBTYPE_PERSONAL);
  match.subtypes.emplace(omnibox::SUBTYPE_ZERO_PREFIX);
  return match;
}

AutocompleteMatch CreateHistoryUrlMlScoredMatch(
    std::string name,
    bool allowed_to_be_default_match,
    int traditional_relevance,
    float ml_output) {
  return CreateMlScoredMatch(name, omnibox::AutocompleteMatchType::kHistoryUrl,
                             allowed_to_be_default_match, traditional_relevance,
                             ml_output);
}

AutocompleteMatch CreateAnswerMlScoredMatch(std::string name,
                                            bool allowed_to_be_default_match,
                                            int traditional_relevance,
                                            float ml_output) {
  AutocompleteMatch match = CreateSearchMlScoredMatch(
      name, allowed_to_be_default_match, traditional_relevance, ml_output);
  match.answer_template = omnibox::RichAnswerTemplate();
  return match;
}

AutocompleteMatch CreateSearchMlScoredMatch(std::string name,
                                            bool allowed_to_be_default_match,
                                            int traditional_relevance,
                                            float ml_output) {
  AutocompleteMatch match = CreateMlScoredMatch(
      name, omnibox::AutocompleteMatchType::kSearchSuggest,
      allowed_to_be_default_match, traditional_relevance, ml_output);
  match.keyword = u"keyword";
  match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
      base::UTF8ToUTF16(name));
  return match;
}

AutocompleteMatch CreateMlScoredMatch(std::string name,
                                      omnibox::AutocompleteMatchType type,
                                      bool allowed_to_be_default_match,
                                      int traditional_relevance,
                                      float ml_output) {
  return CreateAutocompleteMatch(name, type, allowed_to_be_default_match, false,
                                 traditional_relevance, ml_output);
}

AutocompleteMatch CreateBoostedShortcutMatch(std::string name,
                                             int traditional_relevance,
                                             float ml_output) {
  return CreateAutocompleteMatch(name,
                                 omnibox::AutocompleteMatchType::kHistoryUrl,
                                 true, true, traditional_relevance, ml_output);
}

AutocompleteMatch CreateHistoryClusterMatch(std::string name,
                                            int traditional_relevance) {
  return CreateAutocompleteMatch(
      name, omnibox::AutocompleteMatchType::kHistoryCluster, false, false,
      traditional_relevance, std::nullopt);
}

AutocompleteMatch CreateAutocompleteMatch(std::string name,
                                          omnibox::AutocompleteMatchType type,
                                          bool allowed_to_be_default_match,
                                          bool shortcut_boosted,
                                          int traditional_relevance,
                                          std::optional<float> ml_output) {
  AutocompleteMatch match{nullptr, traditional_relevance, false, type};
  match.shortcut_boosted = shortcut_boosted;
  match.allowed_to_be_default_match = allowed_to_be_default_match;
  match.destination_url = GURL{"https://google.com/" + name};
  match.stripped_destination_url = GURL{"https://google.com/" + name};
  match.contents = base::UTF8ToUTF16(name);
  match.contents_class = {{0, 1}};
  if (ml_output.has_value()) {
    match.scoring_signals = {{}};
    match.scoring_signals->set_site_engagement(ml_output.value());
  }
  return match;
}
