// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_URL_LANGUAGE_HISTOGRAM_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_URL_LANGUAGE_HISTOGRAM_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace language {

// Collects data about languages in which the user reads the web and provides
// access to current estimated language preferences. The past behaviour is
// discounted so that the histogram reflects changes in browsing habits. This
// histogram does not have to contain all languages that ever appeared in user's
// browsing, languages with insignificant frequency are removed, eventually.
//
// Operates as a "wrapper" around profile preferences: the state of the
// histogram is read from/written to the PrefService in each method call. This
// allows multiple instances of the histogram to be used in a (non-overlapping)
// sequence without any instance-specific state going "out of sync". This
// behaviour is relied on by clients of the histogram.
class UrlLanguageHistogram : public KeyedService {
 public:
  struct LanguageInfo {
    LanguageInfo() = default;
    LanguageInfo(const std::string& language_code, float frequency)
        : language_code(language_code), frequency(frequency) {}

    // The ISO 639 language code.
    std::string language_code;

    // The current estimated frequency of the language share, a number between 0
    // and 1 (can be understood as the probability that the next page the user
    // opens is in this language). Frequencies over all LanguageInfos from
    // GetTopLanguages() sum to 1 (unless there are no top languages, yet).
    float frequency = 0.0f;
  };

  explicit UrlLanguageHistogram(PrefService* pref_service);
  ~UrlLanguageHistogram() override;

  // Registers profile prefs for the histogram.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns a list of the languages currently tracked by the histogram, sorted
  // by frequency in decreasing order. The list is empty, if the histogram has
  // not enough data points.
  std::vector<LanguageInfo> GetTopLanguages() const;

  // Returns the estimated frequency for the given language or 0 if the language
  // is not among the top languages kept in the histogram.
  float GetLanguageFrequency(const std::string& language_code) const;

  // Informs the histogram that a page with the given language has been visited.
  void OnPageVisited(const std::string& language_code);

  // Reflect in the histogram that history from |begin| to |end| gets cleared.
  void ClearHistory(base::Time begin, base::Time end);

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(UrlLanguageHistogram);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_URL_LANGUAGE_HISTOGRAM_H_
