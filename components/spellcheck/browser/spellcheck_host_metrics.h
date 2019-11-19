// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_HOST_METRICS_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_HOST_METRICS_H_

#include <stddef.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

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

  // Collects the number of words in the custom dictionary, which is
  // to be uploaded via UMA.
  static void RecordCustomWordCountStats(size_t count);

  // Collects status of spellchecking enabling state, which is
  // to be uploaded via UMA
  void RecordEnabledStats(bool enabled);

  // Collects a histogram for dictionary corruption rate
  // to be uploaded via UMA
  void RecordDictionaryCorruptionStats(bool corrupted);

  // Collects status of spellchecking enabling state, which is
  // to be uploaded via UMA
  void RecordCheckedWordStats(const base::string16& word, bool misspell);

  // Collects a histogram for misspelled word replacement
  // to be uploaded via UMA
  void RecordReplacedWordStats(int delta);

  // Collects a histogram for context menu showing as a spell correction
  // attempt to be uploaded via UMA
  void RecordSuggestionStats(int delta);

  // Records if spelling service is enabled or disabled.
  void RecordSpellingServiceStats(bool enabled);

#if defined(OS_WIN)
  // Records how many user spellcheck languages are currently not supported by
  // the Windows OS spellchecker due to missing language packs.
  void RecordMissingLanguagePacksCount(int count);

  // Records how many user languages are not supported by Hunspell for
  // spellchecking.
  void RecordHunspellUnsupportedLanguageCount(int count);
#endif  // defined(OS_WIN)

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
