// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"

#include <string>
#include "base/check.h"

namespace {

using autofill::mojom::SubmissionReadinessState;

// Infers whether a form should be submitted based on the feature's state and
// the form's structure (submission_readiness).
bool CalculateTriggerSubmission(SubmissionReadinessState submission_readiness) {
  switch (submission_readiness) {
    case SubmissionReadinessState::kNoInformation:
    case SubmissionReadinessState::kError:
    case SubmissionReadinessState::kNoUsernameField:
    case SubmissionReadinessState::kNoPasswordField:
    case SubmissionReadinessState::kFieldBetweenUsernameAndPassword:
    case SubmissionReadinessState::kFieldAfterPasswordField:
      return false;

    case SubmissionReadinessState::kEmptyFields:
    case SubmissionReadinessState::kMoreThanTwoFields:
    case SubmissionReadinessState::kTwoFields:
      return true;
  }
}

}  // namespace

namespace password_manager {

PasswordCredentialFillerImpl::PasswordCredentialFillerImpl(
    base::WeakPtr<PasswordManagerDriver> driver,
    SubmissionReadinessState submission_readiness)
    : driver_(driver),
      submission_readiness_(submission_readiness),
      trigger_submission_(CalculateTriggerSubmission(submission_readiness)) {}

PasswordCredentialFillerImpl::~PasswordCredentialFillerImpl() {
  CHECK(!IsReadyToFill()) << "If 'FillUsernameAndPassword' wasn't called, "
                          << "make sure to call 'CleanUp'!";
}

bool PasswordCredentialFillerImpl::IsReadyToFill() {
  return !!driver_;
}

void PasswordCredentialFillerImpl::FillUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password) {
  CHECK(driver_);

  driver_->KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false));
  driver_->FillSuggestion(username, password);

  trigger_submission_ &= !username.empty();

  if (trigger_submission_) {
    // TODO(crbug.com/1283004): As auto-submission has been launched, measuring
    // the time between filling by TTF and submisionn is not crucial. Remove
    // this call, the method itself and the metrics if we are not going to use
    // all that for new launches, e.g. crbug.com/1393043.
    driver_->TriggerFormSubmission();
  }
  driver_ = nullptr;
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

const GURL& PasswordCredentialFillerImpl::GetFrameUrl() const {
  CHECK(driver_);
  return driver_->GetLastCommittedURL();
}

void PasswordCredentialFillerImpl::CleanUp(ToShowVirtualKeyboard should_show) {
  // TODO(crbug/1434278): Avoid using KeyboardReplacingSurfaceClosed.
  std::exchange(driver_, nullptr)->KeyboardReplacingSurfaceClosed(should_show);
}

}  // namespace password_manager
