// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_TYPE_UTIL_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_TYPE_UTIL_H_

#include <memory>

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/password_manager/core/browser/password_form.h"

namespace autofill {

// Picks the most relevant of the FieldTypes encapsulated by the given
// AutofillType to a single one: Password Manager > Credit > Addresses.
FieldType GetMostRelevantFieldType(const AutofillType& type);

// Parses `form_structure` into a `PasswordForm` using
// `password_manager::FormDataParser`, or returns `nullptr` if it is not a
// password form.
std::unique_ptr<password_manager::PasswordForm> ParseToPasswordForm(
    const FormStructure& form_structure);

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_TYPE_UTIL_H_
