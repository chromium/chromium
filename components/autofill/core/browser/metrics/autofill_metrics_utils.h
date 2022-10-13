// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_

#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

// Returns true if the `field` has at least one possible field type. A possible
// field type is every type that is neither UNKNOWN_TYPE or EMPTY_TYPE. The
// latter should never occur together with a possible field type.
// Should only be invoked after the possible types of `field` have been
// determined.
bool FieldHasMeaningfulFieldTypes(const AutofillField& field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
