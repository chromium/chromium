// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UNDO_PASSWORD_CHANGE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UNDO_PASSWORD_CHANGE_CONTROLLER_H_

#include <optional>
#include <string>
#include <unordered_map>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

enum class PasswordRecoveryState {
  // Do not change anything about the suggestion. This should be the state of
  // every password without a backup.
  kRegularFlow,
  // Append a "Trouble signing in" suggestion at the end of the suggestions
  // list.
  kTroubleSigningIn,
  // Append a backup credential next to the primary credential.
  kIncludeBackup,
};

// Controller class for the password recovery flow.
// This class is attached to a tab and holds the recovery state of the last
// filled suggestion. All credentials start with an implicit
// `kRegularFlowState`. Only credentials with a backup password can change this
// state.
class UndoPasswordChangeController {
 public:
  UndoPasswordChangeController();
  ~UndoPasswordChangeController();

  // Updates the state of the filled `credential`:
  // - If credential doesn't have a backup password, resets the flow and ignores
  // the credential.
  // - If `credential.username_value` != `current_username_`, resets the flow.
  // - `kRegularFlow` -> `kTroubleSigningIn`
  // - `kTroubleSigningIn` -> `kTroubleSigningIn` (no-op)
  // - `kIncludeBackup` -> `kIncludeBackup` (no-op)
  //
  // `kTroubleSigningIn` can be changed using `OnTroubleSigningInClicked`.
  // `kIncludeBackup` is a terminal state.
  void OnSuggestionSelected(const autofill::PasswordAndMetadata& credential);

  // Progresses the `kTroubleSigningIn` state to `kIncludeBackup`.
  void OnTroubleSigningInClicked(
      const autofill::Suggestion::PasswordSuggestionDetails& credentials);

  // Return the current state if the `username` matches `current_username`.
  // Returns `kRegularFlow` otherwise.
  PasswordRecoveryState GetState(const std::u16string& username) const;

 private:
  void ResetFlow();

  std::u16string current_username_;
  PasswordRecoveryState current_state_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UNDO_PASSWORD_CHANGE_CONTROLLER_H_
