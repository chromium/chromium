// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/fuzzy_tokenized_string_match.h"
#include <pthread.h>

#include <algorithm>
#include <iterator>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

namespace app_list {

namespace {
constexpr bool kDefaultUseWeightedRatio = true;
constexpr double kDefaultRelevanceThreshold = 0.3;
constexpr double kMinScore = 0.0;
constexpr double kMaxScore = 1.0;
constexpr double kFirstCharacterMatchPenalty = 0.2;
constexpr double kPrefixMatchPenalty = 0.1;

// Returns sorted tokens from a TokenizedString.
std::vector<base::string16> ProcessAndSort(const ash::TokenizedString& text) {
  std::vector<base::string16> result;
  for (const auto& token : text.tokens()) {
    result.emplace_back(token);
  }
  std::sort(result.begin(), result.end());
  return result;
}
}  // namespace

namespace internal {
// Check if the query only contains first characters of the text,
// e.g. "coc" is a match of "Clash of Clan". Range of the score is [0, 1].
double FirstCharacterMatch(const ash::TokenizedString& query,
                           const ash::TokenizedString& text) {
  const base::string16 query_lower = base::i18n::ToLower(query.text());
  size_t query_index = 0;
  for (size_t text_index = 0; text_index < text.tokens().size(); text_index++) {
    if (query_index < query_lower.size() &&
        text.tokens()[text_index][0] == query_lower[query_index]) {
      query_index++;
      if (query_index == query_lower.size()) {
        // Penalizes the score using the number of text's tokens that are
        // needed.
        return std::max(kMinScore,
                        kMaxScore - kFirstCharacterMatchPenalty *
                                        (text_index + 1 - query_lower.size()));
      }
    }
  }
  return kMinScore;
}

// Check if tokens of query are prefixes of text's tokens. Range of score is
// [0, 1].
double PrefixMatch(const ash::TokenizedString& query,
                   const ash::TokenizedString& text) {
  const std::vector<base::string16> query_tokens(query.tokens());
  const std::vector<base::string16> text_tokens(text.tokens());
  double match_score = kMaxScore;
  int previous_matched_index = -1;
  // For every query token, check if it is a prefix of a text token. The newly
  // matching text token must have higher index than the previous matched token.
  for (const auto& query_token : query_tokens) {
    bool matched = false;
    for (size_t text_index = previous_matched_index + 1;
         text_index < text_tokens.size(); text_index++) {
      if (query_token.size() <= text_tokens[text_index].size() &&
          query_token ==
              text_tokens[text_index].substr(0, query_token.size())) {
        matched = true;
        // Penalizes the score based on the number of skipped tokens.
        match_score -=
            kPrefixMatchPenalty * (text_index - previous_matched_index - 1);
        previous_matched_index = text_index;
        break;
      }
    }
    if (!matched) {
      return kMinScore;
    }
  }
  return std::max(kMinScore, match_score);
}
}  // namespace internal

FuzzyTokenizedStringMatch::~FuzzyTokenizedStringMatch() {}
FuzzyTokenizedStringMatch::FuzzyTokenizedStringMatch() {}

double FuzzyTokenizedStringMatch::TokenSetRatio(
    const ash::TokenizedString& query,
    const ash::TokenizedString& text,
    bool partial) {
  std::set<base::string16> query_token(query.tokens().begin(),
                                       query.tokens().end());
  std::set<base::string16> text_token(text.tokens().begin(),
                                      text.tokens().end());

  std::vector<base::string16> intersection;
  std::vector<base::string16> query_diff_text;
  std::vector<base::string16> text_diff_query;

  // Find the intersection and the differences between two set of tokens.
  std::set_intersection(query_token.begin(), query_token.end(),
                        text_token.begin(), text_token.end(),
                        std::back_inserter(intersection));
  std::set_difference(query_token.begin(), query_token.end(),
                      text_token.begin(), text_token.end(),
                      std::back_inserter(query_diff_text));
  std::set_difference(text_token.begin(), text_token.end(), query_token.begin(),
                      query_token.end(), std::back_inserter(text_diff_query));

  const base::string16 intersection_string =
      base::JoinString(intersection, base::UTF8ToUTF16(" "));
  const base::string16 query_rewritten =
      intersection.empty()
          ? base::JoinString(query_diff_text, base::UTF8ToUTF16(" "))
          : base::StrCat(
                {intersection_string, base::UTF8ToUTF16(" "),
                 base::JoinString(query_diff_text, base::UTF8ToUTF16(" "))});
  const base::string16 text_rewritten =
      intersection.empty()
          ? base::JoinString(text_diff_query, base::UTF8ToUTF16(" "))
          : base::StrCat(
                {intersection_string, base::UTF8ToUTF16(" "),
                 base::JoinString(text_diff_query, base::UTF8ToUTF16(" "))});

  if (partial) {
    return std::max({PartialRatio(intersection_string, query_rewritten),
                     PartialRatio(intersection_string, text_rewritten),
                     PartialRatio(query_rewritten, text_rewritten)});
  }

  return std::max(
      {SequenceMatcher(intersection_string, query_rewritten).Ratio(),
       SequenceMatcher(intersection_string, text_rewritten).Ratio(),
       SequenceMatcher(query_rewritten, text_rewritten).Ratio()});
}

double FuzzyTokenizedStringMatch::TokenSortRatio(
    const ash::TokenizedString& query,
    const ash::TokenizedString& text,
    bool partial) {
  const base::string16 query_sorted =
      base::JoinString(ProcessAndSort(query), base::UTF8ToUTF16(" "));
  const base::string16 text_sorted =
      base::JoinString(ProcessAndSort(text), base::UTF8ToUTF16(" "));

  if (partial) {
    return PartialRatio(query_sorted, text_sorted);
  }
  return SequenceMatcher(query_sorted, text_sorted).Ratio();
}

double FuzzyTokenizedStringMatch::PartialRatio(const base::string16& query,
                                               const base::string16& text) {
  if (query.empty() || text.empty()) {
    return kMinScore;
  }
  base::string16 shorter = query;
  base::string16 longer = text;

  if (shorter.size() > longer.size()) {
    shorter = text;
    longer = query;
  }

  const auto matching_blocks =
      SequenceMatcher(shorter, longer).GetMatchingBlocks();
  double partial_ratio = 0;

  for (const auto& block : matching_blocks) {
    const int long_start =
        block.pos_second_string > block.pos_first_string
            ? block.pos_second_string - block.pos_first_string
            : 0;

    // TODO(crbug/990684): currently this part re-calculate the ratio for every
    // pair. Improve this to reduce latency.
    partial_ratio = std::max(
        partial_ratio,
        SequenceMatcher(shorter, longer.substr(long_start, shorter.size()))
            .Ratio());
    if (partial_ratio > 0.995) {
      return kMaxScore;
    }
  }
  return partial_ratio;
}

double FuzzyTokenizedStringMatch::WeightedRatio(
    const ash::TokenizedString& query,
    const ash::TokenizedString& text) {
  const double unbase_scale = 0.95;
  // Since query.text() and text.text() is not normalized, we use query.tokens()
  // and text.tokens() instead.
  const base::string16 query_normalized(
      base::JoinString(query.tokens(), base::UTF8ToUTF16(" ")));
  const base::string16 text_normalized(
      base::JoinString(text.tokens(), base::UTF8ToUTF16(" ")));
  double weighted_ratio =
      SequenceMatcher(query_normalized, text_normalized).Ratio();
  const double length_ratio =
      static_cast<double>(
          std::max(query_normalized.size(), text_normalized.size())) /
      std::min(query_normalized.size(), text_normalized.size());

  // Use partial if two strings are quite different in sizes.
  const bool use_partial = length_ratio >= 1.5;
  double partial_scale = 1;

  if (use_partial) {
    // If one string is much much shorter than the other, set |partial_scale| to
    // be 0.6, otherwise set it to be 0.9.
    partial_scale = length_ratio > 8 ? 0.6 : 0.9;
    weighted_ratio = std::max(
        weighted_ratio,
        PartialRatio(query_normalized, text_normalized) * partial_scale);
  }
  weighted_ratio = std::max(
      weighted_ratio, TokenSortRatio(query, text, use_partial /*partial*/) *
                          unbase_scale * partial_scale);
  weighted_ratio = std::max(
      weighted_ratio, TokenSetRatio(query, text, use_partial /*partial*/) *
                          unbase_scale * partial_scale);
  return weighted_ratio;
}

double FuzzyTokenizedStringMatch::PrefixMatcher(
    const ash::TokenizedString& query,
    const ash::TokenizedString& text) {
  return std::max(internal::PrefixMatch(query, text),
                  internal::FirstCharacterMatch(query, text));
}

bool FuzzyTokenizedStringMatch::IsRelevant(const ash::TokenizedString& query,
                                           const ash::TokenizedString& text) {
  // Find |hits_| using SequenceMatcher on original query and text.
  for (const auto& match :
       SequenceMatcher(query.text(), text.text()).GetMatchingBlocks()) {
    if (match.length > 0) {
      hits_.push_back(gfx::Range(match.pos_second_string,
                                 match.pos_second_string + match.length));
    }
  }

  // If the query is much longer than the text then it's often not a match.
  if (query.text().size() >= text.text().size() * 2) {
    return false;
  }

  const double relevance_threshold = base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kEnableFuzzyAppSearch, "relevance_threshold",
      kDefaultRelevanceThreshold);
  const double prefix_score = PrefixMatcher(query, text);

