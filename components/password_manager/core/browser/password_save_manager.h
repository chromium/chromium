// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_H_

#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace signin {
class GaiaIdHash;
}  // namespace signin

namespace password_manager {

namespace metrics_util {
enum class MoveToAccountStoreTrigger;
}

class PasswordManagerClient;
class FormFetcher;
class VotesUploader;
class FormSaver;
class PasswordFormMetricsRecorder;
class PasswordManagerDriver;
struct PasswordForm;

// Implementations of this interface should encapsulate the password Save/Update
// logic. This ensures that the PasswordFormManager stays agnostic to whether
// one password store or multiple password stores are active. While FormSaver
// abstracts the implementation of different operations (e.g. Save()),
// PasswordSaveManager is responsible for deciding what and where to Save().
class PasswordSaveManager {
 public:
  PasswordSaveManager() = default;

  PasswordSaveManager(const PasswordSaveManager&) = delete;
  PasswordSaveManager& operator=(const PasswordSaveManager&) = delete;

  virtual ~PasswordSaveManager() = default;

  virtual void Init(PasswordManagerClient* client,
                    const FormFetcher* form_fetcher,
                    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
                    VotesUploader* votes_uploader) = 0;

  virtual const PasswordForm& GetPendingCredentials() const = 0;

  virtual const std::u16string& GetGeneratedPassword() const = 0;

  virtual FormSaver* GetProfileStoreFormSaverForTesting() const = 0;

  // Create pending credentials from |parsed_submitted_form| and |observed_form|
  // and |submitted_form|. In the case of HTTP or proxy auth no |observed_form|
  // exists, so this parameter is optional.
  virtual void CreatePendingCredentials(
      const PasswordForm& parsed_submitted_form,
      const autofill::FormData* observed_form,
      const autofill::FormData& submitted_form,
      bool is_http_auth,
      bool is_credential_api_save) = 0;

  virtual void ResetPendingCredentials() = 0;

  // Saves `parsed_submitted_form` to the store. An optional `observed_form` is
  // passed along to be able to send votes. This is null for HTTP or proxy auth.
  virtual void Save(const autofill::FormData* observed_form,
                    const PasswordForm& parsed_submitted_form) = 0;

  virtual void Blocklist(const PasswordFormDigest& form_digest) = 0;
  virtual void Unblocklist(const PasswordFormDigest& form_digest) = 0;

  // Called when generated password is accepted or changed by user.
  virtual void PresaveGeneratedPassword(PasswordForm parsed_form) = 0;

  // Called when user wants to start generation flow for |generated|.
  virtual void GeneratedPasswordAccepted(
      PasswordForm parsed_form,
      base::WeakPtr<PasswordManagerDriver> driver) = 0;

  // Signals that the user cancels password generation.
  virtual void PasswordNoLongerGenerated() = 0;

  // Moves the pending credentials together with any other PSL matched ones from
  // the profile store to the account store.
  // |trigger| represents the user action that triggered the flow and is used
  // for recording metrics.
  virtual void MoveCredentialsToAccountStore(
      metrics_util::MoveToAccountStoreTrigger trigger) = 0;

  // Adds the |gaia_id_hash| to the |moving_blocked_for_list| of the
  // PasswordForm returned by GetPendingCredentials() and stores it in the
  // profile store. This is relevant only for account store users.
  virtual void BlockMovingToAccountStoreFor(
      const signin::GaiaIdHash& gaia_id_hash) = 0;

  // Updates the submission indicator event for pending credentials at the
  // moment of submisison detection.
  virtual void UpdateSubmissionIndicatorEvent(
      autofill::mojom::SubmissionIndicatorEvent event) = 0;

  virtual bool IsNewLogin() const = 0;
  virtual bool IsPasswordUpdate() const = 0;
  virtual bool HasGeneratedPassword() const = 0;

  // Signals that the user updated the username value in the bubble prompt.
  virtual void UsernameUpdatedInBubble() = 0;

  // Returns the password store type into which the form is going to be saved or
  // updated. It might be that the credential is updated in both stores; in this
  // case the result will be the enum value with both bits set (the account and
  // the profile store bits).
  virtual PasswordForm::Store GetPasswordStoreForSaving(
      const PasswordForm& password_form) const = 0;

  virtual std::unique_ptr<PasswordSaveManager> Clone() = 0;
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_H_
