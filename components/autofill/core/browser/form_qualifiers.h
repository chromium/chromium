// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_QUALIFIERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_QUALIFIERS_H_

#include <algorithm>
#include <cstddef>

#include "components/autofill/core/common/autofill_constants.h"

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

// Returns true if this form matches the structural requirements for Autofill.
[[nodiscard]] bool ShouldBeParsed(const FormData& form,
                                  LogManager* log_manager);
[[nodiscard]] bool ShouldBeParsed(const FormStructure& form,
                                  LogManager* log_manager);

// Returns true if heuristic autofill type detection should be attempted for
// this form.
[[nodiscard]] bool ShouldRunHeuristics(const FormData& form);
[[nodiscard]] bool ShouldRunHeuristics(const FormStructure& form);

// Returns true if autofill's heuristic field type detection should be attempted
// for this form given that `kMinRequiredFieldsForHeuristics` is not met.
[[nodiscard]] bool ShouldRunHeuristicsForSingleFields(const FormData& form);
[[nodiscard]] bool ShouldRunHeuristicsForSingleFields(
    const FormStructure& form);

// Returns true if we should query the crowd-sourcing server to determine this
// form's field types. If the form includes author-specified types, this will
// return false unless there are password fields in the form. If there are no
// password fields the assumption is that the author has expressed their intent
// and crowdsourced data should not be used to override this. Password fields
// are different because there is no way to specify password generation
// directly.
[[nodiscard]] bool ShouldBeQueried(const FormStructure& form);

// Returns true if we should upload Autofill votes for this form to the
// crowd-sourcing server. It is not applied for Password Manager votes.
[[nodiscard]] bool ShouldBeUploaded(const FormStructure& form);

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
struct ShouldBeParsedParams {
  size_t min_required_fields =
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
  size_t required_fields_for_forms_with_only_password_fields =
      kRequiredFieldsForFormsWithOnlyPasswordFields;
};

// Variants of ShouldBeParsed() that additionally take ShouldBeParsedParams.
[[nodiscard]] bool ShouldBeParsedForTest(const FormData& form,  // IN-TEST
                                         ShouldBeParsedParams params,
                                         LogManager* log_manager);
[[nodiscard]] bool ShouldBeParsedForTest(const FormStructure& form,  // IN-TEST
                                         ShouldBeParsedParams params,
                                         LogManager* log_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_QUALIFIERS_H_
