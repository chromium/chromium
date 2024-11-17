// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/sensitive_content_manager.h"

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/password_manager/content/browser/password_form_classification_util.h"
#include "components/sensitive_content/features.h"
#include "components/sensitive_content/sensitive_content_client.h"
#include "content/public/browser/web_contents.h"

namespace sensitive_content {

namespace {

using LifecycleState = autofill::AutofillDriver::LifecycleState;
using autofill::AutofillField;
using autofill::AutofillManager;
using autofill::FieldType;
using autofill::FieldTypeGroup;
using autofill::FormGlobalId;

bool IsSensitiveAutofillType(FieldType type) {
  static constexpr autofill::FieldTypeSet kNonSensitivePasswordTypes = {
      FieldType::NOT_ACCOUNT_CREATION_PASSWORD, FieldType::NOT_NEW_PASSWORD,
      FieldType::NOT_PASSWORD, FieldType::NOT_USERNAME};
  FieldTypeGroup field_type_group = autofill::GroupTypeOfFieldType(type);
  // Return true if the field is a credit card or password form field.
  // A field is a password form field if it's part of
  // `FieldTypeGroup::kPasswordField`, but it's not prefixed with "NOT_".
  return field_type_group == FieldTypeGroup::kCreditCard ||
         field_type_group == FieldTypeGroup::kStandaloneCvcField ||
         (field_type_group == FieldTypeGroup::kPasswordField &&
          !kNonSensitivePasswordTypes.contains(type));
}

}  // namespace

SensitiveContentManager::SensitiveContentManager(
    content::WebContents* web_contents,
    SensitiveContentClient* client)
    : client_(CHECK_DEREF(client)) {
  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
}

SensitiveContentManager::~SensitiveContentManager() = default;

bool SensitiveContentManager::UpdateContentSensitivity() {
  const bool content_is_sensitive = !sensitive_fields_.empty();
  // Prevent unnecessary calls to the client.
  if (last_content_was_sensitive_ != content_is_sensitive) {
    client_->SetContentSensitivity(!sensitive_fields_.empty());
    last_content_was_sensitive_ = content_is_sensitive;

    base::UmaHistogramBoolean(
        base::StrCat({client_->GetHistogramPrefix(), "SensitivityChanged"}),
        content_is_sensitive);

    if (content_is_sensitive) {
      content_became_sensitive_timestamp_ = base::TimeTicks::Now();
    } else if (content_became_sensitive_timestamp_.has_value()) {
      base::UmaHistogramLongTimes(
          base::StrCat({client_->GetHistogramPrefix(), "SensitiveTime"}),
          base::TimeTicks::Now() - content_became_sensitive_timestamp_.value());
      content_became_sensitive_timestamp_.reset();
    }
    return true;
  }
  return false;
}

void SensitiveContentManager::OnFieldTypesDetermined(AutofillManager& manager,
                                                     FormGlobalId form_id,
                                                     FieldTypeSource) {
  if (const autofill::FormStructure* form =
          manager.FindCachedFormById(form_id)) {
    for (const std::unique_ptr<AutofillField>& field : form->fields()) {
      const bool field_is_sensitive =
          IsSensitiveAutofillType(field->Type().GetStorableType());
      // The feature param check is done first because reparsing by password
      // manager (calling `ClassifyAsPasswordForm`) can take long. Moreover,
      // this feature param exists only to check whether reparsing has a
      // negative performance impact or not. Otherwise, it is known that
      // reparsing by password manager is more accurate for password forms.
      const bool field_is_sensitive_after_password_manager_reparsing =
          features::kSensitiveContentUsePwmHeuristicsParam.Get() &&
          password_manager::ClassifyAsPasswordForm(manager, form_id,
                                                   field->global_id())
                  .type !=
              autofill::PasswordFormClassification::Type::kNoPasswordForm;

      if (field_is_sensitive ||
          field_is_sensitive_after_password_manager_reparsing) {
        sensitive_fields_.insert(field->global_id());
      } else {
        sensitive_fields_.erase(field->global_id());
      }
    }

    if (UpdateContentSensitivity() && last_content_was_sensitive_) {
      const auto& element = latency_until_sensitive_timer_.find(form_id);
      if (element != latency_until_sensitive_timer_.end()) {
        base::UmaHistogramLongTimes(base::StrCat({client_->GetHistogramPrefix(),
                                                  "LatencyUntilSensitive"}),
                                    base::TimeTicks::Now() - element->second);
      }
    }
  }
}

void SensitiveContentManager::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  for (const FormGlobalId& form_id : updated_forms) {
    latency_until_sensitive_timer_[form_id] = base::TimeTicks::Now();
  }
  for (const FormGlobalId& form_id : removed_forms) {
    latency_until_sensitive_timer_.erase(form_id);
    if (const autofill::FormStructure* form =
            manager.FindCachedFormById(form_id)) {
      for (const std::unique_ptr<AutofillField>& field : form->fields()) {
        sensitive_fields_.erase(field->global_id());
      }
    }
  }
  UpdateContentSensitivity();
}

void SensitiveContentManager::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    LifecycleState previous,
    LifecycleState current) {
  autofill::LocalFrameToken local_frame_token =
      manager.driver().GetFrameToken();

  if (previous == LifecycleState::kActive &&
      current != LifecycleState::kActive) {
    // The frame is not active anymore, so its fields are not anymore in the
    // DOM.
    // If needed, the time complexity can be further improved here by exploiting
    // that fields from the same frame are next to each other in the set.
    std::erase_if(sensitive_fields_,
                  [local_frame_token](const autofill::FieldGlobalId& field_id) {
                    return field_id.frame_token == local_frame_token;
                  });
    std::erase_if(latency_until_sensitive_timer_,
                  [local_frame_token](const auto& item) {
                    FormGlobalId form_id = item.first;
                    return form_id.frame_token == local_frame_token;
                  });
  } else if (previous != LifecycleState::kActive &&
             current == LifecycleState::kActive) {
    // The frame became active, so its fields are present in the DOM again.
    const std::map<FormGlobalId, std::unique_ptr<autofill::FormStructure>>&
        forms = manager.form_structures();

    for (const auto& [form_id, form_structure] : forms) {
      const std::vector<std::unique_ptr<AutofillField>>& fields =
          form_structure->fields();
      for (const std::unique_ptr<AutofillField>& field : fields) {
        if (IsSensitiveAutofillType(field->Type().GetStorableType())) {
          sensitive_fields_.insert(field->global_id());
        }
      }
    }
  }

  UpdateContentSensitivity();
}

}  // namespace sensitive_content
