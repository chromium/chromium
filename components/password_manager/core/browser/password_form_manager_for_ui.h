// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class FormFetcher;
class PasswordFormMetricsRecorder;

// Interface that contains all methods from PasswordFormManager that are used in
// UI.
class PasswordFormManagerForUI {
 public:
  virtual ~PasswordFormManagerForUI() = default;

  // Returns the form fetcher which is responsible for fetching saved matches
  // from the store for the observed from.
  virtual FormFetcher* GetFormFetcher() = 0;

  // Returns origin of the initially observed form.
  virtual const GURL& GetOrigin() const = 0;

  // Returns the best saved matches for the observed form.
  virtual const std::map<base::string16, const autofill::PasswordForm*>&
  GetBestMatches() const = 0;

  // Returns credentials that are ready to be written (saved or updated) to a
  // password store.
  virtual const autofill::PasswordForm& GetPendingCredentials() const = 0;

  // Returns who created this PasswordFormManager. The Credential Management API
  // uses a derived class of the PasswordFormManager that can indicate its
  // origin.
  virtual metrics_util::CredentialSourceType GetCredentialSource() = 0;

  // Returns metric recorder which responsible for recording metrics for this
  // form.
  virtual PasswordFormMetricsRecorder* GetMetricsRecorder() = 0;

  // Returns the blacklisted matches for the current page.
  virtual const std::vector<const autofill::PasswordForm*>&
  GetBlacklistedMatches() const = 0;

  // Determines if the user opted to 'never remember' passwords for this form.
  virtual bool IsBlacklisted() const = 0;

  // Returns whether filled password was overriden by the user.
  // TODO(https://crbug.com/845826): Remove once mobile username
  // editing is implemented.
  virtual bool IsPasswordOverridden() const = 0;

  virtual const autofill::PasswordForm* GetPreferredMatch() const = 0;

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
  virtual void UpdateUsername(const base::string16& new_username) = 0;

  // Updates the password value. Called when user selects a password from the
  // password selection dropdown and clicks the save button. Updates the
  // password and modifies internal state accordingly.
  virtual void UpdatePasswordValue(const base::string16& new_password) = 0;

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

class PasswordManagerDriver;

// This is a temporary class for unification of processing of old an new
// PasswordFormManager in PasswordManager.
// TODO(https://crbug.com/831123): Remove when the old PasswordFormManager is
// gone.
class PasswordFormManagerInterface : public PasswordFormManagerForUI {
 public:
  // Returns whether it is a new (i.e. not saved yet) credentials.
  virtual bool IsNewLogin() const = 0;

  // Returns true if the current pending credentials were found using
  // origin matching of the public suffix, instead of the signon realm of the
  // form.
  virtual bool IsPendingCredentialsPublicSuffixMatch() const = 0;

  // Called when generated password is accepted or changed by user.
  virtual void PresaveGeneratedPassword(const autofill::PasswordForm& form) = 0;

  // Called when user removed a generated password.
  virtual void PasswordNoLongerGenerated() = 0;

  // Returns if the password was generated.
  virtual bool HasGeneratedPassword() const = 0;

  // Called when the generation popup is shown. |is_manual_generation| true if
  // the generation was initiated by the user. It is used for metrics and votes
  // uploading.
  // TODO(https://crbug.com/831123): Remove |generation_popup_was_shown| it is
  // always true.
  virtual void SetGenerationPopupWasShown(bool generation_popup_was_shown,
                                          bool is_manual_generation) = 0;

  // Called when the generation element with identifier |generation_element| is
  // found on the page. It is used for metrics and votes uploading.
  virtual void SetGenerationElement(
      const base::string16& generation_element) = 0;

  // True if we consider this form to be a change password form without username
  // field. We use only client heuristics, so it could include signup forms.
  virtual bool IsPossibleChangePasswordFormWithoutUsername() const = 0;

  // A form is considered to be "retry" password if it has only one field which
  // is a current password field.
  virtual bool RetryPasswordFormPasswordUpdate() const = 0;

  // Returns the drivers representing all the frames for the form.
  virtual std::vector<base::WeakPtr<PasswordManagerDriver>> GetDrivers()
      const = 0;

  // Returns the submitted form, if it exists, otherwise nullptr.
  virtual const autofill::PasswordForm* GetSubmittedForm() const = 0;
};

}  // namespace  password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_FOR_UI_H_
