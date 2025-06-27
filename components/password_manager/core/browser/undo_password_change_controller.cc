// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/undo_password_change_controller.h"

#include <string>

#include "base/logging.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"

namespace password_manager {
UndoPasswordChangeController::UndoPasswordChangeController() = default;
UndoPasswordChangeController::~UndoPasswordChangeController() = default;

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
}

PasswordRecoveryState UndoPasswordChangeController::GetState(
    const std::u16string& username) const {
  if (username == current_username_) {
    return current_state_;
  }
  return PasswordRecoveryState::kRegularFlow;
}

void UndoPasswordChangeController::ResetFlow() {
  current_state_ = PasswordRecoveryState::kRegularFlow;
  current_username_ = u"";
}

}  // namespace password_manager
