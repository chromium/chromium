// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

namespace autofill_ai {

class AutofillAiLogger;

class AutofillAiManagerTestApi {
 public:
  explicit AutofillAiManagerTestApi(AutofillAiManager* manager)
      : manager_(CHECK_DEREF(manager)) {}

  void SetCache(
      std::optional<AutofillAiModelExecutor::PredictionsByGlobalId> cache) {
    manager_->cache_ = cache;
  }

  std::optional<AutofillAiModelExecutor::PredictionsByGlobalId> GetCache() {
    return manager_->cache_;
  }

  void SetLastQueriedFormGlobalId(
      std::optional<autofill::FormGlobalId> last_queried_form_global_id) {
    manager_->last_queried_form_global_id_ = last_queried_form_global_id;
  }

  void SetAutofillSuggestions(
      std::vector<autofill::Suggestion> autofill_suggestions) {
    manager_->autofill_suggestions_ = autofill_suggestions;
  }

  void SetFormFillingPredictionsModelExecutionId(
      std::optional<std::string> model_execution_id) {
    manager_->form_filling_predictions_model_execution_id_ = model_execution_id;
  }

  const base::OneShotTimer& loading_suggestion_timer() {
    return manager_->loading_suggestion_timer_;
  }

  void SetPredictionRetrievalState(
      AutofillAiManager::PredictionRetrievalState prediction_retrieval_state) {
    manager_->prediction_retrieval_state_ = prediction_retrieval_state;
  }

  void SetErrorOrNoInfoSuggestionShown(bool error_or_no_info_suggestion_shown) {
    manager_->error_or_no_info_suggestion_shown_ =
        error_or_no_info_suggestion_shown;
  }

  AutofillAiLogger& logger() { return manager_->logger_; }

 private:
  raw_ref<AutofillAiManager> manager_;
};

inline AutofillAiManagerTestApi test_api(AutofillAiManager& manager) {
  return AutofillAiManagerTestApi(&manager);
}

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_TEST_API_H_
