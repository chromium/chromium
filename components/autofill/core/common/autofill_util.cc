// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

using mojom::FocusedFieldType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

bool IsShowAutofillSignaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowAutofillSignatures);
}

bool IsPrefixOfEmailEndingWithAtSign(std::u16string_view full_string,
                                     std::u16string_view prefix) {
  return full_string.size() > prefix.size() &&
         full_string.starts_with(prefix) && full_string[prefix.size()] == u'@';
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
    form_field_data->set_check_status(FormFieldData::CheckStatus::kChecked);
  } else {
    if (isCheckable) {
      form_field_data->set_check_status(
          FormFieldData::CheckStatus::kCheckableButUnchecked);
    } else {
      form_field_data->set_check_status(
          FormFieldData::CheckStatus::kNotCheckable);
    }
  }
}

std::vector<std::string> LowercaseAndTokenizeAttributeString(
    std::string_view attribute) {
  return base::SplitString(base::ToLowerASCII(attribute),
                           base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::u16string RemoveWhitespace(std::u16string_view value) {
  std::u16string stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

std::u16string SanitizeCreditCardFieldValue(std::u16string_view value) {
  std::u16string sanitized;
  // We remove whitespace as well as some invisible unicode characters.
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized);
  base::TrimString(sanitized,
                   std::u16string({base::i18n::kRightToLeftMark,
                                   base::i18n::kLeftToRightMark}),
                   &sanitized);
  // Some sites have ____-____-____-____ in their credit card number fields, for
  // example.
  base::RemoveChars(sanitized, u"-_", &sanitized);
  return sanitized;
}

bool SanitizedFieldIsEmpty(std::u16string_view value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  static const base::NoDestructor<std::u16string> formatting(
      base::StrCat({u"-_()/",
                    {&base::i18n::kRightToLeftMark, 1},
                    {&base::i18n::kLeftToRightMark, 1},
                    base::kWhitespaceUTF16}));

  return base::ContainsOnlyChars(value, *formatting);
}

bool IsFillable(FocusedFieldType focused_field_type) {
  switch (focused_field_type) {
    case FocusedFieldType::kFillableTextArea:
    case FocusedFieldType::kFillableSearchField:
    case FocusedFieldType::kFillableNonSearchField:
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillablePasswordField:
    case FocusedFieldType::kFillableWebauthnTaggedField:
      return true;
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return false;
  }
  NOTREACHED();
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
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
    case SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      return SubmissionIndicatorEvent::DOM_MUTATION_AFTER_AUTOFILL;
  }

  NOTREACHED_IN_MIGRATION();
  return SubmissionIndicatorEvent::NONE;
}

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

bool IsAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return IsAddressAutofillManuallyTriggered(trigger_source) ||
         IsPaymentsAutofillManuallyTriggered(trigger_source) ||
         IsPasswordsAutofillManuallyTriggered(trigger_source);
}

bool IsAddressAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackAddress;
}

bool IsPaymentsAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackPayments;
}

bool IsPasswordsAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackPasswords;
}

bool IsPlusAddressesManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses;
}

bool IsAddressFieldSwappingEnabled() {
#if BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(features::kAutofillAddressFieldSwapping);
#else
  return true;
#endif
}

}  // namespace autofill
