// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

namespace autofill_prediction_improvements {

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

 private:
  raw_ref<AutofillPredictionImprovementsManager> manager_;
};

AutofillPredictionImprovementsManagerTestApi test_api(
    AutofillPredictionImprovementsManager& manager) {
  return AutofillPredictionImprovementsManagerTestApi(&manager);
}

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_
