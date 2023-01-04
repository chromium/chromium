// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_SIGNALS_ANNOTATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_SIGNALS_ANNOTATOR_H_

#include "components/omnibox/browser/autocomplete_result.h"

// Base class for annotating suggestions in autocomplete results with various
// signals for ML training and scoring.
class AutocompleteScoringSignalsAnnotator {
 public:
  AutocompleteScoringSignalsAnnotator() = default;
  AutocompleteScoringSignalsAnnotator(
      const AutocompleteScoringSignalsAnnotator&) = delete;
  AutocompleteScoringSignalsAnnotator& operator=(
      const AutocompleteScoringSignalsAnnotator&) = delete;
  virtual ~AutocompleteScoringSignalsAnnotator() = default;

  // Annotate the autocomplete result.
  virtual void AnnotateResult(AutocompleteResult* result) = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_SIGNALS_ANNOTATOR_H_
