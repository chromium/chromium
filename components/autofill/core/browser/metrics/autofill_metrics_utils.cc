// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

bool FieldHasMeaningfulFieldTypes(const AutofillField& field) {
  // This function should only be invoked when the possible types have been
  // determined.
  DCHECK(!field.possible_types().empty());

  if (field.possible_types().contains_any({UNKNOWN_TYPE, EMPTY_TYPE})) {
    // If either UNKNOWN_TYPE or EMPTY_TYPE is present, there should be no other
    // type
    DCHECK_EQ(field.possible_types().size(), 1u);
    return false;
  }
  return true;
}

}  // namespace autofill
