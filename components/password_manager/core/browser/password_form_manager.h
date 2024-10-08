// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_form_prediction_waiter.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_save_manager.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class ElapsedTimer;
}

namespace password_manager {

class PasswordFormMetricsRecorder;
class PasswordManagerClient;
class PasswordManagerDriver;
struct PossibleUsernameData;

using FormOrDigest = absl::variant<autofill::FormData, PasswordFormDigest>;

// This class helps with filling the observed form and with saving/updating the
// stored information about it.
class PasswordFormManager : public PasswordFormManagerForUI,
                            public PasswordFormPredictionWaiter::Client,
                            public FormFetcher::Consumer {
 public:
  // TODO(crbug.com/41259715): So far, |form_fetcher| can be null. In that case
  // |this| creates an instance of it itself (meant for production code). Once
  // the fetcher is shared between PasswordFormManager instances, it will be
  // required that |form_fetcher| is not null. |form_saver| is used to
  // save/update the form. |metrics_recorder| records metrics for |*this|. If
  // null a new instance will be created.
  PasswordFormManager(
      PasswordManagerClient* client,
      const base::WeakPtr<PasswordManagerDriver>& driver,
      const autofill::FormData& observed_form_data,
      FormFetcher* form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder);

  // Constructor for http authentication (aka basic authentication).
  PasswordFormManager(
      PasswordManagerClient* client,
      PasswordFormDigest observed_http_auth_digest,
      FormFetcher* form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager);

  PasswordFormManager(const PasswordFormManager&) = delete;
  PasswordFormManager& operator=(const PasswordFormManager&) = delete;

  ~PasswordFormManager() override;

  // The upper limit on how many times Chrome will try to autofill the same
  // form.
  static constexpr int kMaxTimesAutofill = 5;

  // Returns whether the form identified by |form_renderer_id| and |driver|
  // is managed by this password form manager.
  bool DoesManage(autofill::FormRendererId form_renderer_id,
                  const PasswordManagerDriver* driver) const;

  // Returns whether the form managed by this password form manager contains
  // a field identified by the `field_renderer_id`. `driver` is used to check
  // is this password form manager corresponds to the queried web frame.
  bool DoesManage(autofill::FieldRendererId field_renderer_id,
                  const PasswordManagerDriver* driver) const;

  // Check that |submitted_form_| is equal to |form| from the user point of
  // view. It is used for detecting that a form is reappeared after navigation
  // for success detection.
  bool IsEqualToSubmittedForm(const autofill::FormData& form) const;

  // If |submitted_form| is managed by *this (i.e. DoesManage returns true for
  // |submitted_form| and |driver|) then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  // In case a username is missed from the form, heuristically determines if one
  // of the |possible_usernames| outside of the form can be used as a username.
  bool ProvisionallySave(
      const autofill::FormData& submitted_form,
      const PasswordManagerDriver* driver,
      const base::LRUCache<PossibleUsernameFieldIdentifier,
                           PossibleUsernameData>& possible_usernames);

  // If |submitted_form| is managed by *this then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  bool ProvisionallySaveHttpAuthForm(const PasswordForm& submitted_form);

  bool is_submitted() { return is_submitted_; }
  void set_not_submitted() { is_submitted_ = false; }

  bool IsSavingAllowed() const { return is_saving_allowed_; }

  // Returns true if |*this| manages http authentication.
  bool IsHttpAuth() const;

  // Returns true if |*this| manages saving with Credentials API. This class is
  // not used for filling with Credentials API.
  bool IsCredentialAPISave() const;

  // Returns scheme of the observed form or http authentication.
  PasswordForm::Scheme GetScheme() const;

  // Selects from |predictions| predictions that corresponds to
  // |observed_form()|, initiates filling and stores predictions in
  // |predictions_|.
  void ProcessServerPredictions(
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  // Sends fill data to the renderer. If no server predictions exist, it
  // schedules to fill when they become available (or the wait times out).
  void Fill();

  // Updates `observed_form_or_digest_` and form predictions stored in
  // `parser_`, resets the amount of autofills left and stops the timer waiting
  // for server predictions.
  void UpdateFormManagerWithFormChanges(
      const autofill::FormData& observed_form_data,
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  void UpdateSubmissionIndicatorEvent(
      autofill::mojom::SubmissionIndicatorEvent event);

  // Sends the request to prefill the generated password or pops up an
  // additional UI in case of possible override.
  void OnGeneratedPasswordAccepted(
      autofill::FormData form_data,
      autofill::FieldRendererId generation_element_id,
      const std::u16string& password);

  // Check if the field identified by |driver_id| and |field_id| is present in
  // the |observed_form()|.
  bool ObservedFormHasField(int driver_id,
                            autofill::FieldRendererId field_id) const;

  // PasswordFormManagerForUI:
  const GURL& GetURL() const override;
  base::span<const PasswordForm> GetBestMatches() const override;
  base::span<const PasswordForm> GetFederatedMatches() const override;
  const PasswordForm& GetPendingCredentials() const override;
  metrics_util::CredentialSourceType GetCredentialSource() const override;
  PasswordFormMetricsRecorder* GetMetricsRecorder() override;
  base::span<const InteractionsStats> GetInteractionsStats() const override;
  base::span<const PasswordForm> GetInsecureCredentials() const override;
  bool IsBlocklisted() const override;
  bool IsMovableToAccountStore() const override;

  void Save() override;
  bool IsUpdateAffectingPasswordsStoredInTheGoogleAccount() const override;
  void OnUpdateUsernameFromPrompt(const std::u16string& new_username) override;
  void OnUpdatePasswordFromPrompt(const std::u16string& new_password) override;

  void OnNopeUpdateClicked() override;
  void OnNeverClicked() override;
  void OnNoInteraction(bool is_update) override;
  void Blocklist() override;
  void OnPasswordsRevealed() override;
  void MoveCredentialsToAccountStore() override;
  void BlockMovingCredentialsToAccountStore() override;
  PasswordForm::Store GetPasswordStoreForSaving(
      const PasswordForm& password_form) const override;

  bool IsNewLogin() const;
  FormFetcher* GetFormFetcher();
  void PresaveGeneratedPassword(const autofill::FormData& form_data,
                                const std::u16string& generated_password);
  void PasswordNoLongerGenerated();
  bool HasGeneratedPassword() const;
  void SetGenerationPopupWasShown(
      autofill::password_generation::PasswordGenerationType type);
  void SetGenerationElement(autofill::FieldRendererId generation_element);
  bool HasLikelyChangeOrResetFormSubmitted() const;
  bool IsPasswordUpdate() const;
  base::WeakPtr<PasswordManagerDriver> GetDriver() const;
  const PasswordForm* GetSubmittedForm() const;
  const PasswordForm* GetParsedObservedForm() const;

  // Returns the frame id of the corresponding PasswordManagerDriver. See
  // `GetFrameId()` in PasswordManagerDriver for more details.
  int GetFrameId();

#if BUILDFLAG(IS_IOS)
  // Sets a value of the field with |field_identifier| of |observed_form()|
  // to |field_value|. In case if there is a presaved credential this function
  // updates the presaved credential.
  void UpdateStateOnUserInput(autofill::FormRendererId form_id,
                              autofill::FieldRendererId field_id,
                              const std::u16string& field_value);

  void SetDriver(const base::WeakPtr<PasswordManagerDriver>& driver);

  // Copies all known field data from FieldDataManager to |observed_form()|
  // and provisionally saves the manager if the relevant data is found.
  void ProvisionallySaveFieldDataManagerInfo(
      const autofill::FieldDataManager& field_data_manager,
      const PasswordManagerDriver* driver,
      const base::LRUCache<PossibleUsernameFieldIdentifier,
                           PossibleUsernameData>& possible_usernames);

  // Checks if `this` can be inspected for submission detection after unowned
  // form fields were removed. Only to be used on formless form managers.
  bool AreRemovedUnownedFieldsValidForSubmissionDetection(
      const std::set<autofill::FieldRendererId>& removed_fields,
      const autofill::FieldDataManager& field_data_manager) const;
#endif  // BUILDFLAG(IS_IOS)

  // Create a copy of |*this| which can be passed to the code handling
  // save-password related UI. This omits some parts of the internal data, so
  // the result is not identical to the original.
  // TODO(crbug.com/41328828): Replace with translating one appropriate class
  // into another one.
  std::unique_ptr<PasswordFormManager> Clone();

  // Because of the android integration tests, it can't be guarded by if
  // defined(UNIT_TEST).
  static void DisableFillingServerPredictionsForTesting() {
    wait_for_server_predictions_for_filling_ = false;
  }

  // Returns a pointer to the observed form if possible or nullptr otherwise.
  const autofill::FormData* observed_form() const {
    return absl::get_if<autofill::FormData>(&observed_form_or_digest_);
  }

  // Saves username value from |pending_credentials_| to votes uploader.
  void SaveSuggestedUsernameValueToVotesUploader();

  // Returns true if WebAuthn credential filling is enabled and there are
  // credentials available to use.
  bool WebAuthnCredentialsAvailable() const;

#if defined(UNIT_TEST)
  static void set_wait_for_server_predictions_for_filling(bool value) {
    wait_for_server_predictions_for_filling_ = value;
  }

  FormSaver* profile_store_form_saver() const {
    return password_save_manager_->GetProfileStoreFormSaverForTesting();
  }

  const VotesUploader* votes_uploader() const {
    return votes_uploader_.has_value() ? &votes_uploader_.value() : nullptr;
  }
#endif

 protected:
  // Constructor for Credentials API.
  PasswordFormManager(
      PasswordManagerClient* client,
      std::unique_ptr<PasswordForm> saved_form,
      std::unique_ptr<FormFetcher> form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager);

  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  // PasswordFormPredictionWaiter::Client:
  void OnWaitCompleted() override;
  void OnTimeout() override;

  // Create pending credentials from |parsed_submitted_form_| and forms received
  // from the password store.
  void CreatePendingCredentials();

 private:
  // Delegating constructor.
  PasswordFormManager(
      PasswordManagerClient* client,
      FormOrDigest observed_form_or_digest,
      FormFetcher* form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder);
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

  // Sends fill data to the http auth popup.
  void FillHttpAuth();

  // Helper function for calling form parsing and logging results if logging is
  // active.
  FormParsingResult ParseFormAndMakeLogging(const autofill::FormData& form,
                                            FormDataParser::Mode mode);

  void PresaveGeneratedPasswordInternal(
      const autofill::FormData& form,
      const std::u16string& generated_password);

  // Returns a mutable pointer to the observed form if possible or nullptr
  // otherwise.
  autofill::FormData* mutable_observed_form() {
    return absl::get_if<autofill::FormData>(&observed_form_or_digest_);
  }

  // Returns a pointer to the observed digest if possible or nullptr otherwise.
  const PasswordFormDigest* observed_digest() const {
    return absl::get_if<PasswordFormDigest>(&observed_form_or_digest_);
  }

  // Calculates FillingAssistance metric for |parsed_submitted_form|.
  void CalculateFillingAssistanceMetric(
      const PasswordForm& parsed_submitted_form);

  // Calculates SubmittedPasswordFormFrame metric value (main frame, iframe,
  // etc) for |submitted_form|. The metric is recorded when the form manager is
  // destroyed.
  void CalculateSubmittedFormFrameMetric();

  // Calculates SubmittedFormType metric for |parsed_submitted_form_|. The
  // metric is recorded when the form manager is destroyed.
  void CalculateSubmittedFormTypeMetric();

  // Save/update |pending_credentials_| to the password store.
  void SavePendingToStore(bool update);

  PasswordFormDigest ConstructObservedFormDigest() const;

  // Returns whether |possible_username| data can be used in username first
  // flow.
  bool IsPossibleSingleUsernameAvailable(
      const PossibleUsernameData& possible_username) const;

  // Finds best username candidate that is outside of the form. This is done
  // according to priorities listed in `UsernameFoundOutsideOfFormType`.
  // If there are more than one field in the same category, pick the one that is
  // more recently modified by the user.
  std::optional<UsernameFoundOutsideOfForm> FindBestPossibleUsernameCandidate(
      const base::LRUCache<PossibleUsernameFieldIdentifier,
                           PossibleUsernameData>& possible_usernames);

  // Updates the predictions stored in `parser_` with predictions relevant for
  // `observed_form_or_digest_`.
  void UpdatePredictionsForObservedForm(
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  // Creates a timer to wait for server side predictions. On timeout (or on
  // receiving server side predictions), `Fill()` is triggered.
  void DelayFillForServerSidePredictions();

  // Sends fill data to the renderer immediately regardless of whether server
  // predictions are available.
  void FillNow();

  // Checks if `best_candidate` has better signal than the username
  // found inside the password form.
  bool ShouldPreferUsernameFoundOutsideOfForm(
      const std::optional<UsernameFoundOutsideOfForm>& best_candidate,
      UsernameDetectionMethod in_form_username_detection_method);

  // Sets voting data and update `parsed_submitted_form_` with the correct
  // username value for a password form without a username field.
  void HandleUsernameFirstFlow(
      const base::LRUCache<PossibleUsernameFieldIdentifier,
                           PossibleUsernameData>& possible_usernames,
      UsernameDetectionMethod in_form_username_detection_method);

  // Sets voting data for a password form that is likely a forgot password form
  // (a form, into which the user inputs their username to start the
  // password recovery process).
  void HandleForgotPasswordFormData();

  // Returns non-empty, lower case stored usernames based on `GetBestMatches()`.
  base::flat_set<std::u16string> GetStoredUsernames() const;

  // Records provisional save failure using current |client_| and
  // |main_frame_url_|.
  void RecordProvisionalSaveFailure(
      PasswordManagerMetricsRecorder::ProvisionalSaveFailure failure,
      const GURL& form_origin);

  // The client which implements embedder-specific PasswordManager operations.
  const raw_ptr<PasswordManagerClient> client_;

  base::WeakPtr<PasswordManagerDriver> driver_;

  // The frame id of |driver_|.  See `GetFrameId()` in PasswordManagerDriver for
  // more details. This is cached since |driver_| might become null when the
  // frame is deleted.
  int cached_driver_frame_id_ = 0;

  // The id of |driver_|. Cached since |driver_| might become null when the
  // frame frame is deleted.
  int driver_id_ = 0;

  // The observed form or digest. These are mutually exclusive, hence the usage
  // of a variant.
  FormOrDigest observed_form_or_digest_;

  // If the observed form gets blocklisted through |this|, we keep the
  // information in this boolean flag until data is potentially refreshed by
  // reading from PasswordStore again. Upon reading from the store again, we set
  // this boolean to false again.
  bool newly_blocklisted_ = false;

  // Takes care of recording metrics and events for |*this|.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // When not null, then this is the object which |form_fetcher_| points to.
  std::unique_ptr<FormFetcher> owned_form_fetcher_;

  // FormFetcher instance which owns the login data from PasswordStore.
  const raw_ptr<FormFetcher> form_fetcher_;

  std::unique_ptr<PasswordSaveManager> password_save_manager_;

  // Uploads crowdsourcing votes. Is not set if votes shouldn't be uploaded for
  // the observed form.
  std::optional<VotesUploader> votes_uploader_;

  // |is_submitted_| = true means that |*this| is ready for saving.
  // TODO(https://crubg.com/875768): Come up with a better name.
  bool is_submitted_ = false;
  autofill::FormData submitted_form_;
  std::unique_ptr<PasswordForm> parsed_submitted_form_;

  // The form cached after the form parsing corresponding to this form manager.
  std::unique_ptr<PasswordForm> parsed_observed_form_;

  // If Chrome has already autofilled a few times, it is probable that autofill
  // is triggered by programmatic changes in the page. We set a maximum number
  // of times that Chrome will autofill to avoid being stuck in an infinite
  // loop.
  int autofills_left_ = kMaxTimesAutofill;

  // Closure to call when server predictions are received.
  base::OnceClosure server_predictions_closure_;

  // Controls whether to wait or not server before filling. It is used in tests.
  static bool wait_for_server_predictions_for_filling_;

  // Time when stored credentials are received from the store. Used for metrics.
  base::TimeTicks received_stored_credentials_time_;

  PasswordFormPredictionWaiter async_predictions_waiter_;

  // Used to transform FormData into PasswordForms.
  FormDataParser parser_;

  // Used to indicate if password can be offered for saving.
  // The decision is captured at the provisional save time while it can be
  // already different for the landing page.
  bool is_saving_allowed_ = true;

  // Stores if Save() was called when FormFetcher was in WAITING state.
  // In that case we should schedule a Save() call, when FormFecher is ready.
  bool should_schedule_save_for_later_ = false;

  // A password field that is used for generation.
  autofill::FieldRendererId generation_element_;

  // For generating timing metrics on retrieving server-side predictions.
  std::unique_ptr<base::ElapsedTimer> server_side_predictions_timer_;
};

// Returns whether `form_data` differs from the form observed by `form_manager`
// in one of the following ways: 1) The number of form fields differ. 2) A form
// field's renderer id, name, control type, or autocomplete attribute differs.
bool HasObservedFormChanged(const autofill::FormData& form_data,
                            PasswordFormManager& form_manager);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
