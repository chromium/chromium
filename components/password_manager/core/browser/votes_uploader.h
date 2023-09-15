// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTES_UPLOADER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTES_UPLOADER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
class AutofillField;
struct FormData;
class FormStructure;
}  // namespace autofill

namespace password_manager {

class PasswordManagerClient;

// Map from a field's renderer id to a field type.
using FieldTypeMap =
    std::map<autofill::FieldRendererId, autofill::ServerFieldType>;
// A map from field's renderer id to a vote type (e.g. CREDENTIALS_REUSED).
using VoteTypeMap = std::map<autofill::FieldRendererId,
                             autofill::AutofillUploadContents::Field::VoteType>;

// Contains information for sending a SINGLE_USERNAME vote.
struct SingleUsernameVoteData {
  SingleUsernameVoteData();
  SingleUsernameVoteData(
      autofill::FieldRendererId renderer_id,
      const std::u16string& username_value,
      const FormPredictions& form_predictions,
      const std::vector<const PasswordForm*>& stored_credentials,
      bool password_form_had_matching_username);
  SingleUsernameVoteData(const SingleUsernameVoteData&);
  SingleUsernameVoteData& operator=(const SingleUsernameVoteData&);
  SingleUsernameVoteData(SingleUsernameVoteData&& other);
  ~SingleUsernameVoteData();

  // Renderer id of an input element, for which the SINGLE_USERNAME vote will be
  // sent.
  autofill::FieldRendererId renderer_id;

  // Value of the single username candidate field.
  std::u16string username_candidate_value;

  // Predictions for the form which contains a field with |renderer_id|.
  FormPredictions form_predictions;

  // Type of the value seen in the single username candidate field.
  autofill::AutofillUploadContents::ValueType value_type;

  // Information about username edits in a save/update prompt. Not calculated on
  // Android, because it's not possible to edit credentials in prompts on
  // Android.
  autofill::AutofillUploadContents::SingleUsernamePromptEdit prompt_edit;

  // True if the password form has username field whose value matches username
  // value in the single username form.
  // TODO: crbug.com/1468297 - `password_form_had_matching_username` in
  // `VotesUploader` is used for UMA metrics. The variable can be removed once
  // the metrics are not needed anymore.
  bool password_form_had_matching_username;
};

// This class manages vote uploads for password forms.
class VotesUploader {
 public:
  // The states a changed username can be in.
  enum class UsernameChangeState {
    // The user did not change the username.
    kUnchanged,
    // The user changed the username to a different value that was present in
    // the submitted form. For example, via the dropdown in the Desktop bubble.
    kChangedToKnownValue,
    // The user changed the username to a different value that was not present
    // in the submitted form. For example, via the text field in the Desktop
    // bubble.
    kChangedToUnknownValue,
  };

  // Whether two existing single username detection mechanisms (UFF and FPF)
  // have data, overlapping or not. Used for UMA recording, must be in sync with
  // SingleUsernameVoteDataAvailability in "tools/metrics/histograms/enums.xml".
  enum class SingleUsernameVoteDataAvailability {
    // No data available for both UFF and FPF voting.
    kNone = 0,
    // Data only available for single username voting.
    kUsernameFirstOnly = 1,
    // Data only available for forgot password voting.
    kForgotPasswordOnly = 2,
    // Data available for both UFF and FPF voting, but these votes are for
    // different fields.
    kBothNoOverlap = 3,
    // Data available for both UFF and FPF voting, forgot password voting
    // data contains data for UFF voting.
    kBothWithOverlap = 4,
    kMaxValue = kBothWithOverlap
  };

  VotesUploader(PasswordManagerClient* client,
                bool is_possible_change_password_form);
  VotesUploader(const VotesUploader& other);
  ~VotesUploader();

  // Send appropriate votes based on what is currently being saved.
  void SendVotesOnSave(const autofill::FormData& observed,
                       const PasswordForm& submitted_form,
                       const std::vector<const PasswordForm*>& best_matches,
                       PasswordForm* pending_credentials);

  // Check to see if |pending| corresponds to an account creation form. If we
  // think that it does, we label it as such and upload this state to the
  // Autofill server to vote for the correct username field, and also so that
  // we will trigger password generation in the future. This function will
  // update generation_upload_status of |pending| if an upload is performed.
  void SendVoteOnCredentialsReuse(const autofill::FormData& observed,
                                  const PasswordForm& submitted_form,
                                  PasswordForm* pending);

  // Tries to set all votes (e.g. autofill field types, generation vote) to
  // a |FormStructure| and upload it to the server. Returns true on success.
  bool UploadPasswordVote(const PasswordForm& form_to_upload,
                          const PasswordForm& submitted_form,
                          const autofill::ServerFieldType password_type,
                          const std::string& login_form_signature);

