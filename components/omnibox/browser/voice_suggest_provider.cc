// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/voice_suggest_provider.h"

#include <string>

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "third_party/omnibox_proto/types.pb.h"

namespace {
// Maximum and minimum score allowed for voice suggestions.
// The score is multiplied by confidence score to produce suggestion relevance.
constexpr const int kMaxVoiceSuggestionScore = 1250;
constexpr const int kMinVoiceSuggestionScore = 350;
constexpr const float kConfidenceAlternativesCutoff = 0.8f;
constexpr const float kConfidenceRelevanceCutoff = 0.3f;
constexpr const int kMaxVoiceMatchesToOffer = 3;

// Calculate relevance score for voice suggestion from confidence score.
constexpr int ConfidenceScoreToSuggestionScore(float confidence_score) {
  return (kMaxVoiceSuggestionScore - kMinVoiceSuggestionScore) *
             confidence_score +
         kMinVoiceSuggestionScore;
}
}  // namespace

VoiceSuggestProvider::VoiceSuggestProvider(AutocompleteProviderClient* client)
    : BaseSearchProvider(TYPE_VOICE_SUGGEST, client) {}

VoiceSuggestProvider::~VoiceSuggestProvider() = default;

void VoiceSuggestProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  MatchMap map;
  int index = 0;
  for (const auto& score_and_suggestion_pair : voice_matches_) {
    // Drop suggestions that do not meet the bar.
    if (score_and_suggestion_pair.first < kConfidenceRelevanceCutoff)
      break;

    AddMatchToMap(
        SearchSuggestionParser::SuggestResult(
            score_and_suggestion_pair.second,
            AutocompleteMatchType::VOICE_SUGGEST,
            /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
            false,
            /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
            ConfidenceScoreToSuggestionScore(score_and_suggestion_pair.first),
            false, {}),
        input, client()->GetTemplateURLService()->GetDefaultSearchProvider(),
        client()->GetTemplateURLService()->search_terms_data(), index, false,
        false, &map);
    ++index;

    // Stop if the first voice suggestion has a high relevance score suggesting
    // it is properly identified, or if we supplied enough voice matches.
    if ((score_and_suggestion_pair.first >= kConfidenceAlternativesCutoff) ||
        (index >= kMaxVoiceMatchesToOffer))
      break;
  }

  for (auto& match_pair : map) {
    matches_.push_back(std::move(match_pair.second));
  }
}

bool VoiceSuggestProvider::ShouldAppendExtraParams(
    const SearchSuggestionParser::SuggestResult& result) const {
  // We always use the default provider for search, so append the params.
  return true;
}

void VoiceSuggestProvider::RecordDeletionResult(bool success) {}

void VoiceSuggestProvider::Stop(bool clear_cached_results,
                                bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  if (clear_cached_results) {
    ClearCache();
  }
}

void VoiceSuggestProvider::ClearCache() {
  voice_matches_.clear();
}

void VoiceSuggestProvider::AddVoiceSuggestion(std::u16string voice_match,
                                              float confidence_score) {
  voice_matches_.emplace_back(confidence_score, std::move(voice_match));
}
