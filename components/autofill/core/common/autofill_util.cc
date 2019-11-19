// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

using features::kAutofillKeyboardAccessory;
using mojom::FocusedFieldType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

const char kAutofillKeyboardAccessoryAnimationDurationKey[] =
    "animation_duration_millis";
const char kAutofillKeyboardAccessoryLimitLabelWidthKey[] =
    "should_limit_label_width";
const char kAutofillKeyboardAccessoryHintKey[] =
    "is_hint_shown_before_suggestion";

namespace {

const char kSplitCharacters[] = " .,-_@";

template <typename Char>
struct Compare : base::CaseInsensitiveCompareASCII<Char> {
  explicit Compare(bool case_sensitive) : case_sensitive_(case_sensitive) {}
  bool operator()(Char x, Char y) const {
    return case_sensitive_ ? (x == y)
                           : base::CaseInsensitiveCompareASCII<Char>::
                             operator()(x, y);
  }

 private:
  bool case_sensitive_;
};

}  // namespace

bool IsFeatureSubstringMatchEnabled() {
  return base::FeatureList::IsEnabled(features::kAutofillTokenPrefixMatching);
}

bool IsShowAutofillSignaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowAutofillSignatures);
}

bool IsKeyboardAccessoryEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(kAutofillKeyboardAccessory);
#else  // !defined(OS_ANDROID)
  return false;
#endif
}

bool IsTouchToFillEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(features::kAutofillTouchToFill);
#else  // !defined(OS_ANDROID)
  return false;
#endif
}

unsigned int GetKeyboardAccessoryAnimationDuration() {
#if defined(OS_ANDROID)
  return base::GetFieldTrialParamByFeatureAsInt(
      kAutofillKeyboardAccessory,
      kAutofillKeyboardAccessoryAnimationDurationKey, 0);
#else  // !defined(OS_ANDROID)
  NOTREACHED();
  return 0;
#endif
}

bool ShouldLimitKeyboardAccessorySuggestionLabelWidth() {
#if defined(OS_ANDROID)
  return base::GetFieldTrialParamByFeatureAsBool(
      kAutofillKeyboardAccessory, kAutofillKeyboardAccessoryLimitLabelWidthKey,
      false);
#else  // !defined(OS_ANDROID)
  NOTREACHED();
  return false;
#endif
}

bool IsHintEnabledInKeyboardAccessory() {
#if defined(OS_ANDROID)
  return base::GetFieldTrialParamByFeatureAsBool(
      kAutofillKeyboardAccessory, kAutofillKeyboardAccessoryHintKey, false);
#else  // !defined(OS_ANDROID)
  NOTREACHED();
  return false;
#endif
}

bool FieldIsSuggestionSubstringStartingOnTokenBoundary(
    const base::string16& suggestion,
    const base::string16& field_contents,
    bool case_sensitive) {
  if (!IsFeatureSubstringMatchEnabled()) {
    return base::StartsWith(suggestion, field_contents,
                            case_sensitive
                                ? base::CompareCase::SENSITIVE
                                : base::CompareCase::INSENSITIVE_ASCII);
  }

  return suggestion.length() >= field_contents.length() &&
         GetTextSelectionStart(suggestion, field_contents, case_sensitive) !=
             base::string16::npos;
}

bool IsPrefixOfEmailEndingWithAtSign(const base::string16& full_string,
                                     const base::string16& prefix) {
  return base::StartsWith(full_string, prefix + base::UTF8ToUTF16("@"),
                          base::CompareCase::SENSITIVE);
}

size_t GetTextSelectionStart(const base::string16& suggestion,
                             const base::string16& field_contents,
                             bool case_sensitive) {
  const base::string16 kSplitChars = base::ASCIIToUTF16(kSplitCharacters);

  // Loop until we find either the |field_contents| is a prefix of |suggestion|
  // or character right before the match is one of the splitting characters.
  for (base::string16::const_iterator it = suggestion.begin();
       (it = std::search(
            it, suggestion.end(), field_contents.begin(), field_contents.end(),
            Compare<base::string16::value_type>(case_sensitive))) !=
           suggestion.end();
       ++it) {
    if (it == suggestion.begin() ||
        kSplitChars.find(*(it - 1)) != std::string::npos) {
      // Returns the character position right after the |field_contents| within
      // |suggestion| text as a caret position for text selection.
      return it - suggestion.begin() + field_contents.size();
    }
  }

  // Unable to find the |field_contents| in |suggestion| text.
  return base::string16::npos;
}

bool IsDesktopPlatform() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return false;
#else
  return true;
#endif
}

bool ShouldSkipField(const FormFieldData& field) {
  return IsCheckable(field.check_status);
}

bool IsCheckable(const FormFieldData::CheckStatus& check_status) {
  return check_status != FormFieldData::CheckStatus::kNotCheckable;
}

bool IsChecked(const FormFieldData::CheckStatus& check_status) {
  return check_status == FormFieldData::CheckStatus::kChecked;
}

void SetCheckStatus(FormFieldData* form_field_data,
                    bool isCheckable,
                    bool isChecked) {
  if (isChecked) {
    form_field_data->check_status = FormFieldData::CheckStatus::kChecked;
  } else {
    if (isCheckable) {
      form_field_data->check_status =
          FormFieldData::CheckStatus::kCheckableButUnchecked;
    } else {
      form_field_data->check_status = FormFieldData::CheckStatus::kNotCheckable;
    }
  }
}

std::vector<std::string> LowercaseAndTokenizeAttributeString(
    const std::string& attribute) {
  return base::SplitString(base::ToLowerASCII(attribute),
                           base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

bool SanitizedFieldIsEmpty(const base::string16& value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  static base::string16 formatting =
      (base::ASCIIToUTF16("-_()/") +
       base::char16(base::i18n::kRightToLeftMark) +
       base::char16(base::i18n::kLeftToRightMark))
          .append(base::kWhitespaceUTF16);

  return (value.find_first_not_of(formatting) == base::StringPiece::npos);
}

bool ShouldAutoselectFirstSuggestionOnArrowDown() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  return true;
#else
  return false;
#endif
}

bool IsFillable(FocusedFieldType focused_field_type) {
  return focused_field_type == FocusedFieldType::kFillableTextArea ||
         focused_field_type == FocusedFieldType::kFillableSearchField ||
         focused_field_type == FocusedFieldType::kFillableNonSearchField ||
         focused_field_type == FocusedFieldType::kFillableUsernameField ||
         focused_field_type == FocusedFieldType::kFillablePasswordField;
}

SubmissionIndicatorEvent ToSubmissionIndicatorEvent(SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return SubmissionIndicatorEvent::NONE;
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;
    case SubmissionSource::XHR_SUCCEEDED:
      return SubmissionIndicatorEvent::XHR_SUCCEEDED;
    case SubmissionSource::FRAME_DETACHED:
      return SubmissionIndicatorEvent::FRAME_DETACHED;
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR;
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  }

  NOTREACHED();
  return SubmissionIndicatorEvent::NONE;
}

}  // namespace autofill
