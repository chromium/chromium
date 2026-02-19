// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_FORM_PREDICTIONS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_FORM_PREDICTIONS_TRACKER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/form_predictions_tracker.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockFormPredictionsTracker : public FormPredictionsTracker {
 public:
  explicit MockFormPredictionsTracker(AutofillClient* client);
  ~MockFormPredictionsTracker() override;

  MOCK_METHOD(void,
              Wait,
              (base::OnceClosure callback, base::TimeDelta timeout),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_FORM_PREDICTIONS_TRACKER_H_
