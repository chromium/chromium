// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "url/origin.h"

namespace password_manager {

PasswordCredentialFillerImpl::PasswordCredentialFillerImpl(
    base::WeakPtr<PasswordManagerDriver> driver,
    const autofill::PasswordSuggestionRequest& request)
    : driver_(driver),
      submission_readiness_(
          CalculateSubmissionReadiness(request.form_data,
                                       request.username_field_id,
                                       request.password_field_id)),
      trigger_submission_(CalculateTriggerSubmission(submission_readiness_)) {}

PasswordCredentialFillerImpl::~PasswordCredentialFillerImpl() = default;

void PasswordCredentialFillerImpl::FillUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password,
    base::OnceCallback<void(bool)> callback) {
  if (!driver_) {
    // If `driver_` (per frame) was destroyed, it means a navigation happened
    // and the filling data doesn't apply to the new page. The correct behavior
    // in this case is to hide the filling UI, meaning this code path is
    // unreachable. *However*, if the UI wasn't hidden due to a bug, simply
    // ignore the click here. That's better than:
    //   a) Proceeding, which will cause a nullptr deref below.
    //   b) CHECK(driver_), which would crash.
    // Supposedly, the user can still dismiss the UI to get out of the broken
    // state. See crbug.com/349073346.
    return;
  }

  driver_->FillSuggestion(
      username, password,
      base::BindOnce(&PasswordCredentialFillerImpl::TryTriggerSubmission,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     username));
}

void PasswordCredentialFillerImpl::TryTriggerSubmission(
    base::OnceCallback<void(bool)> callback,
    const std::u16string& username,
    bool was_filling_successful) {
  trigger_submission_ &= !username.empty() && was_filling_successful;

  if (trigger_submission_) {
    // TODO(crbug.com/40209736): As auto-submission has been launched, measuring
    // the time between filling by TTF and submisionn is not crucial. Remove
    // this call, the method itself and the metrics if we are not going to use
    // all that for new launches, e.g. crbug.com/1393043.
    driver_->TriggerFormSubmission();
  }
  std::move(callback).Run(trigger_submission_);
}

void PasswordCredentialFillerImpl::UpdateTriggerSubmission(bool new_value) {
  trigger_submission_ = new_value;
}

bool PasswordCredentialFillerImpl::ShouldTriggerSubmission() const {
  return trigger_submission_;
}

SubmissionReadinessState
PasswordCredentialFillerImpl::GetSubmissionReadinessState() const {
  return submission_readiness_;
}

GURL PasswordCredentialFillerImpl::GetFrameUrl() const {
  return driver_ ? driver_->GetLastCommittedURL() : GURL();
}

url::Origin PasswordCredentialFillerImpl::GetFrameOrigin() const {
  return driver_ ? driver_->GetLastCommittedOrigin() : url::Origin();
}

base::WeakPtr<PasswordCredentialFiller>
PasswordCredentialFillerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
