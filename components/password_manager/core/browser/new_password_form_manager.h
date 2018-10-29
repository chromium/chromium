// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_form_user_action.h"
#include "components/password_manager/core/browser/votes_uploader.h"

namespace autofill {
class FormStructure;
}

namespace password_manager {

class FormSaver;
class PasswordFormMetricsRecorder;
class PasswordManagerClient;
class PasswordManagerDriver;

// This class helps with filling the observed form and with saving/updating the
// stored information about it. It is aimed to replace PasswordFormManager and
// to be renamed in new Password Manager design. Details
// go/new-cpm-design-refactoring.
class NewPasswordFormManager : public PasswordFormManagerInterface,
                               public FormFetcher::Consumer {
 public:
  // TODO(crbug.com/621355): So far, |form_fetcher| can be null. In that case
  // |this| creates an instance of it itself (meant for production code). Once
  // the fetcher is shared between PasswordFormManager instances, it will be
  // required that |form_fetcher| is not null. |form_saver| is used to
  // save/update the form. |metrics_recorder| records metrics for |*this|. If
  // null a new instance will be created.
  NewPasswordFormManager(
      PasswordManagerClient* client,
      const base::WeakPtr<PasswordManagerDriver>& driver,
      const autofill::FormData& observed_form,
      FormFetcher* form_fetcher,
      std::unique_ptr<FormSaver> form_saver,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder);

  ~NewPasswordFormManager() override;

  // The upper limit on how many times Chrome will try to autofill the same
  // form.
  static constexpr int kMaxTimesAutofill = 5;

  // Compares |observed_form_| with |form| and returns true if they are the
  // same and if |driver| is the same as |driver_|.
  bool DoesManage(const autofill::FormData& form,
                  const PasswordManagerDriver* driver) const;

  // Check that |submitted_form_| is equal to |form| from the user point of
  // view. It is used for detecting that a form is reappeared after navigation
  // for success detection.
  bool IsEqualToSubmittedForm(const autofill::FormData& form) const;

  // If |submitted_form| is managed by *this (i.e. DoesManage returns true for
  // |submitted_form| and |driver|) then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  bool SetSubmittedFormIfIsManaged(const autofill::FormData& submitted_form,
                                   const PasswordManagerDriver* driver);
  bool is_submitted() { return is_submitted_; }
  void set_not_submitted() { is_submitted_ = false; }

  void set_old_parsing_result(const autofill::PasswordForm& form) {
    old_parsing_result_ = form;
  }

  // Selects from |predictions| predictions that corresponds to
  // |observed_form_|, initiates filling and stores predictions in
  // |predictions_|.
  void ProcessServerPredictions(
      const std::vector<autofill::FormStructure*>& predictions);

  // Sends fill data to the renderer.
  void Fill();

  // PasswordFormManagerForUI:
  FormFetcher* GetFormFetcher() override;
  const GURL& GetOrigin() const override;
  const std::map<base::string16, const autofill::PasswordForm*>&
  GetBestMatches() const override;
  const autofill::PasswordForm& GetPendingCredentials() const override;
  metrics_util::CredentialSourceType GetCredentialSource() override;
  PasswordFormMetricsRecorder* GetMetricsRecorder() override;
  const std::vector<const autofill::PasswordForm*>& GetBlacklistedMatches()
      const override;
  bool IsBlacklisted() const override;
  bool IsPasswordOverridden() const override;
  const autofill::PasswordForm* GetPreferredMatch() const override;

  void Save() override;
  void Update(const autofill::PasswordForm& credentials_to_update) override;
  void UpdateUsername(const base::string16& new_username) override;
  void UpdatePasswordValue(const base::string16& new_password) override;

  void OnNopeUpdateClicked() override;
  void OnNeverClicked() override;
  void OnNoInteraction(bool is_update) override;
  void PermanentlyBlacklist() override;
  void OnPasswordsRevealed() override;

