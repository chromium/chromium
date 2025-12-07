// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/undo_password_change_controller.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "url/origin.h"

namespace password_manager {

namespace {

constexpr char kPasswordChangeRecoveryFlowStateHistogram[] =
    "PasswordManager.PasswordChangeRecoveryFlow";

}  // namespace

UndoPasswordChangeController::UndoPasswordChangeController() = default;
UndoPasswordChangeController::~UndoPasswordChangeController() {
  FinishObserving();
}

void UndoPasswordChangeController::OnSuggestionSelected(
    const autofill::PasswordAndMetadata& password_and_metadata) {
  if (!password_and_metadata.backup_password_value) {
    ResetFlow();
    return;
  }
  if (password_and_metadata.username_value != current_username_) {
    ResetFlow();
  }

  current_username_ = password_and_metadata.username_value;
  if (current_state_ == PasswordRecoveryState::kRegularFlow) {
    current_state_ = PasswordRecoveryState::kTroubleSigningIn;
  }
}

void UndoPasswordChangeController::OnTroubleSigningInClicked(
    const autofill::Suggestion::PasswordSuggestionDetails& suggestion_details) {
  CHECK_EQ(suggestion_details.username, current_username_);

  current_state_ = PasswordRecoveryState::kIncludeBackup;

  base::UmaHistogramEnumeration(
      kPasswordChangeRecoveryFlowStateHistogram,
      password_manager::metrics_util::PasswordChangeRecoveryFlowState::
          kTroubleSigningInClicked);
  ukm::builders::PasswordManager_ChangeRecovery(ukm_source_id_)
      .SetPasswordChangeRecoveryFlow(static_cast<int>(
          password_manager::metrics_util::PasswordChangeRecoveryFlowState::
              kTroubleSigningInClicked))
      .Record(ukm::UkmRecorder::Get());
}

void UndoPasswordChangeController::OnLoginPotentiallyFailed(
    PasswordManagerDriver* driver,
    const PasswordForm& login_form) {
  auto password_field_it = std::ranges::find(
      login_form.form_data.fields(), login_form.password_element_renderer_id,
      &autofill::FormFieldData::renderer_id);
  // Ignore forms where we couldn't possibly fill passwords. This is most likely
  // a false positive failed login detection.
  if (password_field_it == login_form.form_data.fields().end() ||
      !password_field_it->is_focusable()) {
    return;
  }
  driver_ = driver->AsWeakPtr();
  failed_login_form_ = login_form;
  password_form_cache_ = driver_->GetPasswordManager()->GetPasswordFormCache();
  password_form_cache_->AddObserver(this);
}

PasswordRecoveryState UndoPasswordChangeController::GetState(
    const std::u16string& username) const {
  if (username == current_username_) {
    return current_state_;
  }
  return PasswordRecoveryState::kRegularFlow;
}

std::optional<autofill::PasswordAndMetadata>
UndoPasswordChangeController::FindLoginWithProactiveRecoveryState(
    const autofill::PasswordFormFillData* fill_data) const {
  if (current_state_ != PasswordRecoveryState::kShowProactiveRecovery) {
    return std::nullopt;
  }

  if (fill_data->preferred_login.username_value == current_username_) {
    return fill_data->preferred_login;
  }
  auto additional_login_it =
      std::ranges::find(fill_data->additional_logins, current_username_,
                        &autofill::PasswordAndMetadata::username_value);
  if (additional_login_it != fill_data->additional_logins.end()) {
    return *additional_login_it;
  }

  return std::nullopt;
}

void UndoPasswordChangeController::OnSuggestionsHidden() {
  if (current_state_ == PasswordRecoveryState::kShowProactiveRecovery) {
    current_state_ = PasswordRecoveryState::kIncludeBackup;

    base::UmaHistogramEnumeration(
        kPasswordChangeRecoveryFlowStateHistogram,
        password_manager::metrics_util::PasswordChangeRecoveryFlowState::
            kProactiveRecoveryPopupShown);
    ukm::builders::PasswordManager_ChangeRecovery(ukm_source_id_)
        .SetPasswordChangeRecoveryFlow(static_cast<int>(
            password_manager::metrics_util::PasswordChangeRecoveryFlowState::
                kProactiveRecoveryPopupShown))
        .Record(ukm::UkmRecorder::Get());
  }
  FinishObserving();
}

void UndoPasswordChangeController::OnNavigation(const url::Origin& origin,
                                                ukm::SourceId ukm_source_id) {
  if (current_origin_ != origin) {
    ResetFlow();
  }
  current_origin_ = origin;
  ukm_source_id_ = ukm_source_id;
}

void UndoPasswordChangeController::ResetFlow() {
  current_state_ = PasswordRecoveryState::kRegularFlow;
  current_username_ = u"";
  FinishObserving();
}

void UndoPasswordChangeController::FinishObserving() {
  failed_login_form_ = std::nullopt;
  driver_ = nullptr;
  if (password_form_cache_) {
    password_form_cache_->RemoveObserver(this);
  }
}

void UndoPasswordChangeController::OnPasswordFormParsed(
    PasswordFormManager* form_manager) {
  CHECK(form_manager);
  if (!failed_login_form_ || !driver_) {
    return;
  }

  if (form_manager->DoesManageSimilarForm(failed_login_form_.value(),
                                          driver_.get())) {
    const PasswordForm* form_best_match =
        password_manager_util::FindFormByUsername(
            form_manager->GetBestMatches(), failed_login_form_->username_value);
    if (!form_best_match || !form_best_match->GetPasswordBackup() ||
        form_best_match->GetPasswordBackup() ==
            failed_login_form_->password_value ||
        !base::FeatureList::IsEnabled(features::kShowRecoveryPassword)) {
      FinishObserving();
      return;
    }

    current_username_ = form_best_match->username_value;
    current_state_ = PasswordRecoveryState::kShowProactiveRecovery;
    // `OnPasswordFormParsed` is called before we send password data to
    // renderer. Post a task to give renderer a chance to receive password
    // data before creating the proactive recovery popup.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<PasswordManagerDriver> driver,
               autofill::FieldRendererId password_field) {
              if (driver) {
                driver->TriggerPasswordRecoverySuggestions(password_field);
              }
            },
            driver_->AsWeakPtr(),
            form_manager->GetParsedObservedForm()
                ->password_element_renderer_id));
  }
}

}  // namespace password_manager
