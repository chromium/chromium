// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"

#include <map>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "url/origin.h"

namespace {

using DriverFormKey = actor_login::ActorLoginFormFinder::DriverFormKey;

bool IsElementFocusable(autofill::FieldRendererId renderer_id,
                        const autofill::FormData& form_data) {
  auto field = std::ranges::find(form_data.fields(), renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(field != form_data.fields().end());
  return field->is_focusable();
}

bool IsLoginForm(const password_manager::PasswordForm& form) {
  const bool has_focusable_username =
      form.HasUsernameElement() &&
      IsElementFocusable(form.username_element_renderer_id, form.form_data);
  const bool has_focusable_password =
      form.HasPasswordElement() &&
      IsElementFocusable(form.password_element_renderer_id, form.form_data);
  const bool has_focusable_new_password =
      form.HasNewPasswordElement() &&
      IsElementFocusable(form.new_password_element_renderer_id, form.form_data);

  return (has_focusable_username || has_focusable_password) &&
         !has_focusable_new_password;
}

void OnCheckViewAreaVisibleFinished(
    actor_login::LoginFieldType type,
    base::RepeatingCallback<void(std::pair<actor_login::LoginFieldType, bool>)>
        barrier,
    bool visible) {
  barrier.Run(std::make_pair(type, visible));
}

optimization_guide::proto::ActorLoginQuality_FormData_FieldData_FieldType
GetFieldType(const autofill::FormFieldData& field,
             const password_manager::PasswordForm& form) {
  autofill::FieldRendererId field_id = field.renderer_id();
  if (field_id == form.username_element_renderer_id) {
    return optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
        USERNAME;
  } else if (field_id == form.password_element_renderer_id) {
    return optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
        PASSWORD;
  } else if (field_id == form.new_password_element_renderer_id) {
    return optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
        NEW_PASSWORD;
  } else if (field_id == form.confirmation_password_element_renderer_id) {
    return optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
        CONFIRMATION_PASSWORD;
  }
  return optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
      UNKNOWN;
}

bool IsFormOriginSupported(const url::Origin& form_origin,
                           const url::Origin& main_frame_origin) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginSameSiteIframeSupport)) {
    return net::registry_controlled_domains::SameDomainOrHost(
        form_origin, main_frame_origin,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  }
  return form_origin.IsSameOriginWith(main_frame_origin);
}

bool IsValidFrameAndOriginToFill(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    const url::Origin& main_frame_origin) {
  if (driver->IsNestedWithinFencedFrame()) {
    // Fenced frames should not be filled.
    return false;
  }

  if (!IsFormOriginSupported(driver->GetLastCommittedOrigin(),
                             main_frame_origin)) {
    return false;
  }

  bool is_same_origin =
      driver->GetLastCommittedOrigin().IsSameOriginWith(main_frame_origin);

  // We can fill a form if its frame context is considered safe and not overly
  // nested. A "fillable context" is either the primary main frame itself, or
  // a direct child of the primary main frame that is not a fenced frame.
  return is_same_origin || driver->IsInPrimaryMainFrame() ||
         driver->IsDirectChildOfPrimaryMainFrame();
}

void OnIsLoginFormAsyncFinished(
    DriverFormKey key,
    base::OnceCallback<void(std::pair<DriverFormKey, bool>)>
        notify_concurrent_callback,
    bool is_login_form) {
  std::move(notify_concurrent_callback)
      .Run(std::make_pair(std::move(key), is_login_form));
}

int64_t ComputeRequestDurationForLogs(base::TimeTicks start_time) {
  base::TimeDelta request_duration = base::TimeTicks::Now() - start_time;
  return ukm::GetSemanticBucketMinForDurationTiming(
      request_duration.InMilliseconds());
}

}  // namespace

