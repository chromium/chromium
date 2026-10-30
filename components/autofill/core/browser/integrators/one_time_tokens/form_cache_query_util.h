// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_FORM_CACHE_QUERY_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_FORM_CACHE_QUERY_UTIL_H_

// Stateless query utilities over `AutofillManager`'s form cache for locating
// OTP forms and fields, checking user-typed input, and constructing
// `OtpTarget` descriptors. Stateful field tracking or cross-frame resolution
// belongs in `OtpTargetFieldIdentifier`.

#include <optional>

#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class FormStructure;
struct OtpTarget;

// Finds `field_id` in `manager`'s form cache (using `form_id` if provided) and
// returns the `AutofillManager::FormAndField` if it is an eligible OTP field in
// an OTP form, or `{nullptr, nullptr}` otherwise.
AutofillManager::FormAndField FindOtpField(const AutofillManager& manager,
                                           std::optional<FormGlobalId> form_id,
                                           FieldGlobalId field_id);

// Returns the first eligible OTP field in `manager`'s cached forms, or
// `{nullptr, nullptr}` if none exists.
AutofillManager::FormAndField FirstEligibleOtpField(
    const AutofillManager& manager);

// Returns true if any OTP field in `manager`'s cached forms already contains
// user-typed input (`FieldModifier::kUser`).
bool AnyOtpFieldContainsTypedInput(const AutofillManager& manager);

// Returns true if `manager` has at least one cached OTP form.
bool HasOtpForm(const AutofillManager& manager);

// Builds an `OtpTarget` for an OTP `field` belonging to `form` owned by
// `manager`. Preserves `field.origin()` as-is (including when opaque).
OtpTarget BuildOtpTarget(AutofillManager& manager,
                         const FormStructure& form,
                         const AutofillField& field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_FORM_CACHE_QUERY_UTIL_H_
