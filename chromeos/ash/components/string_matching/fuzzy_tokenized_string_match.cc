// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/string_matching/acronym_matcher.h"
#include "chromeos/ash/components/string_matching/diacritic_utils.h"
#include "chromeos/ash/components/string_matching/prefix_matcher.h"
#include "chromeos/ash/components/string_matching/sequence_matcher.h"

namespace ash::string_matching {

namespace {

using Hits = FuzzyTokenizedStringMatch::Hits;

constexpr double kPartialMatchPenaltyRate = 0.9;

constexpr double kMinScore = 0.0;
constexpr double kMaxScore = 1.0;

// The maximum supported size for a prefix matching scoring boost.
constexpr size_t kMaxBoostSize = 2;

// The scale ratio for non exact matching results.
constexpr double kNonExactMatchScaleRatio = 0.97;

// Returns sorted tokens from a TokenizedString.
std::vector<std::u16string> ProcessAndSort(const TokenizedString& text) {
  std::vector<std::u16string> result;
  for (const auto& token : text.tokens()) {
    result.emplace_back(token);
  }
  std::sort(result.begin(), result.end());
  return result;
}

double ScaledRelevance(const double relevance) {
  return 1.0 - std::pow(0.5, relevance);
}

}  // namespace

FuzzyTokenizedStringMatch::~FuzzyTokenizedStringMatch() = default;
FuzzyTokenizedStringMatch::FuzzyTokenizedStringMatch() = default;

double FuzzyTokenizedStringMatch::TokenSetRatio(const TokenizedString& query,
                                                const TokenizedString& text,
                                                bool partial) {
  std::set<std::u16string> query_token(query.tokens().begin(),
                                       query.tokens().end());
  std::set<std::u16string> text_token(text.tokens().begin(),
                                      text.tokens().end());

  std::vector<std::u16string> intersection;
  std::vector<std::u16string> query_diff_text;
  std::vector<std::u16string> text_diff_query;

  // Find the set intersection and the set differences between two sets of
  // tokens.
  std::set_intersection(query_token.begin(), query_token.end(),
                        text_token.begin(), text_token.end(),
                        std::back_inserter(intersection));
  std::set_difference(query_token.begin(), query_token.end(),
                      text_token.begin(), text_token.end(),
                      std::back_inserter(query_diff_text));
  std::set_difference(text_token.begin(), text_token.end(), query_token.begin(),
                      query_token.end(), std::back_inserter(text_diff_query));

  const std::u16string intersection_string =
      base::JoinString(intersection, u" ");
  const std::u16string query_rewritten =
      intersection.empty()
          ? base::JoinString(query_diff_text, u" ")
          : base::StrCat({intersection_string, u" ",
                          base::JoinString(query_diff_text, u" ")});
  const std::u16string text_rewritten =
      intersection.empty()
          ? base::JoinString(text_diff_query, u" ")
          : base::StrCat({intersection_string, u" ",
                          base::JoinString(text_diff_query, u" ")});

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

double FuzzyTokenizedStringMatch::TokenSortRatio(const TokenizedString& query,
                                                 const TokenizedString& text,
                                                 bool partial) {
  const std::u16string query_sorted =
      base::JoinString(ProcessAndSort(query), u" ");
  const std::u16string text_sorted =
      base::JoinString(ProcessAndSort(text), u" ");

  if (partial) {
    return PartialRatio(query_sorted, text_sorted);
  }
  return SequenceMatcher(query_sorted, text_sorted).Ratio();
}

double FuzzyTokenizedStringMatch::PartialRatio(const std::u16string& query,
                                               const std::u16string& text) {
  if (query.empty() || text.empty()) {
    return kMinScore;
  }
  std::u16string shorter = query;
  std::u16string longer = text;

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

    // Penalizes the match if it is not close to the beginning of a token.
    int current = long_start - 1;
    while (current >= 0 &&
           !base::EqualsCaseInsensitiveASCII(longer.substr(current, 1), u" ")) {
      current--;
    }
    const double penalty =
        std::pow(kPartialMatchPenaltyRate, long_start - current - 1);
    // TODO(crbug.com/40638914): currently this part re-calculate the ratio for
    // every pair. Improve this to reduce latency.
    partial_ratio = std::max(
        partial_ratio,
        SequenceMatcher(shorter, longer.substr(long_start, shorter.size()))
                .Ratio() *
            penalty);

    if (partial_ratio > 0.995) {
      return kMaxScore;
    }
  }
  return partial_ratio;
}

double FuzzyTokenizedStringMatch::WeightedRatio(const TokenizedString& query,
                                                const TokenizedString& text) {
  // All token based comparisons are scaled by 0.95 (on top of any partial
  // scalars), as per original implementation:
  // https://github.com/seatgeek/fuzzywuzzy/blob/af443f918eebbccff840b86fa606ac150563f466/fuzzywuzzy/fuzz.py#L245
  const double unbase_scale = 0.95;

  // Since query.text() and text.text() is not normalized, we use query.tokens()
  // and text.tokens() instead.
  const std::u16string query_normalized(base::JoinString(query.tokens(), u" "));
  const std::u16string text_normalized(base::JoinString(text.tokens(), u" "));

  std::vector<double> weighted_ratios;
  weighted_ratios.emplace_back(
      SequenceMatcher(query_normalized, text_normalized)
          .Ratio(/*text_length_agnostic=*/true));

  const double length_ratio =
      static_cast<double>(
          std::max(query_normalized.size(), text_normalized.size())) /
      std::min(query_normalized.size(), text_normalized.size());

  // Use partial if two strings are quite different in sizes.
  const bool use_partial = length_ratio >= 1.5;
  double length_ratio_scale = 1;

  if (use_partial) {
    // TODO(crbug.com/1336160): Consider scaling |partial_scale| smoothly with
    // |length_ratio|, instead of using a step function.
    //
    // If one string is much much shorter than the other, set |partial_scale| to
    // be 0.6, otherwise set it to be 0.9.
    length_ratio_scale = length_ratio > 8 ? 0.6 : 0.9;
    weighted_ratios.emplace_back(
        PartialRatio(query_normalized, text_normalized) * length_ratio_scale);
  }
  weighted_ratios.emplace_back(TokenSortRatio(query, text, use_partial) *
                               unbase_scale * length_ratio_scale);

  // Do not use partial match for token set because the match between the
  // intersection string and query/text rewrites will always return an extremely
  // high value.
  weighted_ratios.emplace_back(TokenSetRatio(query, text, false /*partial*/) *
                               unbase_scale * length_ratio_scale);

  // Return the maximum of all included weighted ratios
  return *std::max_element(weighted_ratios.begin(), weighted_ratios.end());
}

double FuzzyTokenizedStringMatch::PrefixMatcher(const TokenizedString& query,
                                                const TokenizedString& text) {
  string_matching::PrefixMatcher match(query, text);
  match.Match();
  return ScaledRelevance(match.relevance());
}

double FuzzyTokenizedStringMatch::AcronymMatcher(const TokenizedString& query,
                                                 const TokenizedString& text) {
  string_matching::AcronymMatcher match(query, text);
  const double relevance = match.CalculateRelevance();
  return ScaledRelevance(relevance);
}

double FuzzyTokenizedStringMatch::PrefixMatcher(
    const TokenizedString& query,
    const TokenizedString& text,
    std::vector<Hits>& hits_vector) {
  string_matching::PrefixMatcher match(query, text);
  match.Match();

  hits_vector.emplace_back(match.hits());
  return ScaledRelevance(match.relevance());
}

double FuzzyTokenizedStringMatch::AcronymMatcher(
    const TokenizedString& query,
    const TokenizedString& text,
    std::vector<Hits>& hits_vector) {
  string_matching::AcronymMatcher match(query, text);
  const double relevance = match.CalculateRelevance();

  hits_vector.emplace_back(match.hits());
  return ScaledRelevance(relevance);
}

double FuzzyTokenizedStringMatch::Relevance(const TokenizedString& query_input,
                                            const TokenizedString& text_input,
                                            bool use_weighted_ratio,
                                            bool strip_diacritics,
                                            bool use_acronym_matcher) {
  // If the query is much longer than the text then it's often not a match.
  if (query_input.text().size() >= text_input.text().size() * 2) {
    return 0.0;
  }

  std::optional<TokenizedString> stripped_query;
  std::optional<TokenizedString> stripped_text;
  if (strip_diacritics) {
    stripped_query.emplace(RemoveDiacritics(query_input.text()));
    stripped_text.emplace(RemoveDiacritics(text_input.text()));
  }

  const TokenizedString& query =
      strip_diacritics ? stripped_query.value() : query_input;
  const TokenizedString& text =
      strip_diacritics ? stripped_text.value() : text_input;

  // If there is an exact match, relevance will be 1.0 and there is only 1
  // hit that is the entire text/query.
  const auto& query_text = query.text();
  const auto& text_text = text.text();
  const auto query_size = query_text.size();
  const auto text_size = text_text.size();
  if (query_size > 0 && query_size == text_size &&
      base::EqualsCaseInsensitiveASCII(query_text, text_text)) {
    hits_.emplace_back(0, query_size);
    return 1.0;
  }

  // The |relevances| stores the |relevance_scores| calculated from different
  // string matching methods. The highest result among them will be returned.
  std::vector<double> relevances;
  // The |hits_vector| stores the |hits| calculated from different string
  // matching methods. The final selected instance corresponds to the hits
  // generated by the matching algorithm which yielded the highest relevance
  // score. The final selected instance will be assigned to |hits_| then.
  std::vector<Hits> hits_vector;

  double prefix_score = PrefixMatcher(query, text, hits_vector);
  // A scoring boost for short prefix matching queries.
  if (query_size <= kMaxBoostSize && prefix_score > kMinScore) {
    prefix_score = std::min(
        1.0, prefix_score + 2.0 / (query_size * (query_size + text_size)));
  }
  relevances.emplace_back(prefix_score);

  // Find hits using SequenceMatcher on original query and text.
  Hits sequence_hits;
  size_t match_size = 0;
  for (const auto& match :
       SequenceMatcher(query_text, text_text).GetMatchingBlocks()) {
    if (match.length > 0) {
      match_size += match.length;
      sequence_hits.emplace_back(match.pos_second_string,
                                 match.pos_second_string + match.length);
    }
  }
  hits_vector.emplace_back(sequence_hits);

  relevances.emplace_back(use_weighted_ratio
                              ? WeightedRatio(query, text)
                              : SequenceMatcher(base::i18n::ToLower(query_text),
                                                base::i18n::ToLower(text_text))
                                    .Ratio(/*text_length_agnostic=*/true));
  if (use_acronym_matcher) {
    relevances.emplace_back(AcronymMatcher(query, text, hits_vector));
  }

  size_t best_match_pos =
      std::max_element(relevances.begin(), relevances.end()) -
      relevances.begin();
  hits_ = hits_vector[best_match_pos];
  return match_size == text_size
             ? relevances[best_match_pos]
             : relevances[best_match_pos] * kNonExactMatchScaleRatio;
}

}  // namespace ash::string_matching
