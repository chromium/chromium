// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

namespace autofill_prediction_improvements {

class AutofillPredictionImprovementsManagerTestApi {
 public:
  explicit AutofillPredictionImprovementsManagerTestApi(
      AutofillPredictionImprovementsManager* manager)
      : manager_(CHECK_DEREF(manager)) {}

  void SetCache(std::optional<autofill::FormData> cache) {
    manager_->cache_ = cache;
  }

  void SetAddressSuggestions(
      std::vector<autofill::Suggestion> address_suggestions) {
    manager_->address_suggestions_ = address_suggestions;
  }

 private:
  raw_ref<AutofillPredictionImprovementsManager> manager_;
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_TEST_API_H_
