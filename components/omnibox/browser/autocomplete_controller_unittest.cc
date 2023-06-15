// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"

class FakeAutocompleteScoringModelService
    : public AutocompleteScoringModelService {
 public:
  FakeAutocompleteScoringModelService()
      : AutocompleteScoringModelService(/*model_provider=*/nullptr) {}
  // AutocompleteScoringModelService:
  void ScoreAutocompleteUrlMatch(base::CancelableTaskTracker* tracker,
                                 const ScoringSignals& scoring_signals,
                                 const std::string& match_destination_url,
                                 ResultCallback result_callback) override {
    // TODO(crbug/1405555): Properly stub this function.
  }

  void BatchScoreAutocompleteUrlMatches(
      base::CancelableTaskTracker* tracker,
      const std::vector<const ScoringSignals*>& batch_scoring_signals,
      const std::vector<std::string>& stripped_destination_urls,
      BatchResultCallback batch_result_callback) override {
    // TODO(crbug/1405555): Properly stub this function.
  }
};
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

class AutocompleteControllerTest : public testing::Test {
 public:
  AutocompleteControllerTest() = default;

  void SetUp() override {
    auto provider_client = std::make_unique<FakeAutocompleteProviderClient>();

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    provider_client->set_scoring_model_service(
        std::make_unique<FakeAutocompleteScoringModelService>());
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

    controller_ = std::make_unique<AutocompleteController>(
        std::move(provider_client), 0, false);
  }

  void set_autocomplete_matches(std::vector<AutocompleteMatch>& matches) {
    controller_->result_.Reset();
    controller_->result_.AppendMatches(matches);
  }

 protected:
  std::unique_ptr<AutocompleteController> controller_;

  base::test::TaskEnvironment task_environment_;
};
