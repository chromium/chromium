// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_QUALIFIERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_QUALIFIERS_H_

#include <stddef.h>

#include <algorithm>

#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// This file contains several functions that test properties of FormData and
// FormStructure.
//
// Since some functions exist for both FormData and FormStructure and their,
// this file contains both implementations. Otherwise, we'd have to maintain
// equivalent implementations in both classes.
//
// TODO(crbug.com/40232021): Simplify this redundancy when FormData and
// FormStructure have a formal relationship (like composition or inheritance).

class FormData;
class FormStructure;
class LogManager;

// Permissions indicating which parsing operations Autofill may perform.
enum class FormParsingPermission {
  // General heuristic predictions should be computed for fields of this form.
  kHeuristics,
  // Single-field heuristic predictions should be computed for fields of this
  // form.
  kSingleFieldHeuristics,
  // Server predictions should be queried for fields of this form.
  kServerQuery,
  // Autofill should upload votes for fields of this form.
  kServerUpload,
  kMaxValue = kServerUpload,
};

// Returns the set of parsing permissions for `form`.
[[nodiscard]] DenseSet<FormParsingPermission> GetFormParsingPermissions(
    const FormData& form,
    bool ignore_small_forms,
    LogManager* log_manager = nullptr);
[[nodiscard]] DenseSet<FormParsingPermission> GetFormParsingPermissions(
    const FormStructure& form,
    bool ignore_small_forms);

// Returns whether the form is considered parseable and meets a couple of other
// requirements which makes uploading UKM data worthwhile. For example, the form
// should not be a search form, the forms should have at least one focusable
// input field with a type from heuristics or the server.
[[nodiscard]] bool ShouldUploadUkm(const FormStructure& form,
                                   bool require_classified_field);

// Runs a quick heuristic to rule out forms that are obviously not
// autofillable, like google/yahoo/msn search, etc.
[[nodiscard]] bool IsAutofillable(const FormStructure& form);

// Production code only uses the default parameters.
// Exposed publicly for testing. Production code only uses the default values.
struct FormParsingPermissionsParams {
  size_t min_required_fields =
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
  size_t required_fields_for_forms_with_only_password_fields =
      kRequiredFieldsForFormsWithOnlyPasswordFields;
};

// Variants of GetFormParsingPermissions() that additionally take
// FormParsingPermissionsParams.
[[nodiscard]] DenseSet<FormParsingPermission>
GetFormParsingPermissionsForTest(  // IN-TEST
    const FormData& form,
    bool ignore_small_forms,
    FormParsingPermissionsParams params,
    LogManager* log_manager);
[[nodiscard]] DenseSet<FormParsingPermission>
GetFormParsingPermissionsForTest(  // IN-TEST
    const FormStructure& form,
    bool ignore_small_forms,
    FormParsingPermissionsParams params,
    LogManager* log_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_QUALIFIERS_H_
