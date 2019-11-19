// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTES_UPLOADER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTES_UPLOADER_H_

#include <map>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

namespace autofill {
struct FormData;
class FormStructure;
}  // namespace autofill

namespace password_manager {

class PasswordManagerClient;

// A map from field names to field types.
using FieldTypeMap = std::map<base::string16, autofill::ServerFieldType>;
// A map from field names to field vote types.
using VoteTypeMap =
    std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>;

// Contains information for sending a SINGLE_USERNAME vote.
struct SingleUsernameVoteData {
  SingleUsernameVoteData(uint32_t renderer_id,
                         const FormPredictions& form_predictions);
  SingleUsernameVoteData(const SingleUsernameVoteData&);
  SingleUsernameVoteData& operator=(const SingleUsernameVoteData&);
  SingleUsernameVoteData(SingleUsernameVoteData&& other);
  ~SingleUsernameVoteData();

  // Renderer id of an input element, for which the SINGLE_USERNAME vote will be
  // sent.
  uint32_t renderer_id;

  // Predictions for the form which contains a field with |renderer_id|.
  FormPredictions form_predictions;
};

// This class manages vote uploads for password forms.
class VotesUploader {
 public:
  VotesUploader(PasswordManagerClient* client,
                bool is_possible_change_password_form);
  VotesUploader(const VotesUploader& other);
  ~VotesUploader();

  // Send appropriate votes based on what is currently being saved.
  void SendVotesOnSave(
      const autofill::FormData& observed,
      const autofill::PasswordForm& submitted_form,
      const std::vector<const autofill::PasswordForm*>& best_matches,
      autofill::PasswordForm* pending_credentials);

  // Check to see if |pending| corresponds to an account creation form. If we
  // think that it does, we label it as such and upload this state to the
  // Autofill server to vote for the correct username field, and also so that
  // we will trigger password generation in the future. This function will
  // update generation_upload_status of |pending| if an upload is performed.
  void SendVoteOnCredentialsReuse(const autofill::FormData& observed,
                                  const autofill::PasswordForm& submitted_form,
                                  autofill::PasswordForm* pending);

  // Tries to set all votes (e.g. autofill field types, generation vote) to
  // a |FormStructure| and upload it to the server. Returns true on success.
  bool UploadPasswordVote(const autofill::PasswordForm& form_to_upload,
                          const autofill::PasswordForm& submitted_form,
                          const autofill::ServerFieldType password_type,
                          const std::string& login_form_signature);

  // Sends USERNAME and PASSWORD votes, when a credential is used to login for
  // the first time. |form_to_upload| is the submitted login form.
  void UploadFirstLoginVotes(
      const std::vector<const autofill::PasswordForm*>& best_matches,
      const autofill::PasswordForm& pending_credentials,
      const autofill::PasswordForm& form_to_upload);

  // Searches for |username| in |all_possible_usernames| of |matches|. If the
  // username value is found in |all_possible_usernames| and the password value
  // of the match is equal to |password|, the match is saved to
  // |username_correction_vote_| and the method returns true.
  bool FindCorrectedUsernameElement(
      const std::vector<const autofill::PasswordForm*>& matches,
      const base::string16& username,
      const base::string16& password);

  // Generates a password attributes vote based on |password_value| and saves it
  // to |form_structure|. Declared as public for testing.
  void GeneratePasswordAttributesVote(const base::string16& password_value,
                                      autofill::FormStructure* form_structure);

  // Stores the |unique_renderer_id| and |values| of the fields in
  // |observed_form| to |initial_field_values_|.
  void StoreInitialFieldValues(const autofill::FormData& observed_form);

  // Sets the low-entropy hash value of the values stored in |initial_values_|
  // for the detected |username| field to the corresponding field in
  // |form_structure|.
  void SetInitialHashValueOfUsernameField(
      uint32_t username_element_renderer_id,
      autofill::FormStructure* form_structure);

  // Sends single a username vote if |single_username_vote_data_| is set.
  // |credentials_saved| equals true if credentials with a single username form
  // were saved, false if they were not saved.
  // If |single_username_vote_data| is set, the vote sent is either
  // SINGLE_USERNAME (if the user saved the credential with the username
  // captured from |single_username_vote_data|) or NOT_USERNAME (if the user did
  // not save the credential or modified the username).
  void MaybeSendSingleUsernameVote(bool credentials_saved);

  void set_generation_popup_was_shown(bool generation_popup_was_shown) {
    generation_popup_was_shown_ = generation_popup_was_shown;
  }

  void set_is_manual_generation(bool is_manual_generation) {
    is_manual_generation_ = is_manual_generation;
  }

  const base::string16& get_generation_element() const {
    return generation_element_;
  }

  void set_generation_element(const base::string16& generation_element) {
    generation_element_ = generation_element;
  }

  void set_has_username_edited_vote(bool has_username_edited_vote) {
    has_username_edited_vote_ = has_username_edited_vote;
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

  void set_single_username_vote_data(int renderer_id,
                                     const FormPredictions& form_predictions) {
    single_username_vote_data_.emplace(renderer_id, form_predictions);
  }

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
  void SetKnownValueFlag(
      const autofill::PasswordForm& pending_credentials,
      const std::vector<const autofill::PasswordForm*>& best_matches,
      autofill::FormStructure* form_to_upload);

  // Searches for |username| in |all_possible_usernames| of |match|. If the
  // username value is found, the match is saved to |username_correction_vote_|
  // and the function returns true.
  bool FindUsernameInOtherPossibleUsernames(const autofill::PasswordForm& match,
                                            const base::string16& username);

  bool StartUploadRequest(
      std::unique_ptr<autofill::FormStructure> form_to_upload,
      const autofill::ServerFieldTypeSet& available_field_types);

  // Save a vote |field_type| for a field with |field_signature| from a form
  // with |form_signature| to FieldInfoManager.
  void SaveFieldVote(uint64_t form_signature,
                     uint32_t field_signature,
                     autofill::ServerFieldType field_type);

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  // Whether generation popup was shown at least once.
  bool generation_popup_was_shown_ = false;

  // Whether password generation was manually triggered.
  bool is_manual_generation_ = false;

  // A password field name that is used for generation.
  base::string16 generation_element_;

  // True iff a user edited the username value in a prompt and new username is
  // the value of another field of the observed form.
  bool has_username_edited_vote_ = false;

  // If the user typed username that doesn't match any saved credentials, but
  // matches an entry from |all_possible_usernames| of a saved credential,
  // |username_correction_vote_| stores the credential with matched username.
  // The matched credential is copied to |username_correction_vote_|, but
  // |username_correction_vote_.username_element| is set to the name of the
  // field where the matched username was found.
  base::Optional<autofill::PasswordForm> username_correction_vote_;

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

  // Maps an |unique_renderer_id| to the initial value of the fields of an
  // observed form.
  std::map<uint32_t, base::string16> initial_values_;

  base::Optional<SingleUsernameVoteData> single_username_vote_data_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTES_UPLOADER_H_
