// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

using mojom::FocusedFieldType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

constexpr base::StringPiece16 kSplitCharacters = u" .,-_@";

template <typename Char>
struct Compare : base::CaseInsensitiveCompareASCII<Char> {
  explicit Compare(bool case_sensitive) : case_sensitive_(case_sensitive) {}
  bool operator()(Char x, Char y) const {
    return case_sensitive_
               ? (x == y)
               : base::CaseInsensitiveCompareASCII<Char>::operator()(x, y);
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
#if BUILDFLAG(IS_ANDROID)
  return true;
#else  // !BUILDFLAG(IS_ANDROID)
  return false;
#endif
}

bool FieldIsSuggestionSubstringStartingOnTokenBoundary(
    const std::u16string& suggestion,
    const std::u16string& field_contents,
    bool case_sensitive) {
  if (!IsFeatureSubstringMatchEnabled()) {
    return base::StartsWith(suggestion, field_contents,
                            case_sensitive
                                ? base::CompareCase::SENSITIVE
                                : base::CompareCase::INSENSITIVE_ASCII);
  }

  return suggestion.length() >= field_contents.length() &&
         GetTextSelectionStart(suggestion, field_contents, case_sensitive) !=
             std::u16string::npos;
}

bool IsPrefixOfEmailEndingWithAtSign(const std::u16string& full_string,
                                     const std::u16string& prefix) {
  return base::StartsWith(full_string, prefix + u"@",
                          base::CompareCase::SENSITIVE);
}

size_t GetTextSelectionStart(const std::u16string& suggestion,
                             const std::u16string& field_contents,
                             bool case_sensitive) {
  // Loop until we find either the |field_contents| is a prefix of |suggestion|
  // or character right before the match is one of the splitting characters.
  for (std::u16string::const_iterator it = suggestion.begin();
       (it = std::search(
            it, suggestion.end(), field_contents.begin(), field_contents.end(),
            Compare<std::u16string::value_type>(case_sensitive))) !=
       suggestion.end();
       ++it) {
    if (it == suggestion.begin() ||
        kSplitCharacters.find(it[-1]) != std::string::npos) {
      // Returns the character position right after the |field_contents| within
      // |suggestion| text as a caret position for text selection.
      return it - suggestion.begin() + field_contents.size();
    }
  }

  // Unable to find the |field_contents| in |suggestion| text.
  return std::u16string::npos;
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
    base::StringPiece attribute) {
  return base::SplitString(base::ToLowerASCII(attribute),
                           base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

bool SanitizedFieldIsEmpty(const std::u16string& value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  static std::u16string formatting =
      base::StrCat({u"-_()/",
                    {&base::i18n::kRightToLeftMark, 1},
                    {&base::i18n::kLeftToRightMark, 1},
                    base::kWhitespaceUTF16});

  return (value.find_first_not_of(formatting) == base::StringPiece::npos);
}

bool ShouldAutoselectFirstSuggestionOnArrowDown() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
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
  NOTREACHED_NORETURN();
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

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace autofill
