// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/test_form_filler.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

void TestFormFiller::ScheduleRefill(const FormData& form,
                                    RefillContext& refill_context,
                                    AutofillTriggerSource trigger_source,
                                    RefillTriggerReason refill_trigger_reason) {
  TriggerRefill(form, trigger_source, refill_trigger_reason);
}

}  // namespace autofill
