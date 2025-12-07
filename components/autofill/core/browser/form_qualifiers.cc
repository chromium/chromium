// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_qualifiers.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace internal {

namespace {

// Returns true if the scheme given by |url| is one for which autofill is
// allowed to activate. By default this only returns true for HTTP and HTTPS.
bool HasAllowedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

template <typename T>
concept IsForm = std::same_as<T, FormStructure> || std::same_as<T, FormData>;

const GURL& url(const FormData& form) {
  return form.url();
}
const GURL& url(const FormStructure& form) {
  return form.source_url();
}

const GURL& action(const FormData& form) {
  return form.action();
}
const GURL& action(const FormStructure& form) {
  return form.target_url();
}

const std::vector<FormFieldData>& fields(const FormData& form) {
  return form.fields();
}
const std::vector<std::unique_ptr<AutofillField>>& fields(
    const FormStructure& form) {
  return form.fields();
}

// A field is active if it contributes to the form signature and it is are
// included in queries to the Autofill server.
auto is_active = absl::Overload{
    [](const FormFieldData& field) {
      return !IsCheckable(field.check_status());
    },
    [](const std::unique_ptr<AutofillField>& field) {
      return !IsCheckable(field->check_status());
    },
};

auto has_autocomplete = absl::Overload{
    [](const FormFieldData& field) {
      return field.parsed_autocomplete().has_value();
    },
    [](const std::unique_ptr<AutofillField>& field) {
      return field->parsed_autocomplete().has_value();
    },
};

auto is_password_field = absl::Overload{
    [](const FormFieldData& field) {
      return field.form_control_type() == FormControlType::kInputPassword;
    },
    [](const std::unique_ptr<AutofillField>& field) {
      return field->form_control_type() == FormControlType::kInputPassword;
    },
};

auto is_select_element = absl::Overload{
    [](const FormFieldData& field) { return field.IsSelectElement(); },
    [](const std::unique_ptr<AutofillField>& field) {
      return field->IsSelectElement();
    },
};

// Returns true if at least `num` fields satisfy `p`.
// This is useful if `num` is significantly smaller than `fields.size()` because
// it may avoid iterating over all of `fields`. It's equivalent to
// `std::range::count_if(fields, [](auto& f) { p(*f); }) >= num`.
template <typename T, typename Predicate>
  requires IsForm<T>
bool AtLeastNumFieldsSatisfy(const T& form, size_t num, Predicate p) {
  for (auto& field : fields(form)) {
    if (num == 0) {
      break;
    }
    if constexpr (std::same_as<T, FormStructure>) {
      if (std::invoke(p, *field)) {
        --num;
      }
    } else {
      if (std::invoke(p, field)) {
        --num;
      }
    }
  }
  return num == 0;
}

template <typename T>
  requires IsForm<T>
bool ShouldBeParsed(const T& form,
                    ShouldBeParsedParams params,
                    LogManager* log_manager) {
  // Exclude URLs not on the web via HTTP(S).
  if (!HasAllowedScheme(url(form))) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotAllowedScheme << form;
    return false;
  }

  if (!AtLeastNumFieldsSatisfy(form, params.min_required_fields, is_active) &&
      (!AtLeastNumFieldsSatisfy(
           form, params.required_fields_for_forms_with_only_password_fields,
           is_active) ||
       !std::ranges::all_of(fields(form), is_password_field)) &&
      std::ranges::none_of(fields(form), has_autocomplete)) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotEnoughFields
                        << std::ranges::count_if(fields(form), is_active)
                        << form;
    return false;
  }

  // Rule out search forms.
  if (MatchesRegex<kUrlSearchActionRe>(
          base::UTF8ToUTF16(action(form).path()))) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingUrlMatchesSearchRegex
                        << form;
    return false;
  }

  bool has_text_field =
      std::ranges::any_of(fields(form), std::not_fn(is_select_element));
  if (!has_text_field) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingFormHasNoTextfield << form;
  }
  return has_text_field;
}

template <typename T>
  requires IsForm<T>
bool ShouldRunHeuristics(const T& form) {
  return AtLeastNumFieldsSatisfy(form, kMinRequiredFieldsForHeuristics,
                                 is_active) &&
         HasAllowedScheme(url(form));
}

template <typename T>
  requires IsForm<T>
