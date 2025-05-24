// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_TEST_FORM_FILLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_TEST_FORM_FILLER_H_

#include "components/autofill/core/browser/filling/form_filler.h"

namespace autofill {

// Manages filling forms and fields.
class TestFormFiller : public FormFiller {
 public:
  using FormFiller::FormFiller;

 private:
  // Directly calls TriggerRefill.
  void ScheduleRefill(const FormData& form,
                      RefillContext& refill_context,
                      AutofillTriggerSource trigger_source,
                      RefillTriggerReason refill_trigger_reason) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_TEST_FORM_FILLER_H_