  if (base::GetFieldTrialParamByFeatureAsBool(
          app_list_features::kEnableFuzzyAppSearch, "use_prefix_only", false) &&
      prefix_score >= relevance_threshold) {
    // If the prefix score is already higher than |relevance_threshold|, use
    // prefix score as final score.
    relevance_ = prefix_score;
    return true;
  }

  const bool use_weighted_ratio = base::GetFieldTrialParamByFeatureAsBool(
      app_list_features::kEnableFuzzyAppSearch, "use_weighted_ratio",
      kDefaultUseWeightedRatio);

  if (use_weighted_ratio) {
    // If WeightedRatio is used, |relevance_| is the average of WeightedRatio
    // and PrefixMatcher scores.
    relevance_ = (WeightedRatio(query, text) + prefix_score) / 2;
  } else {
    // Use simple algorithm to calculate match ratio.
    double partial_match = 0.0;
    for (const auto& query_token : query.tokens()) {
      for (const auto& text_token : text.tokens()) {
        partial_match = std::max(
            partial_match, SequenceMatcher(query_token, text_token).Ratio());
      }
    }
    const double partial_scale = 0.9;
    relevance_ = (std::max(SequenceMatcher(query.text(), text.text()).Ratio(),
                           partial_match * partial_scale) +
                  prefix_score) /
                 2;
  }

  return relevance_ >= relevance_threshold;
}

}  // namespace app_list
