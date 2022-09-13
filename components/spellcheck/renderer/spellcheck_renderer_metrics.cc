// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_

#include "components/spellcheck/renderer/spellcheck_renderer_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
namespace {

// Records the duration of a spell check request. This variation is for when
// spell check is performed only by Hunspell.
void RecordHunspellSpellcheckDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES(
      "Spellcheck.Windows.SpellcheckRequestDuration.HunspellOnly", duration);
}

// Records the duration of a spell check request. This variation is for when
// spell check is performed by both Hunspell and the OS spell checker.
void RecordHybridSpellcheckDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Spellcheck.Windows.SpellcheckRequestDuration.Hybrid",
                      duration);
}

// Records the duration of a spell check request. This variation is for when
// spell check is performed only by the OS spell checker.
void RecordNativeSpellcheckDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Spellcheck.Windows.SpellcheckRequestDuration.NativeOnly",
                      duration);
}

}  // anonymous namespace
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

namespace spellcheck_renderer_metrics {

void RecordAsyncCheckedTextLength(int length) {
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.async", length);
}

void RecordCheckedTextLengthNoSuggestions(int length) {
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.check", length);
}

void RecordCheckedTextLengthWithSuggestions(int length) {
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.check.suggestions", length);
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void RecordHunspellSuggestionDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES(
      "Spellcheck.Windows.SuggestionGatheringDuration.HunspellOnly", duration);
}

void RecordHybridSuggestionDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Spellcheck.Windows.SuggestionGatheringDuration.Hybrid",
                      duration);
}

void RecordSpellcheckDuration(base::TimeDelta duration,
                              bool used_hunspell,
                              bool used_native) {
  if (used_hunspell && !used_native) {
    RecordHunspellSpellcheckDuration(duration);
  } else if (used_hunspell && used_native) {
    RecordHybridSpellcheckDuration(duration);
  } else if (!used_hunspell && used_native) {
    RecordNativeSpellcheckDuration(duration);
  }
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

}  // namespace spellcheck_renderer_metrics

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_
