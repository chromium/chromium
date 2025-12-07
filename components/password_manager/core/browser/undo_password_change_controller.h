// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UNDO_PASSWORD_CHANGE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UNDO_PASSWORD_CHANGE_CONTROLLER_H_

#include <optional>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace password_manager {

enum class PasswordRecoveryState {
  // Do not change anything about the suggestion. This should be the state of
  // every password without a backup.
  kRegularFlow,
  // Append a "Trouble signing in" suggestion at the end of the suggestions
  // list.
  kTroubleSigningIn,
  // Instead of the regular suggestions popup, create a custom one with a single
  // backup password.
  kShowProactiveRecovery,
  // Append a backup credential next to the primary credential.
  kIncludeBackup,
};

// Controller class for the password recovery flow.
// This class is attached to a tab and holds the recovery state of the last
// filled suggestion. All credentials start with an implicit
// `kRegularFlowState`. Only credentials with a backup password can change this
// state.
class UndoPasswordChangeController : public PasswordFormManagerObserver {
 public:
  UndoPasswordChangeController();
  ~UndoPasswordChangeController() override;

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

  // Called when PasswordManager detected a potentially failed login on
  // `login_form`.
  // This will simply store the `driver` and `login_form` and subscribe to form
  // parsing events. The actual logic of handling failed login is in
  // `OnPasswordFormParsed`.
  void OnLoginPotentiallyFailed(PasswordManagerDriver* driver,
                                const PasswordForm& login_form);

  // If the current state is `kShowProactiveRecovery`, goes through all
  // credentials in `fill_data` and finds the first one to match
  // `current_username_`
  // If nothing is found, returns empty optional.
  std::optional<autofill::PasswordAndMetadata>
  FindLoginWithProactiveRecoveryState(
      const autofill::PasswordFormFillData* fill_data) const;

  void OnSuggestionsHidden();

  // Called when the URL that the user interacts with changes. Resets the flow
  // if the signon realm changes.
  void OnNavigation(const url::Origin& url, ukm::SourceId ukm_source_id);

#if defined(UNIT_TEST)
  std::optional<PasswordForm> failed_login_form() { return failed_login_form_; }
#endif

 private:
  // PasswordFormManagerObserver:

  // If form_manager manages `failed_login_form_`, this will call renderer to
  // open a suggestions popup attached to the password field and at the same
  // time set the current state to `kShowProactiveRecovery` so that when
  // `PasswordAutofillManager` creates the suggestions, it will create a
  // proactive popup instead of the usual one.
  void OnPasswordFormParsed(PasswordFormManager* form_manager) override;

  void ResetFlow();

  void FinishObserving();

  url::Origin current_origin_;
  std::u16string current_username_;
  PasswordRecoveryState current_state_ = PasswordRecoveryState::kRegularFlow;
  std::optional<PasswordForm> failed_login_form_;
  base::WeakPtr<PasswordManagerDriver> driver_;
  // Keep the pointer to the cache to unsubsribe at destruction
  raw_ptr<PasswordFormCache> password_form_cache_;
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UNDO_PASSWORD_CHANGE_CONTROLLER_H_