  // PasswordFormManagerInterface:
  bool IsNewLogin() const override;
  bool IsPendingCredentialsPublicSuffixMatch() const override;
  void PresaveGeneratedPassword(const autofill::PasswordForm& form) override;
  void PasswordNoLongerGenerated() override;
  bool HasGeneratedPassword() const override;
  void SetGenerationPopupWasShown(bool generation_popup_was_shown,
                                  bool is_manual_generation) override;
  void SetGenerationElement(const base::string16& generation_element) override;
  bool IsPossibleChangePasswordFormWithoutUsername() const override;
  bool RetryPasswordFormPasswordUpdate() const override;
  std::vector<base::WeakPtr<PasswordManagerDriver>> GetDrivers() const override;
  const autofill::PasswordForm* GetSubmittedForm() const override;

  // Create a copy of |*this| which can be passed to the code handling
  // save-password related UI. This omits some parts of the internal data, so
  // the result is not identical to the original.
  // TODO(crbug.com/739366): Replace with translating one appropriate class into
  // another one.
  std::unique_ptr<NewPasswordFormManager> Clone();

#if defined(UNIT_TEST)
  static void set_wait_for_server_predictions_for_filling(bool value) {
    wait_for_server_predictions_for_filling_ = value;
  }

  FormSaver* form_saver() { return form_saver_.get(); }
#endif

