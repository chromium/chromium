// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <stddef.h>

#include <algorithm>
#include <numeric>
#include <vector>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

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

bool IsShowAutofillSignaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowAutofillSignatures);
}

bool IsPrefixOfEmailEndingWithAtSign(const std::u16string& full_string,
                                     const std::u16string& prefix) {
  return full_string.starts_with(prefix + u"@");
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
    std::string_view attribute) {
  return base::SplitString(base::ToLowerASCII(attribute),
                           base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::u16string RemoveWhitespace(const std::u16string& value) {
  std::u16string stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

bool SanitizedFieldIsEmpty(const std::u16string& value) {
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
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
    case SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      return SubmissionIndicatorEvent::DOM_MUTATION_AFTER_AUTOFILL;
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

void DumpWithoutCrashingForDuplicateIds(const FormData& form,
                                        const base::Location& location) {
  SCOPED_CRASH_KEY_STRING64("AFCrash", "URL", form.url.possibly_invalid_spec());
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "NumUniqueFieldIds",
                          base::MakeFlatSet<FieldGlobalId>(
                              form.fields, {}, &FormFieldData::global_id)
                              .size());
  size_t num_field_ids2 =
      base::MakeFlatSet<std::pair<FieldGlobalId, FormRendererId>>(
          form.fields, {},
          [](const FormFieldData& field) {
            return std::make_pair(field.global_id(), field.host_form_id);
          })
          .size();
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "NumUniqueFieldIds2", num_field_ids2);
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "NumFields", form.fields.size());
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "NumFrames",
                          base::MakeFlatSet<LocalFrameToken>(
                              form.fields, {}, &FormFieldData::host_frame)
                              .size());

  std::map<FieldGlobalId, std::vector<const FormFieldData*>> id_to_fields;
  for (const FormFieldData& field : form.fields) {
    id_to_fields[field.global_id()].push_back(&field);
  }
  size_t num_null_id = 0;
  DenseSet<FormControlType> form_control_types;
  std::set<LocalFrameToken> frames_with_duplicates;
  bool some_duplicate_from_flattened_form = false;
  bool some_duplicate_from_unflattened_form = false;
  bool some_field_from_flattened_form = false;
  bool some_field_from_unflattened_form = false;
  for (auto& [field_id, duplicate_fields] : id_to_fields) {
    auto is_flattened = [&form](const FormFieldData* field) {
      return field->renderer_form_id() != form.global_id();
    };
    some_field_from_unflattened_form |=
        base::ranges::any_of(duplicate_fields, is_flattened);
    some_field_from_flattened_form |=
        base::ranges::any_of(duplicate_fields, std::not_fn(is_flattened));
    if (duplicate_fields.size() > 1) {
      some_duplicate_from_unflattened_form |=
          base::ranges::any_of(duplicate_fields, is_flattened);
      some_duplicate_from_flattened_form |=
          base::ranges::any_of(duplicate_fields, std::not_fn(is_flattened));
      frames_with_duplicates.insert(field_id.frame_token);
      if (field_id.renderer_id.is_null()) {
        num_null_id += duplicate_fields.size();
      }
      for (const FormFieldData* field : duplicate_fields) {
        form_control_types.insert(field->form_control_type);
      }
    }
  }
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "NumNullDuplicateIDs", num_null_id);
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "NumFramesWithDuplicates",
                          frames_with_duplicates.size());
  static_assert(form_control_types.data().size() == 1);
  SCOPED_CRASH_KEY_NUMBER("AFCrash", "FormControlTypes",
                          form_control_types.data()[0]);
  SCOPED_CRASH_KEY_BOOL("AFCrash", "SomeFieldFromFlattenedForm",
                        some_field_from_flattened_form);
  SCOPED_CRASH_KEY_BOOL("AFCrash", "SomeFieldFromUnflattenedForm",
                        some_field_from_unflattened_form);
  SCOPED_CRASH_KEY_BOOL("AFCrash", "SomeDupeFromFlattenedForm",
                        some_duplicate_from_flattened_form);
  SCOPED_CRASH_KEY_BOOL("AFCrash", "SomeDupeFromUnflattenedForm",
                        some_duplicate_from_unflattened_form);
  base::debug::DumpWithoutCrashing(location);
}

}  // namespace autofill
