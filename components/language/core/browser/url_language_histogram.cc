// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/url_language_histogram.h"

#include <algorithm>
#include <map>
#include <set>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace language {

namespace {

const char kUrlLanguageHistogramCounters[] = "language_model_counters";

const int kMaxCountersSum = 1000;
const int kMinCountersSum = 10;
const float kCutoffRatio = 0.005f;
const float kDiscountFactor = 0.75f;

// Gets the sum of the counter for all languages in the histogram.
int GetCountersSum(const base::DictionaryValue& dict) {
  int sum = 0;
  for (base::DictionaryValue::Iterator itr(dict); !itr.IsAtEnd();
       itr.Advance()) {
    if (itr.value().is_int())
      sum += itr.value().GetInt();
  }
  return sum;
}

// Removes languages with small counter values and discount remaining counters.
void DiscountAndCleanCounters(base::DictionaryValue* dict) {
  std::set<std::string> remove_keys;

  for (base::DictionaryValue::Iterator itr(*dict); !itr.IsAtEnd();
       itr.Advance()) {
    // Remove languages with invalid or small values.
    if (!itr.value().is_int() ||
        itr.value().GetInt() < (kCutoffRatio * kMaxCountersSum)) {
      remove_keys.insert(itr.key());
      continue;
    }

    // Discount the value.
    dict->SetInteger(itr.key(), itr.value().GetInt() * kDiscountFactor);
  }

  for (const std::string& lang_to_remove : remove_keys)
    dict->Remove(lang_to_remove, nullptr);
}

// Transforms the counters from prefs into a list of LanguageInfo structs.
std::vector<UrlLanguageHistogram::LanguageInfo> GetAllLanguages(
    const base::DictionaryValue& dict) {
  int counters_sum = GetCountersSum(dict);

  // If the sample is not large enough yet, pretend there are no top languages.
  if (counters_sum < kMinCountersSum)
    return std::vector<UrlLanguageHistogram::LanguageInfo>();

  std::vector<UrlLanguageHistogram::LanguageInfo> top_languages;
  for (base::DictionaryValue::Iterator itr(dict); !itr.IsAtEnd();
       itr.Advance()) {
    if (!itr.value().is_int())
      continue;
    top_languages.emplace_back(
        itr.key(), static_cast<float>(itr.value().GetInt()) / counters_sum);
  }
  return top_languages;
}

}  // namespace

UrlLanguageHistogram::UrlLanguageHistogram(PrefService* pref_service)
    : pref_service_(pref_service) {}

UrlLanguageHistogram::~UrlLanguageHistogram() = default;

// static
void UrlLanguageHistogram::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kUrlLanguageHistogramCounters);
}

std::vector<UrlLanguageHistogram::LanguageInfo>
UrlLanguageHistogram::GetTopLanguages() const {
  std::vector<UrlLanguageHistogram::LanguageInfo> top_languages =
      GetAllLanguages(
          *pref_service_->GetDictionary(kUrlLanguageHistogramCounters));

  std::sort(top_languages.begin(), top_languages.end(),
            [](UrlLanguageHistogram::LanguageInfo a,
               UrlLanguageHistogram::LanguageInfo b) {
              return a.frequency > b.frequency;
            });

  return top_languages;
}

float UrlLanguageHistogram::GetLanguageFrequency(
    const std::string& language_code) const {
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(kUrlLanguageHistogramCounters);
  int counters_sum = GetCountersSum(*dict);
  // If the sample is not large enough yet, pretend there are no top languages.
  if (counters_sum < kMinCountersSum)
    return 0;

  int counter_value = 0;
  // If the key |language_code| does not exist, |counter_value| stays 0.
  dict->GetInteger(language_code, &counter_value);

  return static_cast<float>(counter_value) / counters_sum;
}

void UrlLanguageHistogram::OnPageVisited(const std::string& language_code) {
  DictionaryPrefUpdate update(pref_service_, kUrlLanguageHistogramCounters);
  base::DictionaryValue* dict = update.Get();
  int counter_value = 0;
  // If the key |language_code| does not exist, |counter_value| stays 0.
  dict->GetInteger(language_code, &counter_value);
  dict->SetInteger(language_code, counter_value + 1);

  if (GetCountersSum(*dict) > kMaxCountersSum)
    DiscountAndCleanCounters(dict);
}

void UrlLanguageHistogram::ClearHistory(base::Time begin, base::Time end) {
  // Ignore all partial removals and react only to "entire" history removal.
  bool is_entire_history = (begin == base::Time() && end == base::Time::Max());
  if (!is_entire_history) {
    return;
  }

  pref_service_->ClearPref(kUrlLanguageHistogramCounters);
}

}  // namespace language