namespace actor_login {

using ParsedFormDetails =
    optimization_guide::proto::ActorLoginQuality_ParsedFormDetails;

FormFinderResult::FormFinderResult() = default;

FormFinderResult::FormFinderResult(
    std::vector<password_manager::PasswordFormManager*> eligible_managers,
    std::vector<ParsedFormDetails> parsed_forms_details)
    : eligible_managers(std::move(eligible_managers)),
      parsed_forms_details(std::move(parsed_forms_details)) {}

FormFinderResult::~FormFinderResult() = default;

FormFinderResult::FormFinderResult(const FormFinderResult&) = default;
FormFinderResult& FormFinderResult::operator=(const FormFinderResult&) =
    default;
FormFinderResult::FormFinderResult(FormFinderResult&&) = default;
FormFinderResult& FormFinderResult::operator=(FormFinderResult&&) = default;

ActorLoginFormFinder::ActorLoginFormFinder(
    password_manager::PasswordManagerClient* client)
    : client_(client) {}
ActorLoginFormFinder::~ActorLoginFormFinder() = default;

// static
void ActorLoginFormFinder::SetFormData(
    optimization_guide::proto::ActorLoginQuality_FormData& form_data_proto,
    const password_manager::PasswordForm& form) {
  form_data_proto.set_form_signature(
      autofill::CalculateFormSignature(form.form_data).value());
  for (const auto& field : form.form_data.fields()) {
    optimization_guide::proto::ActorLoginQuality_FormData_FieldData field_data;
    field_data.set_signature(
        autofill::CalculateFieldSignatureForField(field).value());
    field_data.set_field_type(GetFieldType(field, form));
    *form_data_proto.add_field_data() = field_data;
  }
}

// static
std::u16string ActorLoginFormFinder::GetSourceSiteOrAppFromUrl(
    const GURL& url) {
  return base::UTF8ToUTF16(url.GetWithEmptyPath().spec());
}

// static
password_manager::PasswordFormManager*
ActorLoginFormFinder::GetSigninFormManager(
    const std::vector<password_manager::PasswordFormManager*>&
        eligible_managers) {
  password_manager::PasswordFormManager* signin_form_manager = nullptr;
  for (auto* manager : eligible_managers) {
    // Prefer filling the primary main frame form if one exists, but
    // also prefer more recently-parsed forms.
    if (manager->GetDriver()->IsInPrimaryMainFrame()) {
      signin_form_manager = manager;
    }
    // Otherwise, store this form manager and look to see if there is a primary
    // main frame form later.
    if (!signin_form_manager) {
      signin_form_manager = manager;
    }
  }
  return signin_form_manager;
}

FormFinderResult ActorLoginFormFinder::GetEligibleLoginFormManagers(
    const url::Origin& origin) {
  std::vector<password_manager::PasswordFormManager*> eligible_form_managers;
  password_manager::PasswordFormCache* form_cache =
      client_->GetPasswordManager()->GetPasswordFormCache();
  if (!form_cache) {
    return FormFinderResult(std::move(eligible_form_managers),
                            /*parsed_forms_details=*/{});
  }
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (!manager->GetDriver()) {
      continue;
    }

    if (!IsValidFrameAndOriginToFill(manager->GetDriver(), origin)) {
      continue;
    }

    const password_manager::PasswordForm* parsed_form =
        manager->GetParsedObservedForm();

    if (!parsed_form) {
      continue;
    }

    ParsedFormDetails form_details;
    SetFormData(*form_details.mutable_form_data(), *parsed_form);
    parsed_forms_details_.emplace_back(std::move(form_details));

    if (!IsLoginForm(*parsed_form)) {
      continue;
    }

    eligible_form_managers.emplace_back(manager.get());
  }
  return FormFinderResult(std::move(eligible_form_managers),
                          std::move(parsed_forms_details_));
}

