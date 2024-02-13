// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_host_metrics.h"

#include <stdint.h>

#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"

SpellCheckHostMetrics::SpellCheckHostMetrics()
    : misspelled_word_count_(0),
      last_misspelled_word_count_(-1),
      spellchecked_word_count_(0),
      last_spellchecked_word_count_(-1),
      suggestion_show_count_(0),
      last_suggestion_show_count_(-1),
      replaced_word_count_(0),
      last_replaced_word_count_(-1),
      last_unique_word_count_(0),
      start_time_(base::TimeTicks::Now()) {
  const uint64_t kHistogramTimerDurationInMinutes = 30;
  recording_timer_.Start(FROM_HERE,
                         base::Minutes(kHistogramTimerDurationInMinutes), this,
                         &SpellCheckHostMetrics::OnHistogramTimerExpired);
  RecordWordCounts();
}

SpellCheckHostMetrics::~SpellCheckHostMetrics() {
}

void SpellCheckHostMetrics::RecordEnabledStats(bool enabled) {
  base::UmaHistogramBoolean("SpellCheck.Enabled2", enabled);
}

void SpellCheckHostMetrics::RecordCheckedWordStats(const std::u16string& word,
                                                   bool misspell) {
  spellchecked_word_count_++;
  if (misspell) {
    misspelled_word_count_++;
    // If an user misspelled, that user should be counted as a part of
    // the population.  So we ensure to instantiate the histogram
    // entries here at the first time.
    if (misspelled_word_count_ == 1)
      RecordReplacedWordStats(0);
  }

  // Collects actual number of checked words, excluding duplication.
  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(word), &digest);
  checked_word_hashes_.insert(base::MD5DigestToBase16(digest));

  RecordWordCounts();
}

void SpellCheckHostMetrics::OnHistogramTimerExpired() {
  if (0 < spellchecked_word_count_) {
    // Collects word checking rate, which is represented
    // as a word count per hour.
    base::TimeDelta since_start = base::TimeTicks::Now() - start_time_;
    // This shouldn't happen since OnHistogramTimerExpired() is called on
    // a 30 minute interval. If the time was 0 we will end up dividing by zero.
    CHECK_NE(0, since_start.InSeconds());
    size_t checked_words_per_hour = spellchecked_word_count_ *
                                    base::Hours(1).InSeconds() /
                                    since_start.InSeconds();
    base::UmaHistogramCounts1M(
        "SpellCheck.CheckedWordsPerHour",
        base::saturated_cast<int>(checked_words_per_hour));
  }
}

void SpellCheckHostMetrics::RecordSuggestionStats(int delta) {
  suggestion_show_count_ += delta;
  // RecordReplacedWordStats() Calls RecordWordCounts() eventually.
  RecordReplacedWordStats(0);
}

void SpellCheckHostMetrics::RecordReplacedWordStats(int delta) {
  replaced_word_count_ += delta;

  if (suggestion_show_count_) {
    int percentage = (100 * replaced_word_count_) / suggestion_show_count_;
    UMA_HISTOGRAM_PERCENTAGE("SpellCheck.SuggestionHitRatio", percentage);
  }

  RecordWordCounts();
}

void SpellCheckHostMetrics::RecordWordCounts() {
  if (spellchecked_word_count_ != last_spellchecked_word_count_) {
    DCHECK(spellchecked_word_count_ > last_spellchecked_word_count_);
    UMA_HISTOGRAM_COUNTS_1M("SpellCheck.CheckedWords",
                            spellchecked_word_count_);
    last_spellchecked_word_count_ = spellchecked_word_count_;
  }

  if (misspelled_word_count_ != last_misspelled_word_count_) {
    DCHECK(misspelled_word_count_ > last_misspelled_word_count_);
    UMA_HISTOGRAM_COUNTS_1M("SpellCheck.MisspelledWords",
                            misspelled_word_count_);
    last_misspelled_word_count_ = misspelled_word_count_;
  }

  if (replaced_word_count_ != last_replaced_word_count_) {
    DCHECK(replaced_word_count_ > last_replaced_word_count_);
    UMA_HISTOGRAM_COUNTS_1M("SpellCheck.ReplacedWords", replaced_word_count_);
    last_replaced_word_count_ = replaced_word_count_;
  }

  if (checked_word_hashes_.size() != last_unique_word_count_) {
    DCHECK(checked_word_hashes_.size() > last_unique_word_count_);
    UMA_HISTOGRAM_COUNTS_1M(
        "SpellCheck.UniqueWords",
        base::saturated_cast<int>(checked_word_hashes_.size()));
    last_unique_word_count_ = checked_word_hashes_.size();
  }

  if (suggestion_show_count_ != last_suggestion_show_count_) {
    DCHECK(suggestion_show_count_ > last_suggestion_show_count_);
    UMA_HISTOGRAM_COUNTS_1M("SpellCheck.ShownSuggestions",
                            suggestion_show_count_);
    last_suggestion_show_count_ = suggestion_show_count_;
  }
}

void SpellCheckHostMetrics::RecordSpellingServiceStats(bool enabled) {
  base::UmaHistogramBoolean("SpellCheck.SpellingService.Enabled2", enabled);
}

#if BUILDFLAG(IS_WIN)
// static
void SpellCheckHostMetrics::RecordAcceptLanguageStats(
    const LocalesSupportInfo& locales_info) {
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.ChromeLocalesSupport2.Both",
      base::saturated_cast<int>(
          locales_info.locales_supported_by_hunspell_and_native),
      20);
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.ChromeLocalesSupport2.HunspellOnly",
      base::saturated_cast<int>(
          locales_info.locales_supported_by_hunspell_only),
      20);
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.ChromeLocalesSupport2.NativeOnly",
      base::saturated_cast<int>(locales_info.locales_supported_by_native_only),
      20);
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.ChromeLocalesSupport2.NoSupport",
      base::saturated_cast<int>(locales_info.unsupported_locales), 20);
}

// static
void SpellCheckHostMetrics::RecordSpellcheckLanguageStats(
    const LocalesSupportInfo& locales_info) {
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.SpellcheckLocalesSupport2.Both",
      base::saturated_cast<int>(
          locales_info.locales_supported_by_hunspell_and_native),
      20);
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.SpellcheckLocalesSupport2.HunspellOnly",
      base::saturated_cast<int>(
          locales_info.locales_supported_by_hunspell_only),
      20);
  base::UmaHistogramExactLinear(
      "Spellcheck.Windows.SpellcheckLocalesSupport2.NativeOnly",
      base::saturated_cast<int>(locales_info.locales_supported_by_native_only),
      20);
}
#endif  // BUILDFLAG(IS_WIN)
