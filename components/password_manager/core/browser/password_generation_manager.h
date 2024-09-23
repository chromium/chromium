// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

class FormSaver;
class PasswordManagerClient;
class PasswordManagerDriver;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Class represents user changes on the initially generated password by noting
// if the |CharacterClass| (from
// components/autofill/core/browser/proto/password_requirements.proto) was
// added, deleted, characters changed or not changed from the generated password
// at the time of submission.
enum class CharacterClassPresenceChange {
  kNoChange = 0,
  kAdded = 1,
  kDeleted = 2,
  kSpecificCharactersChanged = 3,
  kMaxValue = kSpecificCharactersChanged,
};

class PasswordGenerationManager {
 public:
  explicit PasswordGenerationManager(PasswordManagerClient* client);
  ~PasswordGenerationManager();
  PasswordGenerationManager(const PasswordGenerationManager& rhs) = delete;
  PasswordGenerationManager& operator=(const PasswordGenerationManager&) =
      delete;

  std::unique_ptr<PasswordGenerationManager> Clone() const;

  // Returns true iff the generated password was presaved.
  bool HasGeneratedPassword() const { return presaved_.has_value(); }

  const std::u16string& generated_password() const {
    return presaved_->password_value;
  }

  // Called when user wants to start generation flow for |generated|.
  // |non_federated_matches| and |federated_matches| are used to determine
  // whether there is a username conflict. If there is none, the message is
  // synchronously passed to |driver|. Otherwise, the UI on the client is
  // invoked to ask for overwrite permission. There is one corner case that is
  // still not covered. The user had the current password saved with empty
  // username. |store_for_saving| indicates into which store the generated
  // password will be pre-saved.
  // - The change password form has no username.
  // - The user generates a password and sees the bubble with an empty username.
  // - The user clicks 'Update'.
  // - The actual form submission doesn't succeed for some reason.
  void GeneratedPasswordAccepted(
      PasswordForm generated,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          non_federated_matches,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          federated_matches,
      PasswordForm::Store store_for_saving,
      base::WeakPtr<PasswordManagerDriver> driver);

  // Called when generated password is accepted or changed by user.
  void PresaveGeneratedPassword(
      PasswordForm generated,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      FormSaver* form_saver);

  // Signals that the user cancels password generation.
  void PasswordNoLongerGenerated(FormSaver* form_saver);

  // Finish the generation flow by saving the final credential |generated|.
  // |matches| and |old_password| have the same meaning as in FormSaver.
  void CommitGeneratedPassword(PasswordForm generated,
                               base::span<const PasswordForm> matches,
                               const std::u16string& old_password,
                               PasswordForm::Store store_to_save,
                               FormSaver* profile_store_form_saver,
                               FormSaver* account_store_form_saver);

 private:
  void OnPresaveBubbleResult(const base::WeakPtr<PasswordManagerDriver>& driver,
                             bool accepted,
                             const PasswordForm& pending);

  // The client for the password form.
  const raw_ptr<PasswordManagerClient> client_;
  // Stores the pre-saved credential.
  std::optional<PasswordForm> presaved_;
  // Stores the initially generated password, i.e. before any user edits.
  std::u16string initial_generated_password_;
  // Used to produce callbacks.
  base::WeakPtrFactory<PasswordGenerationManager> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_
