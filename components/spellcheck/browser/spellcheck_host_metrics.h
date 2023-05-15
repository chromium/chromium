// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_HOST_METRICS_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_HOST_METRICS_H_

#include <stddef.h>

#include <string>
#include <unordered_set>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
// Simple struct to keep track of how many languages are supported by which
// spell checker.
struct LocalesSupportInfo {
  size_t locales_supported_by_hunspell_and_native;
  size_t locales_supported_by_hunspell_only;
  size_t locales_supported_by_native_only;
  size_t unsupported_locales;
};
#endif  // BUILDFLAG(IS_WIN)

// A helper object for recording spell-check related histograms.
// This class encapsulates histogram names and metrics API.
// This also carries a set of counters for collecting histograms
// and a timer for making a periodical summary.
//
// We expect a user of SpellCheckHost class to instantiate this object,
// and pass the metrics object to SpellCheckHost's factory method.
//
//   metrics.reset(new SpellCheckHostMetrics());
//   spell_check_host = SpellChecHost::Create(...., metrics.get());
//
// The lifetime of the object should be managed by a caller,
// and the lifetime should be longer than SpellCheckHost instance
// because SpellCheckHost will use the object.
class SpellCheckHostMetrics {
 public:
  SpellCheckHostMetrics();
  ~SpellCheckHostMetrics();

  // Collects status of spellchecking enabling state, which is
  // to be uploaded via UMA. Intended to be called only for regular
  // profiles (not system, guest, incognito, etc.).
  void RecordEnabledStats(bool enabled);

  // Collects status of spellchecking enabling state, which is
  // to be uploaded via UMA
  void RecordCheckedWordStats(const std::u16string& word, bool misspell);

  // Collects a histogram for misspelled word replacement
  // to be uploaded via UMA
  void RecordReplacedWordStats(int delta);

  // Collects a histogram for context menu showing as a spell correction
  // attempt to be uploaded via UMA
  void RecordSuggestionStats(int delta);

  // Records if spelling service is enabled or disabled. Intended to be called
  // only for regular profiles (not system, guest, incognito, etc.).
  void RecordSpellingServiceStats(bool enabled);

#if BUILDFLAG(IS_WIN)
  // Records spell check support for user-added Chrome languages that are not
  // eligible for spell checking (due to the hard-coded spell check locales
  // list).
  static void RecordAcceptLanguageStats(const LocalesSupportInfo& locales_info);

  // Records which spell checker can handle which enabled spell check locales.
  static void RecordSpellcheckLanguageStats(
      const LocalesSupportInfo& locales_info);
#endif  // BUILDFLAG(IS_WIN)

 private:
  friend class SpellcheckHostMetricsTest;
  void OnHistogramTimerExpired();

  // Records various counters without changing their values.
  void RecordWordCounts();

  // Number of corrected words of checked words.
  int misspelled_word_count_;
  int last_misspelled_word_count_;

  // Number of checked words.
  int spellchecked_word_count_;
  int last_spellchecked_word_count_;

  // Number of suggestion list showings.
  int suggestion_show_count_;
  int last_suggestion_show_count_;

  // Number of misspelled words replaced by a user.
  int replaced_word_count_;
  int last_replaced_word_count_;

  // Last recorded number of unique words.
  size_t last_unique_word_count_;

  // Time when first spellcheck happened.
  base::TimeTicks start_time_;
  // Set of checked words in the hashed form.
  std::unordered_set<std::string> checked_word_hashes_;
  base::RepeatingTimer recording_timer_;
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_HOST_METRICS_H_
