// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace base {
struct Feature;
}

namespace autofill {

extern const char kAutofillKeyboardAccessoryAnimationDurationKey[];
extern const char kAutofillKeyboardAccessoryLimitLabelWidthKey[];
extern const char kAutofillKeyboardAccessoryHintKey[];

// The length of the GUIDs used for local autofill data. It is different than
// the length used for server autofill data.
constexpr int kLocalGuidSize = 36;

// Returns true when command line switch |kEnableSuggestionsWithSubstringMatch|
// is on.
bool IsFeatureSubstringMatchEnabled();

// Returns true if showing autofill signature as HTML attributes is enabled.
bool IsShowAutofillSignaturesEnabled();

// Returns true when keyboard accessory is enabled.
bool IsKeyboardAccessoryEnabled();

// Returns whether the Touch To Fill feature is enabled.
bool IsTouchToFillEnabled();

// Returns animation duration for keyboard accessory. If 0, we do not animate.
unsigned int GetKeyboardAccessoryAnimationDuration();

// Returns true if we must limit width of keyboard accessory suggestion label to
// half of device's pixel width.
bool ShouldLimitKeyboardAccessorySuggestionLabelWidth();

// Returns true if we show a hint in the keyboard accessory suggestions to call
// attention to the availability of autofill suggestions.
bool IsHintEnabledInKeyboardAccessory();

// A token is a sequences of contiguous characters separated by any of the
// characters that are part of delimiter set {' ', '.', ',', '-', '_', '@'}.

// Returns true if the |field_contents| is a substring of the |suggestion|
// starting at token boundaries. |field_contents| can span multiple |suggestion|
// tokens.
bool FieldIsSuggestionSubstringStartingOnTokenBoundary(
    const base::string16& suggestion,
    const base::string16& field_contents,
    bool case_sensitive);

// Currently, a token for the purposes of this method is defined as {'@'}.
// Returns true if the |full_string| has a |prefix| as a prefix and the prefix
// ends on a token.
bool IsPrefixOfEmailEndingWithAtSign(const base::string16& full_string,
                                     const base::string16& prefix);

// Finds the first occurrence of a searched substring |field_contents| within
// the string |suggestion| starting at token boundaries and returns the index to
// the end of the located substring, or base::string16::npos if the substring is
// not found. "preview-on-hover" feature is one such use case where the
// substring |field_contents| may not be found within the string |suggestion|.
size_t GetTextSelectionStart(const base::string16& suggestion,
                             const base::string16& field_contents,
                             bool case_sensitive);

// Returns true if running on a desktop platform. Any platform that is not
// Android or iOS is considered desktop.
bool IsDesktopPlatform();

bool ShouldSkipField(const FormFieldData& field);

bool IsCheckable(const FormFieldData::CheckStatus& check_status);
bool IsChecked(const FormFieldData::CheckStatus& check_status);
void SetCheckStatus(FormFieldData* form_field_data,
                    bool isCheckable,
                    bool isChecked);

// Lowercases and tokenizes a given |attribute| string.
// Considers any ASCII whitespace character as a possible separator.
// Also ignores empty tokens, resulting in a collapsing of whitespace.
std::vector<std::string> LowercaseAndTokenizeAttributeString(
    const std::string& attribute);

// Returns true if and only if the field value has no character except the
// formatting characters. This means that the field value is a formatting string
// entered by the website and not a real value entered by the user.
bool SanitizedFieldIsEmpty(const base::string16& value);

// Returns true if the first suggestion should be autoselected when the autofill
// dropdown is shown due to an arrow down event. Enabled on desktop only.
bool ShouldAutoselectFirstSuggestionOnArrowDown();

// Returns true if focused_field_type corresponds to a fillable field.
bool IsFillable(mojom::FocusedFieldType focused_field_type);

mojom::SubmissionIndicatorEvent ToSubmissionIndicatorEvent(
    mojom::SubmissionSource source);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_