  // Sends USERNAME and PASSWORD votes, when a credential is used to login for
  // the first time. |form_to_upload| is the submitted login form.
  void UploadFirstLoginVotes(
      const std::vector<const PasswordForm*>& best_matches,
      const PasswordForm& pending_credentials,
      const PasswordForm& form_to_upload);

  // Searches for |username| in |all_alternative_usernames| of |matches|. If the
  // username value is found in |all_alternative_usernames| and the password
  // value of the match is equal to |password|, the match is saved to
  // |username_correction_vote_| and the method returns true.
  bool FindCorrectedUsernameElement(
      const std::vector<const PasswordForm*>& matches,
      const std::u16string& username,
      const std::u16string& password);

  // Generates a password attributes vote based on |password_value| and saves it
  // to |form_structure|. Declared as public for testing.
  void GeneratePasswordAttributesVote(const std::u16string& password_value,
                                      autofill::FormStructure* form_structure);

  // Stores the |unique_renderer_id| and |values| of the fields in
  // |observed_form| to |initial_field_values_|.
  void StoreInitialFieldValues(const autofill::FormData& observed_form);

  // Sets the low-entropy hash value of the values stored in |initial_values_|
  // for the detected |username_element_renderer_id| field to the corresponding
  // field in |form_structure|.
  void SetInitialHashValueOfUsernameField(
      autofill::FieldRendererId username_element_renderer_id,
      autofill::FormStructure* form_structure);

  // Sends single username vote if |single_username_vote_data_| or
  // |forgot_password_vote_data_| is set.
  // The vote sent is either SINGLE_USERNAME (if the user saved the credential
  // with the username cached in |single_username_vote_data|), or
  // SINGLE_USERNAME_FORGOT_PASSWORD (if the user saved the credential
  // with the username cached in |forgot_password_vote_data|), or NOT_USERNAME
  // (if the saved username contradicts cached potential usernames).
  // TODO (crbug.com/959776): Have a single point in code that calls this
  // method.
  void MaybeSendSingleUsernameVotes();

  // Calculate whether the username value was edited in a prompt based on
  // suggested and saved username values and whether it confirms or
  // contradicts the data about potential single username.
  void CalculateUsernamePromptEditState(const std::u16string& saved_username);

  // Cache the vote data for a form that is likely a forgot password form
  // (a form, into which the user inputs their username to start the
  // password recovery process).
  void AddForgotPasswordVoteData(const SingleUsernameVoteData& vote_data);

  void set_generation_popup_was_shown(bool generation_popup_was_shown) {
    generation_popup_was_shown_ = generation_popup_was_shown;
  }

  void set_is_manual_generation(bool is_manual_generation) {
    is_manual_generation_ = is_manual_generation;
  }

  autofill::FieldRendererId get_generation_element() const {
    return generation_element_;
  }

  void set_generation_element(autofill::FieldRendererId generation_element) {
    generation_element_ = generation_element;
  }

  void set_username_change_state(UsernameChangeState username_change_state) {
    username_change_state_ = username_change_state;
  }

  void set_has_passwords_revealed_vote(bool has_passwords_revealed_vote) {
    has_passwords_revealed_vote_ = has_passwords_revealed_vote;
  }

  void set_password_overridden(bool password_overridden) {
    password_overridden_ = password_overridden;
  }

  void set_has_generated_password(bool has_generated_password) {
    has_generated_password_ = has_generated_password;
  }

  bool generated_password_changed() const {
    return generated_password_changed_;
  }

  void set_generated_password_changed(bool generated_password_changed) {
    generated_password_changed_ = generated_password_changed;
  }

  void clear_single_username_vote_data() { single_username_vote_data_.reset(); }

  void set_single_username_vote_data(const SingleUsernameVoteData& data) {
    single_username_vote_data_ = data;
  }

  void set_suggested_username(const std::u16string& suggested_username) {
    suggested_username_ = suggested_username;
  }

#if defined(UNIT_TEST)
  const std::u16string& suggested_username() const {
    return suggested_username_;
  }
#endif

 private:
  // The outcome of the form classifier.
  enum FormClassifierOutcome {
    kNoOutcome,
    kNoGenerationElement,
    kFoundGenerationElement
  };

  // Adds a vote on password generation usage to |form_structure|.
  void AddGeneratedVote(autofill::FormStructure* form_structure);

  // Sets the known-value flag for each field, indicating that the field
  // contained a previously stored credential on submission.
  void SetKnownValueFlag(const PasswordForm& pending_credentials,
                         const std::vector<const PasswordForm*>& best_matches,
                         autofill::FormStructure* form_to_upload);

