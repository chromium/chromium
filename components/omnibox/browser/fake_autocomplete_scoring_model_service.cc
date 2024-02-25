// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_autocomplete_scoring_model_service.h"

#include <vector>

#include "components/omnibox/browser/autocomplete_scoring_model_service.h"

FakeAutocompleteScoringModelService::FakeAutocompleteScoringModelService()
    : AutocompleteScoringModelService(/*model_provider=*/nullptr) {}

FakeAutocompleteScoringModelService::~FakeAutocompleteScoringModelService() =
    default;

std::vector<FakeAutocompleteScoringModelService::Result>
FakeAutocompleteScoringModelService::BatchScoreAutocompleteUrlMatchesSync(
    const std::vector<
        const FakeAutocompleteScoringModelService::ScoringSignals*>&
        batch_scoring_signals) {
  std::vector<Result> results;
  for (const auto* signals : batch_scoring_signals)
    results.push_back(signals->site_engagement());
  return results;
}
