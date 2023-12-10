// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_

#include <string>
#include <vector>

#include "components/omnibox/browser/autocomplete_scoring_model_service.h"

class FakeAutocompleteScoringModelService
    : public AutocompleteScoringModelService {
 public:
  FakeAutocompleteScoringModelService();
  ~FakeAutocompleteScoringModelService() override;

  // AutocompleteScoringModelService:
  std::vector<Result> BatchScoreAutocompleteUrlMatchesSync(
      const std::vector<const ScoringSignals*>& batch_scoring_signals,
      const std::vector<std::string>& stripped_destination_urls) override;

  std::vector<Result> fake_response_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
