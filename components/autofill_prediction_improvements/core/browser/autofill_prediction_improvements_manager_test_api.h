// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

namespace autofill_prediction_improvements {

class AutofillPredictionImprovementsLogger;

class AutofillPredictionImprovementsManagerTestApi {
 public:
  explicit AutofillPredictionImprovementsManagerTestApi(
      AutofillPredictionImprovementsManager* manager)
      : manager_(CHECK_DEREF(manager)) {}

  void SetCache(
      std::optional<
          AutofillPredictionImprovementsFillingEngine::PredictionsByGlobalId>
          cache) {
    manager_->cache_ = cache;
  }

  void SetLastQueriedFormGlobalId(
      std::optional<autofill::FormGlobalId> last_queried_form_global_id) {
    manager_->last_queried_form_global_id_ = last_queried_form_global_id;
  }

  void SetAutofillSuggestions(
      std::vector<autofill::Suggestion> autofill_suggestions) {
    manager_->autofill_suggestions_ = autofill_suggestions;
  }

  void SetFeedbackId(std::optional<std::string> feedback_id) {
    manager_->feedback_id_ = feedback_id;
  }

  const base::OneShotTimer& loading_suggestion_timer() {
    return manager_->loading_suggestion_timer_;
  }

  void SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState
          prediction_retrieval_state) {
    manager_->prediction_retrieval_state_ = prediction_retrieval_state;
  }

  void SetErrorOrNoInfoSuggestionShown(bool error_or_no_info_suggestion_shown) {
    manager_->error_or_no_info_suggestion_shown_ =
        error_or_no_info_suggestion_shown;
  }

  AutofillPredictionImprovementsLogger& logger() { return manager_->logger_; }

  bool ShouldSkipAutofillSuggestion(
      const autofill::FormData& form,
      const autofill::Suggestion& autofill_suggestion) {
    return manager_->ShouldSkipAutofillSuggestion(form, autofill_suggestion);
  }

 private:
  raw_ref<AutofillPredictionImprovementsManager> manager_;
};

inline AutofillPredictionImprovementsManagerTestApi test_api(
    AutofillPredictionImprovementsManager& manager) {
  return AutofillPredictionImprovementsManagerTestApi(&manager);
}

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_
