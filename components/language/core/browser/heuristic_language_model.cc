// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/heuristic_language_model.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "components/prefs/pref_service.h"

namespace language {

namespace {

// Input source cutoffs.
constexpr double kUlpConfidenceCutoff = 0.7;
constexpr double kUlpProbabilityCutoff = 0.55;
constexpr float kHistogramFrequencyCutoff = 0.3f;

// Input source weights.
constexpr float kUiWeight = 2.0f;
constexpr float kAcceptWeight = 1.1f;
constexpr float kHistogramWeight = 1.0f;
constexpr float kUlpWeight = 0.5f;
constexpr float kScoreDelta = 0.01f;

// For reading ULP prefs.
constexpr char kUlpConfidenceKey[] = "confidence";
constexpr char kUlpLanguageKey[] = "language";
constexpr char kUlpPreferenceKey[] = "preference";
constexpr char kUlpProbabilityKey[] = "probability";
constexpr char kUlpReadingKey[] = "reading";

}  // namespace

bool HasBaseAndRegion(const std::string& lang, std::string* const base) {
  const std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      lang, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  *base = tokens.size() > 0 ? std::string(tokens[0]) : "";
  return tokens.size() > 1 && !tokens[1].empty();
}

std::vector<std::string> PromoteRegions(
    const std::vector<std::string>& in_langs) {
  // Construct a map from base language to the list of region-specific languages
  // for that base.
  std::unordered_map<std::string, std::vector<std::string>> regions_by_base;
  std::string base;
  for (const std::string& lang : in_langs) {
    if (HasBaseAndRegion(lang, &base))
      regions_by_base[base].push_back(lang);
  }

  // For each input language, output its region-specific variants (if we found
  // any), or otherwise just the language itself.
  std::vector<std::string> out_langs;
  std::unordered_set<std::string> seen;
  for (const std::string& lang : in_langs) {
    const auto lookup = regions_by_base.find(lang);
    const std::vector<std::string>& regions =
        lookup == regions_by_base.end() ? std::vector<std::string>(1, lang)
                                        : lookup->second;

    for (const std::string& out_lang : regions) {
      if (seen.find(out_lang) == seen.end()) {
        out_langs.push_back(out_lang);
        seen.insert(out_lang);
      }
    }
  }
  return out_langs;
}

std::unordered_map<std::string, float> PromoteRegionScores(
    const std::unordered_map<std::string, float>& in_scores) {
  std::unordered_map<std::string, float> out_scores;
  std::unordered_set<std::string> seen;

  // First pass: add promoted scores for region-specific languges and note base
  //             languages for which we've observed a region-specific variant.
  std::string base;
  for (const auto& kv : in_scores) {
    const std::string& lang = kv.first;
    const float score = kv.second;

    if (HasBaseAndRegion(lang, &base)) {
      seen.insert(lang);
      seen.insert(base);

      const auto lookup = in_scores.find(base);
      out_scores[lang] =
          score + (lookup == in_scores.end() ? 0.0f : lookup->second);
    }
  }

  // Second pass: add missing base languages.
  for (const auto& kv : in_scores) {
    if (seen.find(kv.first) == seen.end()) {
      out_scores.insert(kv);
      seen.insert(kv.first);
    }
  }

  return out_scores;
}

void UpdateLanguageScores(
    const float score_delta,
    const float initial_score,
    const std::vector<std::string>& langs,
    std::unordered_map<std::string, float>* const scores) {
  DCHECK(scores);

  float cur_score = initial_score;
  for (const std::string& lang : langs) {
    (*scores)[lang] += cur_score;
    cur_score = std::max(cur_score - score_delta, 0.0f);
  }
}

std::vector<LanguageModel::LanguageDetails> ScoresToDetails(
    const std::unordered_map<std::string, float>& scores) {
  // Convert map entries into language details.
  std::vector<LanguageModel::LanguageDetails> details;
  for (const auto& kv : scores) {
    details.push_back(LanguageModel::LanguageDetails(kv.first, kv.second));
  }

  // Sort the details in descending order of score. We impose a total order
  // (i.e. break score ties by language code comparison) so that the output of
  // this function is consistent even when two scores are equal.
  const auto cmp = [](const LanguageModel::LanguageDetails& a,
                      const LanguageModel::LanguageDetails& b) {
    return a.score > b.score ||
           (a.score == b.score && a.lang_code > b.lang_code);
  };
  std::sort(details.begin(), details.end(), cmp);

  // If there's no scaling to do, return the list directly.
  if (details.empty() || details[0].score == 0.0f)
    return details;

  // Normalize the scores to lie in [0, 1].
  const float scale = details[0].score;
  DCHECK(scale != 0.0f);
  for (LanguageModel::LanguageDetails& detail : details) {
    detail.score /= scale;
  }

  return details;
}

std::vector<LanguageModel::LanguageDetails> MakeHeuristicLanguageList(
    const float score_delta,
    const std::vector<std::pair<float, std::vector<std::string>>>& inputs) {
  std::unordered_map<std::string, float> scores;
  for (const std::pair<float, std::vector<std::string>>& input : inputs) {
    UpdateLanguageScores(score_delta, input.first, PromoteRegions(input.second),
                         &scores);
  }
  return ScoresToDetails(PromoteRegionScores(scores));
}

std::vector<std::string> GetUlpLanguages(
    const double confidence_cutoff,
    const double probability_cutoff,
    const base::DictionaryValue* const dict) {
  DCHECK(dict);

  std::vector<std::string> langs;

  // If there's no reading list, return an empty list.
  const base::DictionaryValue* entries = nullptr;
  if (!dict->GetDictionary(kUlpReadingKey, &entries))
    return langs;

  // If we have missing or insufficent confidence, return an empty list.
  double confidence = 0.0;
  if (!entries->GetDouble(kUlpConfidenceKey, &confidence) ||
      confidence < confidence_cutoff)
    return langs;

  // If we have no list of language preferences, return an empty list.
  const base::ListValue* preference = nullptr;
  if (!entries->GetList(kUlpPreferenceKey, &preference))
    return langs;

  // It is assumed that languages appear in descending order of probability.
  for (const auto& entry : preference->GetList()) {
    const base::DictionaryValue* item = nullptr;
    std::string language;
    double probability = 0.0;

    if (!entry.GetAsDictionary(&item) ||
        !item->GetString(kUlpLanguageKey, &language) ||
        !item->GetDouble(kUlpProbabilityKey, &probability))
      continue;

    if (probability < probability_cutoff)
      break;

    langs.push_back(language);
  }

  return langs;
}

std::vector<std::string> GetHistogramLanguages(
    const float frequency_cutoff,
    const UrlLanguageHistogram& hist) {
  std::vector<std::string> langs;
  for (const auto& info : hist.GetTopLanguages()) {
    if (info.frequency >= frequency_cutoff)
      langs.push_back(info.language_code);
  }
  return langs;
}

HeuristicLanguageModel::HeuristicLanguageModel(
    PrefService* const pref_service,
    const std::string& ui_lang,
    const std::string& accept_langs_pref,
    const std::string& ulp_pref)
    : pref_service_(pref_service),
      ui_lang_(ui_lang),
      accept_langs_pref_(accept_langs_pref),
      ulp_pref_(ulp_pref),
      lang_histogram_(pref_service) {
  DCHECK(pref_service);
  DCHECK(!ui_lang.empty());
  DCHECK(!accept_langs_pref.empty());
  DCHECK(!ulp_pref.empty());
  DCHECK(pref_service->FindPreference(accept_langs_pref));
  DCHECK(pref_service->FindPreference(ulp_pref));
}

HeuristicLanguageModel::~HeuristicLanguageModel() {}

std::vector<LanguageModel::LanguageDetails>
HeuristicLanguageModel::GetLanguages() {
  const std::vector<std::pair<float, std::vector<std::string>>> inputs = {
      {kUiWeight, {ui_lang_}},
      {kAcceptWeight,
       base::SplitString(pref_service_->GetString(accept_langs_pref_), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)},
      {kHistogramWeight,
       GetHistogramLanguages(kHistogramFrequencyCutoff, lang_histogram_)},
      {kUlpWeight, GetUlpLanguages(kUlpConfidenceCutoff, kUlpProbabilityCutoff,
                                   pref_service_->GetDictionary(ulp_pref_))},
  };
  return MakeHeuristicLanguageList(kScoreDelta, inputs);
}

}  // namespace language