  // Searches for |username| in |all_alternative_usernames| of |match|. If the
  // username value is found, the match is saved to |username_correction_vote_|
  // and the function returns true.
  bool FindUsernameInOtherAlternativeUsernames(const PasswordForm& match,
                                            const std::u16string& username);

  bool StartUploadRequest(
      std::unique_ptr<autofill::FormStructure> form_to_upload,
      const autofill::ServerFieldTypeSet& available_field_types);

  // On username first and forgot password flows votes are uploaded both for the
  // single username form and for the single password form. This method sets the
  // data needed to upload vote on the username form. The vote is based on the
  // user interaction with the save prompt (i.e. whether the suggested value was
  // actually saved).
  bool SetSingleUsernameVoteOnUsernameForm(
      autofill::AutofillField* field,
      const SingleUsernameVoteData& single_username,
      autofill::ServerFieldTypeSet* available_field_types,
      autofill::FormSignature form_signature,
      bool is_forgot_password_vote);

  // On username first flow votes are uploaded both for the single username form
  // and for the single password form. This method sets the data needed to
  // upload vote on the password form. The vote is based on whether there was
  // a username form that preceded the password form, and on the type of user
  // input it had (e.g. email-like, phone-like, arbitrary string).
  void SetSingleUsernameVoteOnPasswordForm(
      const SingleUsernameVoteData& vote_data,
      autofill::FormStructure& form_structure);

  // Calculates whether the |saved_username| (the value actually saved in the
  // Password Manager) confirms or contradicts |potential_username| (Password
  // Manager's guess based on preceding text fields that the user has interacted
  // with).
  autofill::AutofillUploadContents::SingleUsernamePromptEdit
  CalculateUsernamePromptEdit(const std::u16string& saved_username,
                              const std::u16string& potential_username);

  // Attempts to send a vote for a single username form.
  // `is_forgot_password_form` specifies whether the form is considered to be a
  // part of a username first or a forgot password flow:
  // 1) When it's true, SINGLE_USERNAME_FORGOT_PASSWORD & NOT_USERNAME can be
  // sent.
  // 2) When it's false, SINGLE_USERNAME & NOT_USERNAME votes can be sent.
  // Returns true if the vote is sent.
  bool MaybeSendSingleUsernameVote(
      const SingleUsernameVoteData& single_username,
      const FormPredictions& predictions,
      bool is_forgot_password_vote);

  // The client which implements embedder-specific PasswordManager operations.
  raw_ptr<PasswordManagerClient> client_ = nullptr;

  // Whether generation popup was shown at least once.
  bool generation_popup_was_shown_ = false;

  // Whether password generation was manually triggered.
  bool is_manual_generation_ = false;

  // A password field name that is used for generation.
  autofill::FieldRendererId generation_element_;

  // Captures whether the user changed the username to a known value, an unknown
  // value, or didn't change the username at all.
  UsernameChangeState username_change_state_ = UsernameChangeState::kUnchanged;

  // If the user typed username that doesn't match any saved credentials, but
  // matches an entry from |all_alternative_usernames| of a saved credential,
  // |username_correction_vote_| stores the credential with matched username.
  // The matched credential is copied to |username_correction_vote_|, but
  // |username_correction_vote_.username_element| is set to the name of the
  // field where the matched username was found.
  absl::optional<PasswordForm> username_correction_vote_;

  // Whether the password values have been shown to the user on the save prompt.
  bool has_passwords_revealed_vote_ = false;

  // Whether the saved password was overridden.
  bool password_overridden_ = false;

  // True if the observed form of owning PasswordFormManager is considered to be
  // change password form.
  bool is_possible_change_password_form_ = false;

  // Whether this form has an auto generated password.
  bool has_generated_password_ = false;

  // Whether this form has a generated password changed by user.
  bool generated_password_changed_ = false;

  // Maps a unique renderer ID to the initial value of the fields of an
  // observed form.
  std::map<autofill::FieldRendererId, std::u16string> initial_values_;

  // The data for voting on potential single username form for the username
  // first flow. Populated when the password form, that follows single username
  // form, is submitted.
  absl::optional<SingleUsernameVoteData> single_username_vote_data_;

  // The username that is suggested in a save/update prompt. The user might
  // modify it in the prompt before saving.
  std::u16string suggested_username_;

  // The data for voting on potential forgot password forms (forms, into
  // which the user inputs their username to start the password recovery
  // process). These forms do not contain password fields, so voting requires
  // the same information as for single username forms for username first flow.
  // Populated when the password reset form, that follows the forgot password
  // form, is submitted.
  std::map<autofill::FieldRendererId, SingleUsernameVoteData>
      forgot_password_vote_data_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTES_UPLOADER_H_
