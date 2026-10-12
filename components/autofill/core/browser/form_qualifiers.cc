// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_qualifiers.h"

#include <stddef.h>

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

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
DenseSet<FormParsingPermission> GetFormParsingPermissions(
    const T& form,
    bool ignore_small_forms,
    FormParsingPermissionsParams params,
    LogManager* log_manager) {
  // Exclude URLs not on the web via HTTP(S).
  if (!HasAllowedScheme(url(form))) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotAllowedScheme << form;
    return {};
  }

  if (fields(form).size() < params.min_required_fields &&
      (fields(form).size() <
           params.required_fields_for_forms_with_only_password_fields ||
       !std::ranges::all_of(fields(form), is_password_field)) &&
      std::ranges::none_of(fields(form), has_autocomplete)) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotEnoughFields
                        << fields(form).size() << form;
    return {};
  }

  // Rule out search forms.
  if (MatchesRegex<kUrlSearchActionRe>(
          base::UTF8ToUTF16(action(form).path()))) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingUrlMatchesSearchRegex
                        << form;
    return {};
  }

  bool has_text_field =
      std::ranges::any_of(fields(form), std::not_fn(is_select_element));
  if (!has_text_field) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingFormHasNoTextfield << form;
    return {};
  }

  DenseSet<FormParsingPermission> permissions;
  if (!ignore_small_forms ||
      fields(form).size() >= kMinRequiredFieldsForHeuristics) {
    permissions.insert(FormParsingPermission::kHeuristics);
  }
  if (fields(form).size() >= 1) {
    permissions.insert(FormParsingPermission::kSingleFieldHeuristics);
  }
  if (fields(form).size() >= kMinRequiredFieldsForQuery ||
      std::ranges::any_of(fields(form), is_password_field)) {
    permissions.insert(FormParsingPermission::kServerQuery);
  }
  if (fields(form).size() >= kMinRequiredFieldsForUpload) {
    permissions.insert(FormParsingPermission::kServerUpload);
  }
  return permissions;
}

bool ShouldUploadUkm(const FormStructure& form, bool require_classified_field) {
  if (GetFormParsingPermissions(form, /*ignore_small_forms=*/true, {}, nullptr)
          .empty()) {
    return false;
  }

  auto is_focusable_text_field =
      [](const std::unique_ptr<AutofillField>& field) {
        return field->IsTextInputElement() && field->is_focusable();
      };

  // Return true if the field is a visible text input field which has predicted
  // types from heuristics or the server.
  auto is_focusable_predicted_text_field =
      [](const std::unique_ptr<AutofillField>& field) {
        return field->IsTextInputElement() && field->is_focusable() &&
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

DenseSet<FormParsingPermission> GetFormParsingPermissions(
    const FormData& form,
    bool ignore_small_forms,
    LogManager* log_manager) {
  return internal::GetFormParsingPermissions(form, ignore_small_forms, {},
                                             log_manager);
}

DenseSet<FormParsingPermission> GetFormParsingPermissions(
    const FormStructure& form,
    bool ignore_small_forms) {
  return internal::GetFormParsingPermissions(form, ignore_small_forms, {},
                                             nullptr);
}

bool ShouldUploadUkm(const FormStructure& form, bool require_classified_field) {
  return internal::ShouldUploadUkm(form, require_classified_field);
}

bool IsAutofillable(const FormStructure& form) {
  static constexpr size_t kMinRequiredFields =
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
  return internal::AtLeastNumFieldsSatisfy(form, kMinRequiredFields,
                                           &AutofillField::IsFieldFillable);
}

DenseSet<FormParsingPermission> GetFormParsingPermissionsForTest(  // IN-TEST
    const FormData& form,
    bool ignore_small_forms,
    FormParsingPermissionsParams params,
    LogManager* log_manager) {
  return internal::GetFormParsingPermissions(form, ignore_small_forms, params,
                                             log_manager);
}

DenseSet<FormParsingPermission> GetFormParsingPermissionsForTest(  // IN-TEST
    const FormStructure& form,
    bool ignore_small_forms,
    FormParsingPermissionsParams params,
    LogManager* log_manager) {
  return internal::GetFormParsingPermissions(form, ignore_small_forms, params,
                                             log_manager);
}

}  // namespace autofill
