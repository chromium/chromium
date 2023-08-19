// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace password_manager {

struct InteractionsStats;
struct PasswordForm;
class PasswordFormMetricsRecorder;

// Interface that contains all methods from PasswordFormManager that are used in
// UI.
class PasswordFormManagerForUI {
 public:
  virtual ~PasswordFormManagerForUI() = default;

  // Returns URL of the initially observed form.
  virtual const GURL& GetURL() const = 0;

  // Returns the best saved matches for the observed form.
  virtual const std::vector<const PasswordForm*>& GetBestMatches() const = 0;

  // Returns the federated saved matches for the observed form.
  // TODO(crbug.com/831123): merge with GetBestMatches.
  virtual std::vector<const PasswordForm*> GetFederatedMatches() const = 0;

  // Returns credentials that are ready to be written (saved or updated) to a
  // password store.
  virtual const PasswordForm& GetPendingCredentials() const = 0;

  // Returns who created this PasswordFormManager. The Credential Management API
  // uses a derived class of the PasswordFormManager that can indicate its
  // origin.
  virtual metrics_util::CredentialSourceType GetCredentialSource() const = 0;

  // Returns metric recorder which responsible for recording metrics for this
  // form.
  virtual PasswordFormMetricsRecorder* GetMetricsRecorder() = 0;

  // Statistics for recent password bubble usage.
  virtual base::span<const InteractionsStats> GetInteractionsStats() const = 0;

  // List of insecure passwords for the current site.
  virtual std::vector<const PasswordForm*> GetInsecureCredentials() const = 0;

  // Determines if the user opted to 'never remember' passwords for this form.
  virtual bool IsBlocklisted() const = 0;

  // Determines whether the submitted credentials returned by
  // GetPendingCredentials() can be moved to the signed in account store.
  // Returns true if the submitted credentials are stored in the profile store
  // and the current signed in user didn't block moving them.
  virtual bool IsMovableToAccountStore() const = 0;

  // Handles save-as-new or update of the form managed by this manager.
  virtual void Save() = 0;

  // Updates the password store entry for |credentials_to_update|, using the
  // password from the pending credentials. It modifies the pending credentials.
  // |credentials_to_update| should be one of the best matches or the pending
  // credentials.
  virtual void Update(const PasswordForm& credentials_to_update) = 0;

  // Updates the username value. Called when user edits the username and clicks
  // the save button. Updates the username and modifies internal state
  // accordingly.
  virtual void OnUpdateUsernameFromPrompt(
      const std::u16string& new_username) = 0;

  // Updates the password value. Called when user selects a password from the
  // password selection dropdown and clicks the save button. Updates the
  // password and modifies internal state accordingly.
  virtual void OnUpdatePasswordFromPrompt(
      const std::u16string& new_password) = 0;

  // Called when the user chose not to update password.
  virtual void OnNopeUpdateClicked() = 0;

  // Called when the user clicked "Never" button in the "save password" prompt.
  virtual void OnNeverClicked() = 0;

  // Called when the user didn't interact with UI. |is_update| is true iff
  // it was the update UI.
  virtual void OnNoInteraction(bool is_update) = 0;

  // A user opted to 'never remember' passwords for this form.
  // Blocklist it so that from now on when it is seen we ignore it.
  virtual void Blocklist() = 0;

  // Called when the passwords were shown on on the bubble without obfuscation.
  virtual void OnPasswordsRevealed() = 0;

  // A user opted to move the credentials used for a successful login from the
  // profile store to the account store.
  virtual void MoveCredentialsToAccountStore() = 0;

  // Suppresses future prompts for moving the submitted credentials returned by
  // GetPendingCredentials() to the account store of the currently signed in
  // user.
  virtual void BlockMovingCredentialsToAccountStore() = 0;
};

}  // namespace  password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_
