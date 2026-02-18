// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/form_predictions_tracker.h"

namespace autofill {

class FormPredictionsTrackerTestApi {
 public:
  explicit FormPredictionsTrackerTestApi(FormPredictionsTracker* tracker)
      : tracker_(CHECK_DEREF(tracker)) {}

  const absl::flat_hash_map<FormGlobalId,
                            FormPredictionsTracker::FormParsingStatus>&
  form_parsing_status() const {
    return tracker_->form_parsing_status_;
  }

  size_t num_callbacks() const { return tracker_->callbacks_.size(); }

 private:
  const raw_ref<FormPredictionsTracker> tracker_;
};

FormPredictionsTrackerTestApi test_api(FormPredictionsTracker& tracker) {
  return FormPredictionsTrackerTestApi(&tracker);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_TEST_API_H_