void ActorLoginFormFinder::GetEligibleLoginFormManagersAsync(
    const url::Origin& origin,
    EligibleManagersCallback callback) {
  password_manager::PasswordFormCache* form_cache =
      client_->GetPasswordManager()->GetPasswordFormCache();
  CHECK(form_cache);

  std::vector<password_manager::PasswordFormManager*> candidate_managers;
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (!manager->GetDriver()) {
      continue;
    }

    if (!manager->GetParsedObservedForm()) {
      continue;
    }

    if (!IsValidFrameAndOriginToFill(manager->GetDriver(), origin)) {
      ParsedFormDetails form_details;
      SetFormData(*form_details.mutable_form_data(),
                  *manager->GetParsedObservedForm());
      form_details.set_is_valid_frame_and_origin(false);
      parsed_forms_details_.emplace_back(std::move(form_details));
      continue;
    }

    candidate_managers.emplace_back(manager.get());
  }

  if (candidate_managers.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       FormFinderResult(/*eligible_managers=*/{},
                                        std::move(parsed_forms_details_))));
    return;
  }

  base::ConcurrentCallbacks<std::pair<DriverFormKey, bool>> concurrent;
  for (password_manager::PasswordFormManager* manager : candidate_managers) {
    DriverFormKey key = std::make_pair(
        manager->GetParsedObservedForm()->form_data.renderer_id(),
        manager->GetDriver()->AsWeakPtr());

    base::OnceCallback<void(bool)> on_single_check_done =
        base::BindOnce(&OnIsLoginFormAsyncFinished, std::move(key),
                       concurrent.CreateCallback());

    IsLoginFormAsync(*manager->GetParsedObservedForm(), manager->GetDriver(),
                     std::move(on_single_check_done));
  }

  std::move(concurrent)
      .Done(base::BindOnce(&ActorLoginFormFinder::OnAllEligibleChecksCompleted,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void ActorLoginFormFinder::IsLoginFormAsync(
    const password_manager::PasswordForm& form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    base::OnceCallback<void(bool)> callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (!driver) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  ParsedFormDetails form_details;
  SetFormData(*form_details.mutable_form_data(), form);
  // This method should only be called for forms that are in a valid frame and
  // origin so setting this to `true` is correct.
  form_details.set_is_valid_frame_and_origin(true);

  // Add the fields to be checked.
  std::vector<std::pair<LoginFieldType, autofill::FieldRendererId>>
      fields_to_check;
  if (form.HasUsernameElement()) {
    fields_to_check.emplace_back(LoginFieldType::kUsername,
                                 form.username_element_renderer_id);
  }
  if (form.HasPasswordElement()) {
    fields_to_check.emplace_back(LoginFieldType::kPassword,
                                 form.password_element_renderer_id);
  }
  if (form.HasNewPasswordElement()) {
    fields_to_check.emplace_back(LoginFieldType::kNewPassword,
                                 form.new_password_element_renderer_id);
  }

  if (fields_to_check.empty()) {
    parsed_forms_details_.emplace_back(std::move(form_details));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  auto on_all_checks_done =
      base::BindOnce(&ActorLoginFormFinder::OnVisibilityChecksComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(form_details),
                     start_time, std::move(callback));

  auto barrier = base::BarrierCallback<std::pair<LoginFieldType, bool>>(
      fields_to_check.size(), std::move(on_all_checks_done));

  for (const auto& [type, renderer_id] : fields_to_check) {
    driver->CheckViewAreaVisible(
        renderer_id,
        base::BindOnce(&OnCheckViewAreaVisibleFinished, type, barrier));
  }
}

void ActorLoginFormFinder::OnVisibilityChecksComplete(
    ParsedFormDetails form_details,
    base::TimeTicks start_time,
    base::OnceCallback<void(bool)> callback,
    std::vector<std::pair<LoginFieldType, bool>> results) {
  bool is_username_visible = false;
  bool is_password_visible = false;
  bool is_new_password_visible = false;

  for (const auto& [type, is_visible] : results) {
    switch (type) {
      case LoginFieldType::kUsername:
        is_username_visible = is_visible;
        break;
      case LoginFieldType::kPassword:
        is_password_visible = is_visible;
        break;
      case LoginFieldType::kNewPassword:
        is_new_password_visible = is_visible;
        break;
    }
  }

  form_details.set_is_username_field_visible(is_username_visible);
  form_details.set_is_password_field_visible(is_password_visible);
  form_details.set_is_new_password_visible(is_new_password_visible);

  // Calculate and record time
  form_details.set_async_check_time_ms(
      ComputeRequestDurationForLogs(start_time));
  parsed_forms_details_.emplace_back(std::move(form_details));

  bool is_login_form =
      (is_username_visible || is_password_visible) && !is_new_password_visible;

  std::move(callback).Run(is_login_form);
}

void ActorLoginFormFinder::OnAllEligibleChecksCompleted(
    EligibleManagersCallback callback,
    std::vector<std::pair<DriverFormKey, bool>> results) {
  std::vector<password_manager::PasswordFormManager*> eligible_managers;

  password_manager::PasswordFormCache* form_cache =
      client_->GetPasswordManager()->GetPasswordFormCache();
  CHECK(form_cache);
  for (const auto& manager : form_cache->GetFormManagers()) {
    const password_manager::PasswordForm* parsed_form =
        manager->GetParsedObservedForm();
    if (!manager->GetDriver() || !parsed_form) {
      continue;
    }

    for (const auto& [key, is_login_form] : results) {
      if (is_login_form && manager->DoesManage(key.first, key.second.get())) {
        eligible_managers.emplace_back(manager.get());
        break;
      }
    }
  }

  std::move(callback).Run(FormFinderResult(std::move(eligible_managers),
                                           std::move(parsed_forms_details_)));
}

}  // namespace actor_login
