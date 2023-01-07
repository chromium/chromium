// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

// A namespace for recording spell-check related histograms.
// This namespace encapsulates histogram names and metrics API for the renderer
// side of the spell checker.
namespace spellcheck_renderer_metrics {

// The length of text checked via async checking.
void RecordAsyncCheckedTextLength(int length);

// The length of text checked by spellCheck. No replacement suggestions were
// requested.
void RecordCheckedTextLengthNoSuggestions(int length);

// The length of text checked by spellCheck. Replacement suggestions were
// requested.
void RecordCheckedTextLengthWithSuggestions(int length);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Records the duration of gathering spelling suggestions. This variation is for
// when spell check is performed only by Hunspell.
void RecordHunspellSuggestionDuration(base::TimeDelta duration);

// Records the duration of gathering spelling suggestions. This variation is for
// when spell check is performed by both Hunspell and the OS spell checker.
void RecordHybridSuggestionDuration(base::TimeDelta duration);

// Records the total time it took to complete an end-to-end spell check.
// If at least one locale was checked by Hunspell, |used_hunspell| should be set
// to |true|. If at least one locale was checked by the Windows native spell
// checker, |used_native| should be set to |true|.
void RecordSpellcheckDuration(base::TimeDelta duration,
                              bool used_hunspell,
                              bool used_native);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

}  // namespace spellcheck_renderer_metrics

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_
