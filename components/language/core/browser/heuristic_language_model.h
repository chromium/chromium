// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_HEURISTIC_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_HEURISTIC_LANGUAGE_MODEL_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/url_language_histogram.h"

namespace base {
class DictionaryValue;
}  // namespace base

class PrefService;

namespace language {

// A model that heuristically assigns scores to languages that appear in the
// user's various language lists.
//
// First, languages are scored by their rank in the following lists (which are
// presented in descending order of importance):
//  1) (Singleton) UI language list
//  2) Accept language list
//  3) URL language histogram
//  4) User Profile's (ULP) "reading list"
// If a language and its base language (e.g. "en-AU" and "en") both appear in
// the same input list, the region-specific language is promoted to appear
// directly above the base language, and the base language is discarded.
//
// The language scores are then combined into one output list. The combination
// process has the following properties:
//  - A language's scores from each input list are summed.
//  - If a language and its base language have both been scored, the region-
//    specific language is assigned the sum of the two scores and the base
//    language is not included in the model.
//  - Languages are output in descending order of score, with scores normalized
//    to lie in [0, 1].
//
// NOTE: This model reads the accept language list, URL language histogram and
//       reading list from user preferences. Hence, these sources must be
//       registered on the PrefService passed in at construction time.
class HeuristicLanguageModel : public LanguageModel {
 public:
  HeuristicLanguageModel(PrefService* pref_service,
                         const std::string& ui_lang,
                         const std::string& accept_langs_pref,
                         const std::string& ulp_pref);
  ~HeuristicLanguageModel() override;

  // LanguageModel implementation.
  std::vector<LanguageDetails> GetLanguages() override;

 private:
  const PrefService* const pref_service_;
  const std::string ui_lang_;
  const std::string accept_langs_pref_;
  const std::string ulp_pref_;
  const UrlLanguageHistogram lang_histogram_;
};

// The following functions are exposed for testing only.

// Given a language code |lang|, returns true if |lang| includes a
// region-specific subcode (e.g. "en-AU"). Sets |base| to the base subcode (e.g.
// "en") of |lang| if it can be extracted, or the empty string otherwise.
bool HasBaseAndRegion(const std::string& lang, std::string* base);

// Accepts a list |in_langs| of language codes, and returns a copy that includes
// the following modifications:
//  - If any region-specific code (e.g. "en-AU") appears after its base (e.g.
//    "en") the list, it is moved directly in front of the base code. Multiple
//    such region-specific codes will all be promoted, and their relative
//    ordering will be preserved.
//  - If the list contains both a region-specific code and its base, the base is
//    removed.
std::vector<std::string> PromoteRegions(
    const std::vector<std::string>& in_langs);

// Accepts a map |in_scores| of language codes to scores, and returns a copy
// that includes the following modifications:
//  - The score for each region-specific code (e.g. "en-AU") is incremented by
//    the score of its base code (e.g. "en") if it is also present in the map.
//  - If the map contains both a region-specific code and its base, the base is
//    removed.
std::unordered_map<std::string, float> PromoteRegionScores(
    const std::unordered_map<std::string, float>& in_scores);

// Given an existing map |scores| of language codes to scores, and an list
// |langs| of input codes, increments the score of each input code by
//     |initial_score| - r * |score_delta|
// (if this value is positive), where r is the rank of the code in the input
// list.
void UpdateLanguageScores(float score_delta,
                          float initial_score,
                          const std::vector<std::string>& langs,
                          std::unordered_map<std::string, float>* scores);

// Converts a map |scores| of (language code, scores) pairs into a list of
// |LanguageDetails|, with entries ordered by descending score and linearly
// scaled such that the first (non-zero) entry has score 1.0.
std::vector<LanguageModel::LanguageDetails> ScoresToDetails(
    const std::unordered_map<std::string, float>& scores);

// Constructs a heuristically-scored language list from a vector |inputs| of
// (language source, source weight) pairs and a rank penalty |score_delta|.
std::vector<LanguageModel::LanguageDetails> MakeHeuristicLanguageList(
    float score_delta,
    const std::vector<std::pair<float, std::vector<std::string>>>& inputs);

// Outputs language codes from the ULP "reading list" stored in |dict|. If a
// given language has probability less than |probability_cutoff| it is not
// included, and no languages are included if the reading list has confidence
// less than |confidence_cutoff|.
std::vector<std::string> GetUlpLanguages(double confidence_cutoff,
                                         double probability_cutoff,
                                         const base::DictionaryValue* dict);

// Returns the list of language codes in |hist| with frequency no less than
// |frequency_cutoff|.
std::vector<std::string> GetHistogramLanguages(
    float frequency_cutoff,
    const UrlLanguageHistogram& hist);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_HEURISTIC_LANGUAGE_MODEL_H_
