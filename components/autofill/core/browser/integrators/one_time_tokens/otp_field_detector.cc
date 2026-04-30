// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace autofill {

namespace {

// Returns true if the form contains at least one ONE_TIME_CODE field and
// all ONE_TIME_CODE fields in the form are same-site with the main frame's
// origin.
[[nodiscard]] bool IsOtpForm(const FormStructure& form) {
  const bool restrict_to_same_tld = base::FeatureList::IsEnabled(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  bool has_otp_field = false;
  for (const std::unique_ptr<AutofillField>& f : form.fields()) {
    if (!f->Type().GetTypes().contains(ONE_TIME_CODE) || !f->is_focusable()) {
      continue;
    }
    has_otp_field = true;
    if (!restrict_to_same_tld) {
      return true;
    }

    if (!net::registry_controlled_domains::SameDomainOrHost(
            f->origin(), form.main_frame_origin(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      // TODO(crbug.com/441433533): Consider making this less strict by
      // introducing a field-level check in the manager instead of dropping
      // the entire form.
      return false;
    }
  }

  return has_otp_field;
}

// Returns true if the `form_id` in `manager` contains at least one ONE_TIME_CODE
// field and all ONE_TIME_CODE fields in the form are same-site with the main
// frame's origin.
[[nodiscard]] bool IsOtpForm(const AutofillManager& manager,
                             FormGlobalId form_id) {
  const FormStructure* form_structure = manager.FindCachedFormById(form_id);
  return form_structure && IsOtpForm(*form_structure);
}

}  // namespace

OtpFieldDetector::OtpFieldDetector(AutofillClient* client) {
  if (client) {
    autofill_manager_observation_.Observe(client);
  }
}
OtpFieldDetector::~OtpFieldDetector() = default;

base::CallbackListSubscription
OtpFieldDetector::RegisterOtpFieldsDetectedCallback(
    OtpFieldDetector::OtpFieldsDetectedCallback callback) {
  return callback_list_otp_fields_detected_.Add(std::move(callback));
}

base::CallbackListSubscription
OtpFieldDetector::RegisterOtpFieldsSubmittedCallback(
    OtpFieldDetector::OtpFieldsSubmittedCallback callback) {
  return callback_list_otp_fields_submitted_.Add(std::move(callback));
}

bool OtpFieldDetector::IsOtpFieldPresent() const {
  const bool is_otp_present = !forms_with_otps_.empty();
  // TODO(crbug.com/415273270) This metric could be improved because
  // 1) there is no guarantee inside `OtpFieldDetector` that
  //   `IsOneTimeTokenFieldPresent()` is called only once
  // 2) because the `OtpFieldDetector` also also considers OTP fields
  //    in iframes (i.e. the "InMainFrame" suffix is incorrect).
  // This exists for legacy purposes.
  base::UmaHistogramBoolean("PasswordManager.OtpPresentInMainTab",
                            is_otp_present);
  return is_otp_present;
}

void OtpFieldDetector::OnFieldTypesDetermined(AutofillManager& manager,
                                              FormGlobalId form,
                                              FieldTypeSource source,
                                              bool small_forms_were_parsed) {
  if (IsOtpForm(manager, form)) {
    AddFormAndNotifyIfNecessary(form);
  } else {
    RemoveFormAndNotifyIfNecessary(form);
  }
}

void OtpFieldDetector::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  for (const FormGlobalId form : removed_forms) {
    RemoveFormAndNotifyIfNecessary(form);
  }
}

void OtpFieldDetector::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillDriver::LifecycleState old_state,
    AutofillDriver::LifecycleState new_state) {
  if (new_state != AutofillDriver::LifecycleState::kActive) {
    manager.ForEachCachedForm([&](const FormStructure& form) {
      RemoveFormAndNotifyIfNecessary(form.global_id());
    });
  } else {
    manager.ForEachCachedForm([&](const FormStructure& form) {
      if (IsOtpForm(form)) {
        AddFormAndNotifyIfNecessary(form.global_id());
      }
    });
  }
}

void OtpFieldDetector::OnAfterFormSubmitted(AutofillManager& manager,
                                            const FormData& form) {
  RemoveFormAndNotifyIfNecessary(form.global_id());
}

void OtpFieldDetector::AddFormAndNotifyIfNecessary(FormGlobalId form_id) {
  // Memorize the state of `forms_with_otps_` before the update, then perform
  // the update and only then notify the callbacks to ensure that
  // `IsOneTimeTokenFieldPresent()` returns the correct answer when called by
  // a notified callback.
  const bool was_empty = forms_with_otps_.empty();
  forms_with_otps_.insert(form_id);
  if (was_empty) {
    callback_list_otp_fields_detected_.Notify();
  }
}

void OtpFieldDetector::RemoveFormAndNotifyIfNecessary(FormGlobalId form_id) {
  if (!forms_with_otps_.contains(form_id)) {
    return;
  }
  forms_with_otps_.erase(form_id);
  if (forms_with_otps_.empty()) {
    callback_list_otp_fields_submitted_.Notify();
  }
}

}  // namespace autofill
