// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TEST_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TEST_UTIL_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/entity_info.pb.h"

AutocompleteMatch CreateAutocompleteMatch(std::string name,
                                          AutocompleteMatchType::Type type,
                                          bool allowed_to_be_default_match,
                                          bool shortcut_boosted,
                                          int traditional_relevance,
                                          std::optional<float> ml_output);

AutocompleteMatch CreateHistoryURLMatch(std::string destination_url,
                                        bool is_zero_prefix = false);

AutocompleteMatch CreateCompanyEntityMatch(std::string website_uri);

AutocompleteMatch CreateSearchMatch(std::u16string contents = u"text");

AutocompleteMatch CreateContextualSearchMatch(
    std::u16string contents = u"text");

AutocompleteMatch CreateLensActionMatch(
  std::u16string contents = u"text");

AutocompleteMatch CreateZeroPrefixSearchMatch(
    std::u16string contents = u"text");

AutocompleteMatch CreateStarterPackMatch(std::u16string keyword);

AutocompleteMatch CreateFeaturedEnterpriseSearch(std::u16string keyword);

AutocompleteMatch CreateActionInSuggestMatch(
    std::u16string description,
    std::vector<omnibox::ActionInfo::ActionType> action_types);

AutocompleteMatch CreateSearchMatch(std::string name,
                                    bool allowed_to_be_default_match,
                                    int traditional_relevance);

AutocompleteMatch CreatePersonalizedZeroPrefixMatch(std::string name,
                                                    int traditional_relevance);

AutocompleteMatch CreateHistoryUrlMlScoredMatch(
    std::string name,
    bool allowed_to_be_default_match,
    int traditional_relevance,
    float ml_output);

AutocompleteMatch CreateAnswerMlScoredMatch(std::string name,
                                            omnibox::AnswerType answer_type,
                                            std::string answer_json,
                                            bool allowed_to_be_default_match,
                                            int traditional_relevance,
                                            float ml_output);

AutocompleteMatch CreateSearchMlScoredMatch(std::string name,
                                            bool allowed_to_be_default_match,
                                            int traditional_relevance,
                                            float ml_output);

AutocompleteMatch CreateMlScoredMatch(std::string name,
                                      AutocompleteMatchType::Type type,
                                      bool allowed_to_be_default_match,
                                      int traditional_relevance,
                                      float ml_output);

AutocompleteMatch CreateBoostedShortcutMatch(std::string name,
                                             int traditional_relevance,
                                             float ml_output);
AutocompleteMatch CreateKeywordHintMatch(std::string name,
                                         int traditional_relevance);

AutocompleteMatch CreateHistoryClusterMatch(std::string name,
                                            int traditional_relevance);

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_TEST_UTIL_H_