  // TODO(https://crbug.com/831123): Remove it when the old form parsing is
  // removed.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder() {
    return metrics_recorder_;
  }

 protected:
  // FormFetcher::Consumer:
  void ProcessMatches(
      const std::vector<const autofill::PasswordForm*>& non_federated,
      size_t filtered_count) override;

 private:
  // Compares |parsed_form| with |old_parsing_result_| and records UKM metric.
  // TODO(https://crbug.com/831123): Remove it when the old form parsing is
  // removed.
  void RecordMetricOnCompareParsingResult(
      const autofill::PasswordForm& parsed_form);

  // Records the status of readonly fields during parsing, combined with the
  // overall success of the parsing. It reports through two different metrics,
  // depending on whether |mode| indicates parsing for saving or filling.
  void RecordMetricOnReadonly(
      FormDataParser::ReadonlyPasswordFields readonly_status,
      bool parsing_successful,
      FormDataParser::Mode mode);

  // Report the time between receiving credentials from the password store and
  // the autofill server responding to the lookup request.
  void ReportTimeBetweenStoreAndServerUMA();

  // Create pending credentials from |submitted_form_| and forms received from
  // the password store.
  void CreatePendingCredentials();

  // Create pending credentials from provisionally saved form when this form
  // represents credentials that were not previosly saved.
  void CreatePendingCredentialsForNewCredentials(
      const autofill::PasswordForm& submitted_password_form,
      const base::string16& password_element);

  // If |best_matches_| contains only one entry, then return this entry.
  // Otherwise for empty |password| return nullptr and for non-empty |password|
  // returns the any entry in |best_matches_| with the same password, if it
  // exists, and nullptr otherwise.
  const autofill::PasswordForm* FindBestMatchForUpdatePassword(
      const base::string16& password) const;

  // Try to return a member of |best_matches_| which is most likely to represent
  // the same credential as |form|. Return null if there is none. This is used
  // to tell whether the user submitted a credential filled by Chrome.
  const autofill::PasswordForm* FindBestSavedMatch(
      const autofill::PasswordForm* form) const;

  // Sets |user_action_| and records some metrics.
  void SetUserAction(UserAction user_action);

  void SetPasswordOverridden(bool password_overridden) {
    password_overridden_ = password_overridden;
    votes_uploader_.set_password_overridden(password_overridden);
  }

  // Helper for Save in the case there is at least one match for the pending
  // credentials. This sends needed signals to the autofill server, and also
  // triggers some UMA reporting.
  void ProcessUpdate();

  // Goes through |not_best_matches_|, updates the password of those which share
  // the old password and username with |pending_credentials_| to the new
  // password of |pending_credentials_|, and returns copies of all such modified
  // credentials.
  std::vector<autofill::PasswordForm> FindOtherCredentialsToUpdate();

  // Helper function for calling form parsing and logging results if logging is
  // active.
  std::unique_ptr<autofill::PasswordForm> ParseFormAndMakeLogging(
      const autofill::FormData& form,
      FormDataParser::Mode mode);

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  base::WeakPtr<PasswordManagerDriver> driver_;

  const autofill::FormData observed_form_;

  // Set of nonblacklisted PasswordForms from the DB that best match the form
  // being managed by |this|, indexed by username. The PasswordForms are owned
  // by |form_fetcher_|.
  std::map<base::string16, const autofill::PasswordForm*> best_matches_;

  // Set of forms from PasswordStore that correspond to the current site and
  // that are not in |best_matches_|. They are owned by |form_fetcher_|.
  // It is leftover from the old PasswordFormManager.
  // TODO(https://crbug.com/831123): update all places where it is used with
  // saved credentials from |form_fetcher_|.
  std::vector<const autofill::PasswordForm*> not_best_matches_;

  // Set of blacklisted forms from the PasswordStore that best match the current
  // form. They are owned by |form_fetcher_|.
  std::vector<const autofill::PasswordForm*> blacklisted_matches_;

  // If the observed form gets blacklisted through |this|, the blacklist entry
  // gets stored in |new_blacklisted_| until data is potentially refreshed by
  // reading from PasswordStore again. |blacklisted_matches_| will contain
  // |new_blacklisted_.get()| in that case. The PasswordForm will usually get
  // accessed via |blacklisted_matches_|, this unique_ptr is only used to store
  // it (unlike the rest of forms being pointed to in |blacklisted_matches_|,
  // which are owned by |form_fetcher_|).
  std::unique_ptr<autofill::PasswordForm> new_blacklisted_;

  // Convenience pointer to entry in best_matches_ that is marked
  // as preferred. This is only allowed to be null if there are no best matches
  // at all, since there will always be one preferred login when there are
  // multiple matches (when first saved, a login is marked preferred).
  const autofill::PasswordForm* preferred_match_ = nullptr;

  // Takes care of recording metrics and events for |*this|.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // When not null, then this is the object which |form_fetcher_| points to.
  std::unique_ptr<FormFetcher> owned_form_fetcher_;

  // FormFetcher instance which owns the login data from PasswordStore.
  FormFetcher* form_fetcher_;

  // FormSaver instance used by |this| to all tasks related to storing
  // credentials.
  const std::unique_ptr<FormSaver> form_saver_;

  VotesUploader votes_uploader_;

  // |is_submitted_| = true means that a submission of the managed form was seen
  // and then |submitted_form_| contains the submitted form.
  bool is_submitted_ = false;
  autofill::FormData submitted_form_;
  std::unique_ptr<autofill::PasswordForm> parsed_submitted_form_;

  // Stores updated credentials when the form was submitted but success is still
  // unknown. This variable contains credentials that are ready to be written
  // (saved or updated) to a password store. It is calculated based on
  // |submitted_form_| and |best_matches_|.
  autofill::PasswordForm pending_credentials_;

  // Whether |pending_credentials_| stores a new login or is an update to an
  // existing one.
  bool is_new_login_ = true;

  // Whether this form has an auto generated password.
  bool has_generated_password_ = false;

  // Whether the saved password was overridden.
  bool password_overridden_ = false;

  // A form is considered to be "retry" password if it has only one field which
  // is a current password field.
  // This variable is true if the password passed through ProvisionallySave() is
  // a password that is not part of any password form stored for this origin
  // and it was entered on a retry password form.
  bool retry_password_form_password_update_ = false;

  // Records the action the user has taken while interacting with the password
  // form.
  UserAction user_action_ = UserAction::kNone;

  // If Chrome has already autofilled a few times, it is probable that autofill
  // is triggered by programmatic changes in the page. We set a maximum number
  // of times that Chrome will autofill to avoid being stuck in an infinite
  // loop.
  int autofills_left_ = kMaxTimesAutofill;

  // Controls whether to wait or not server before filling. It is used in tests.
  static bool wait_for_server_predictions_for_filling_;

  // Used for comparison metrics.
  // TODO(https://crbug.com/831123): Remove it when the old form parsing is
  // removed.
  autofill::PasswordForm old_parsing_result_;

  // Time when stored credentials are received from the store. Used for metrics.
  base::TimeTicks received_stored_credentials_time_;

  // Used to transform FormData into PasswordForms.
  FormDataParser parser_;

  base::WeakPtrFactory<NewPasswordFormManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewPasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_