bool ShouldRunHeuristicsForSingleFields(const T& form) {
  return AtLeastNumFieldsSatisfy(form, 1, is_active) &&
         HasAllowedScheme(url(form));
}

bool ShouldBeQueried(const FormStructure& form) {
  return (AtLeastNumFieldsSatisfy(form, kMinRequiredFieldsForQuery,
                                  is_active) ||
          std::ranges::any_of(fields(form), is_password_field)) &&
         ShouldBeParsed(form, {}, nullptr);
}

bool ShouldBeUploaded(const FormStructure& form) {
  return AtLeastNumFieldsSatisfy(form, kMinRequiredFieldsForUpload,
                                 is_active) &&
         ShouldBeParsed(form, {}, nullptr);
}

bool ShouldUploadUkm(const FormStructure& form, bool require_classified_field) {
  if (!ShouldBeParsed(form, {}, nullptr)) {
    return false;
  }

  auto is_focusable_text_field =
      [](const std::unique_ptr<AutofillField>& field) {
        return field->IsTextInputElement() && field->IsFocusable();
      };

  // Return true if the field is a visible text input field which has predicted
  // types from heuristics or the server.
  auto is_focusable_predicted_text_field =
      [](const std::unique_ptr<AutofillField>& field) {
        return field->IsTextInputElement() && field->IsFocusable() &&
               ((field->server_type() != NO_SERVER_DATA &&
                 field->server_type() != UNKNOWN_TYPE) ||
                field->heuristic_type() != UNKNOWN_TYPE ||
                field->html_type() != HtmlFieldType::kUnspecified);
      };

  size_t num_text_fields = std::ranges::count_if(
      fields(form), require_classified_field ? is_focusable_predicted_text_field
                                             : is_focusable_text_field);
  if (num_text_fields == 0) {
    return false;
  }

  // If the form contains a single text field and this contains the string
  // "search" in its name/id/placeholder, the function return false and the form
  // is not recorded into UKM. The form is considered a search box.
  if (num_text_fields == 1) {
    auto it = std::ranges::find_if(fields(form),
                                   require_classified_field
                                       ? is_focusable_predicted_text_field
                                       : is_focusable_text_field);
    if (base::ToLowerASCII((*it)->placeholder()).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->name()).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->label()).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->aria_label()).find(u"search") !=
            std::string::npos) {
      return false;
    }
  }

  return true;
}

}  // namespace

}  // namespace internal

bool ShouldBeParsed(const FormData& form, LogManager* log_manager) {
  return internal::ShouldBeParsed(form, {}, log_manager);
}

bool ShouldBeParsed(const FormStructure& form, LogManager* log_manager) {
  return internal::ShouldBeParsed(form, {}, log_manager);
}

bool ShouldRunHeuristics(const FormData& form) {
  return internal::ShouldRunHeuristics(form);
}

bool ShouldRunHeuristics(const FormStructure& form) {
  return internal::ShouldRunHeuristics(form);
}

bool ShouldRunHeuristicsForSingleFields(const FormData& form) {
  return internal::ShouldRunHeuristicsForSingleFields(form);
}

bool ShouldRunHeuristicsForSingleFields(const FormStructure& form) {
  return internal::ShouldRunHeuristicsForSingleFields(form);
}

bool ShouldBeQueried(const FormStructure& form) {
  return internal::ShouldBeQueried(form);
}

bool ShouldBeUploaded(const FormStructure& form) {
  return internal::ShouldBeUploaded(form);
}

bool ShouldUploadUkm(const FormStructure& form, bool require_classified_field) {
  return internal::ShouldUploadUkm(form, require_classified_field);
}

bool IsAutofillable(const FormStructure& form) {
  static constexpr size_t kMinRequiredFields =
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
  return internal::AtLeastNumFieldsSatisfy(form, kMinRequiredFields,
                                           &AutofillField::IsFieldFillable) &&
         internal::ShouldBeParsed(form, {}, nullptr);
}

bool ShouldBeParsedForTest(const FormData& form,  // IN-TEST
                           ShouldBeParsedParams params,
                           LogManager* log_manager) {
  return internal::ShouldBeParsed(form, params, log_manager);
}

bool ShouldBeParsedForTest(const FormStructure& form,  // IN-TEST
                           ShouldBeParsedParams params,
                           LogManager* log_manager) {
  return internal::ShouldBeParsed(form, params, log_manager);
}

}  // namespace autofill
