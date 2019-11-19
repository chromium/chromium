// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

class FormFetcher;
class FormSaver;
class PasswordManagerClient;
class PasswordManagerDriver;

class PasswordGenerationManager {
 public:
  PasswordGenerationManager(FormSaver* form_saver,
                            PasswordManagerClient* client);
  ~PasswordGenerationManager();
  PasswordGenerationManager(const PasswordGenerationManager& rhs) = delete;
  PasswordGenerationManager& operator=(const PasswordGenerationManager&) =
      delete;

  std::unique_ptr<PasswordGenerationManager> Clone(FormSaver* form_saver) const;

  // Returns true iff the generated password was presaved.
  bool HasGeneratedPassword() const { return presaved_.has_value(); }

  const base::string16& generated_password() const {
    return presaved_->password_value;
  }

  // Called when user wants to start generation flow for |generated|. If there
  // is no username conflict, the message is synchronously passed to |driver|.
  // |fetcher| to fill that UI with correct data.
  // Otherwise, the UI on the client is invoked to ask for overwrite permission.
  // There is one corner case that is still not covered.
  // The user had the current password saved with empty username.
  // - The change password form has no username.
  // - The user generates a password and sees the bubble with an empty username.
  // - The user clicks 'Update'.
  // - The actual form submission doesn't succeed for some reason.
  void GeneratedPasswordAccepted(autofill::PasswordForm generated,
                                 const FormFetcher& fetcher,
                                 base::WeakPtr<PasswordManagerDriver> driver);

  // Called when generated password is accepted or changed by user.
  void PresaveGeneratedPassword(
      autofill::PasswordForm generated,
      const std::vector<const autofill::PasswordForm*>& matches);

  // Signals that the user cancels password generation.
  void PasswordNoLongerGenerated();

  // Finish the generation flow by saving the final credential |generated|.
  // |matches| and |old_password| have the same meaning as in FormSaver.
  void CommitGeneratedPassword(
      autofill::PasswordForm generated,
      const std::vector<const autofill::PasswordForm*>& matches,
      const base::string16& old_password);

#if defined(UNIT_TEST)
  void set_clock(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }
#endif

 private:
  void OnPresaveBubbleResult(const base::WeakPtr<PasswordManagerDriver>& driver,
                             bool accepted,
                             const autofill::PasswordForm& pending);

  // Weak reference to the interface for saving credentials.
  FormSaver* const form_saver_;
  // The client for the password form.
  PasswordManagerClient* const client_;
  // Stores the pre-saved credential.
  base::Optional<autofill::PasswordForm> presaved_;
  // Interface to get current time.
  std::unique_ptr<base::Clock> clock_;
  // Used to produce callbacks.
  base::WeakPtrFactory<PasswordGenerationManager> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_
