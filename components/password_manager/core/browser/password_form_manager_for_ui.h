// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_

#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

struct InteractionsStats;
class PasswordFormMetricsRecorder;

// Interface that contains all methods from PasswordFormManager that are used in
// UI.
class PasswordFormManagerForUI {
 public:
  virtual ~PasswordFormManagerForUI() = default;

  // Returns origin of the initially observed form.
  virtual const GURL& GetOrigin() const = 0;

  // Returns the best saved matches for the observed form.
  virtual const std::vector<const autofill::PasswordForm*>& GetBestMatches()
      const = 0;

  // Returns the federated saved matches for the observed form.
  // TODO(crbug.com/831123): merge with GetBestMatches.
  virtual std::vector<const autofill::PasswordForm*> GetFederatedMatches()
      const = 0;

  // Returns credentials that are ready to be written (saved or updated) to a
  // password store.
  virtual const autofill::PasswordForm& GetPendingCredentials() const = 0;

  // Returns who created this PasswordFormManager. The Credential Management API
  // uses a derived class of the PasswordFormManager that can indicate its
  // origin.
  virtual metrics_util::CredentialSourceType GetCredentialSource() const = 0;

  // Returns metric recorder which responsible for recording metrics for this
  // form.
  virtual PasswordFormMetricsRecorder* GetMetricsRecorder() = 0;

  // Statistics for recent password bubble usage.
  virtual base::span<const InteractionsStats> GetInteractionsStats() const = 0;

  // Determines if the user opted to 'never remember' passwords for this form.
  virtual bool IsBlacklisted() const = 0;

  // Handles save-as-new or update of the form managed by this manager.
  virtual void Save() = 0;

  // Updates the password store entry for |credentials_to_update|, using the
  // password from the pending credentials. It modifies the pending credentials.
  // |credentials_to_update| should be one of the best matches or the pending
  // credentials.
  virtual void Update(const autofill::PasswordForm& credentials_to_update) = 0;

  // Updates the username value. Called when user edits the username and clicks
  // the save button. Updates the username and modifies internal state
  // accordingly.
  virtual void OnUpdateUsernameFromPrompt(
      const base::string16& new_username) = 0;

  // Updates the password value. Called when user selects a password from the
  // password selection dropdown and clicks the save button. Updates the
  // password and modifies internal state accordingly.
  virtual void OnUpdatePasswordFromPrompt(
      const base::string16& new_password) = 0;

  // Called when the user chose not to update password.
  virtual void OnNopeUpdateClicked() = 0;

  // Called when the user clicked "Never" button in the "save password" prompt.
  virtual void OnNeverClicked() = 0;

  // Called when the user didn't interact with UI. |is_update| is true iff
  // it was the update UI.
  virtual void OnNoInteraction(bool is_update) = 0;

  // A user opted to 'never remember' passwords for this form.
  // Blacklist it so that from now on when it is seen we ignore it.
  virtual void PermanentlyBlacklist() = 0;

  // Called when the passwords were shown on on the bubble without obfuscation.
  virtual void OnPasswordsRevealed() = 0;
};

}  // namespace  password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_
