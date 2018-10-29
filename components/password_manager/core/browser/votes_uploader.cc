// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <ctype.h>
#include <map>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::AutofillField;
using autofill::AutofillManager;
using autofill::AutofillUploadContents;
using autofill::FormData;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::ServerFieldType;
using autofill::ServerFieldTypeSet;
using autofill::ValueElementPair;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {
namespace {

// Sets autofill types of password and new password fields in |field_types|.
// |password_type| (the autofill type of new password field) should be equal to
// NEW_PASSWORD, PROBABLY_NEW_PASSWORD or NOT_NEW_PASSWORD. These values
// correspond to cases when the user confirmed password update, did nothing or
// declined to update password respectively.
void SetFieldLabelsOnUpdate(const ServerFieldType password_type,
                            const PasswordForm& submitted_form,
                            FieldTypeMap* field_types) {
  DCHECK(password_type == autofill::NEW_PASSWORD ||
         password_type == autofill::PROBABLY_NEW_PASSWORD ||
         password_type == autofill::NOT_NEW_PASSWORD)
      << password_type;
  if (submitted_form.new_password_element.empty())
    return;

  (*field_types)[submitted_form.password_element] = autofill::PASSWORD;
  (*field_types)[submitted_form.new_password_element] = password_type;
}

// Sets the autofill type of the password field stored in |submitted_form| to
// |password_type| in |field_types| map.
void SetFieldLabelsOnSave(const ServerFieldType password_type,
                          const PasswordForm& form,
                          FieldTypeMap* field_types) {
  DCHECK(password_type == autofill::PASSWORD ||
         password_type == autofill::ACCOUNT_CREATION_PASSWORD ||
         password_type == autofill::NOT_ACCOUNT_CREATION_PASSWORD)
      << password_type;

  if (!form.new_password_element.empty())
    (*field_types)[form.new_password_element] = password_type;
  else if (!form.password_element.empty())
    (*field_types)[form.password_element] = password_type;
}

// Label username and password fields with autofill types in |form_structure|
// based on |field_types|, and vote types based on |vote_types|. The function
// also adds the types to |available_field_types|. For fields of |USERNAME|
// type, a vote type must exist.
void LabelFields(const FieldTypeMap& field_types,
                 const VoteTypeMap& vote_types,
                 FormStructure* form_structure,
                 ServerFieldTypeSet* available_field_types) {
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);

    ServerFieldType type = autofill::UNKNOWN_TYPE;
    if (!field->name.empty()) {
      auto iter = field_types.find(field->name);
      if (iter != field_types.end()) {
        type = iter->second;
        available_field_types->insert(type);
      }

      auto vote_type_iter = vote_types.find(field->name);
      if (vote_type_iter != vote_types.end())
        field->set_vote_type(vote_type_iter->second);
      DCHECK(type != autofill::USERNAME ||
             field->vote_type() !=
                 AutofillUploadContents::Field::NO_INFORMATION);
    }

    ServerFieldTypeSet types;
    types.insert(type);
    field->set_possible_types(types);
  }
}

// Returns true iff |credentials| has the same password as an entry in |matches|
// which doesn't have a username.
bool IsAddingUsernameToExistingMatch(
    const PasswordForm& credentials,
    const std::map<base::string16, const PasswordForm*>& matches) {
  const auto match = matches.find(base::string16());
  return !credentials.username_value.empty() && match != matches.end() &&
         !match->second->is_public_suffix_match &&
         match->second->password_value == credentials.password_value;
}

// Helper functions for character type classification. The built-in functions
// depend on locale, platform and other stuff. To make the output more
// predictable, the function are re-implemented here.
bool IsNumeric(int c) {
  return '0' <= c && c <= '9';
}
bool IsLowercaseLetter(int c) {
  return 'a' <= c && c <= 'z';
}
bool IsUppercaseLetter(int c) {
  return 'A' <= c && c <= 'Z';
}
bool IsSpecialSymbol(int c) {
  return ('!' <= c && c <= '/') || (':' <= c && c <= '@') ||
         ('[' <= c && c <= '`') || ('{' <= c && c <= '~');
}

}  // namespace

VotesUploader::VotesUploader(PasswordManagerClient* client,
                             bool is_possible_change_password_form)
    : client_(client),
      is_possible_change_password_form_(is_possible_change_password_form) {}

VotesUploader::VotesUploader(const VotesUploader& other) = default;
VotesUploader::~VotesUploader() = default;

void VotesUploader::SendVotesOnSave(
    const FormData& observed,
    const PasswordForm& submitted_form,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    PasswordForm* pending_credentials) {
  if (pending_credentials->times_used == 1 ||
      IsAddingUsernameToExistingMatch(*pending_credentials, best_matches))
    UploadFirstLoginVotes(best_matches, *pending_credentials, submitted_form);

  // Upload credentials the first time they are saved. This data is used
  // by password generation to help determine account creation sites.
  // Credentials that have been previously used (e.g., PSL matches) are checked
  // to see if they are valid account creation forms.
  if (pending_credentials->times_used == 0) {
    UploadPasswordVote(*pending_credentials, submitted_form, autofill::PASSWORD,
                       std::string());
    if (has_username_correction_vote_) {
      UploadPasswordVote(username_correction_vote_, submitted_form,
                         autofill::USERNAME,
                         FormStructure(observed).FormSignatureAsStr());
    }
  } else {
    SendVoteOnCredentialsReuse(observed, submitted_form, pending_credentials);
  }
}

void VotesUploader::SendVoteOnCredentialsReuse(
    const FormData& observed,
    const PasswordForm& submitted_form,
    PasswordForm* pending) {
  // Ignore |pending_structure| if its FormData has no fields. This is to
  // weed out those credentials that were saved before FormData was added
  // to PasswordForm. Even without this check, these FormStructure's won't
  // be uploaded, but it makes it hard to see if we are encountering
  // unexpected errors.
  if (pending->form_data.fields.empty())
    return;

  FormStructure pending_structure(pending->form_data);
  FormStructure observed_structure(observed);

  if (pending_structure.form_signature() !=
      observed_structure.form_signature()) {
    // Only upload if this is the first time the password has been used.
    // Otherwise the credentials have been used on the same field before so
    // they aren't from an account creation form.
    // Also bypass uploading if the username was edited. Offering generation
    // in cases where we currently save the wrong username isn't great.
    if (pending->times_used == 1) {
      if (UploadPasswordVote(*pending, submitted_form,
                             autofill::ACCOUNT_CREATION_PASSWORD,
                             observed_structure.FormSignatureAsStr())) {
        pending->generation_upload_status = PasswordForm::POSITIVE_SIGNAL_SENT;
      }
    }
  } else if (pending->generation_upload_status ==
             PasswordForm::POSITIVE_SIGNAL_SENT) {
    // A signal was sent that this was an account creation form, but the
    // credential is now being used on the same form again. This cancels out
    // the previous vote.
    if (UploadPasswordVote(*pending, submitted_form,
                           autofill::NOT_ACCOUNT_CREATION_PASSWORD,
                           std::string())) {
      pending->generation_upload_status = PasswordForm::NEGATIVE_SIGNAL_SENT;
    }
  } else if (generation_popup_was_shown_) {
    // Even if there is no autofill vote to be sent, send the vote about the
    // usage of the generation popup.
    UploadPasswordVote(*pending, submitted_form, autofill::UNKNOWN_TYPE,
                       std::string());
  }
}

bool VotesUploader::UploadPasswordVote(
    const PasswordForm& form_to_upload,
    const PasswordForm& submitted_form,
    const ServerFieldType autofill_type,
    const std::string& login_form_signature) {
  // Check if there is any vote to be sent.
  bool has_autofill_vote = autofill_type != autofill::UNKNOWN_TYPE;
  bool has_password_generation_vote = generation_popup_was_shown_;
  if (!has_autofill_vote && !has_password_generation_vote)
    return false;

  if (form_to_upload.form_data.fields.empty()) {
    // List of fields may be empty in tests.
    return false;
  }

  AutofillManager* autofill_manager = client_->GetAutofillManagerForMainFrame();
  if (!autofill_manager || !autofill_manager->download_manager())
    return false;

  // If this is an update, a vote about the observed form is sent. If the user
  // re-uses credentials, a vote about the saved form is sent. If the user saves
  // credentials, the observed and pending forms are the same.
  FormStructure form_structure(form_to_upload.form_data);
  form_structure.set_submission_event(submitted_form.submission_event);

  ServerFieldTypeSet available_field_types;
  // A map from field names to field types.
  FieldTypeMap field_types;
  auto username_vote_type = AutofillUploadContents::Field::NO_INFORMATION;
  if (autofill_type != autofill::USERNAME) {
    if (has_autofill_vote) {
      bool is_update = autofill_type == autofill::NEW_PASSWORD ||
                       autofill_type == autofill::PROBABLY_NEW_PASSWORD ||
                       autofill_type == autofill::NOT_NEW_PASSWORD;

      if (is_update) {
        if (form_to_upload.new_password_element.empty())
          return false;
        SetFieldLabelsOnUpdate(autofill_type, form_to_upload, &field_types);
      } else {  // Saving.
        SetFieldLabelsOnSave(autofill_type, form_to_upload, &field_types);
      }
      if (autofill_type != autofill::ACCOUNT_CREATION_PASSWORD) {
        // If |autofill_type| == autofill::ACCOUNT_CREATION_PASSWORD, Chrome
        // will upload a vote for another form: the one that the credential was
        // saved on.
        field_types[submitted_form.confirmation_password_element] =
            autofill::CONFIRMATION_PASSWORD;
        form_structure.set_passwords_were_revealed(
            has_passwords_revealed_vote_);
      }
    }
    if (autofill_type != autofill::ACCOUNT_CREATION_PASSWORD) {
      if (generation_popup_was_shown_)
        AddGeneratedVote(&form_structure);
      if (has_username_edited_vote_) {
        field_types[form_to_upload.username_element] = autofill::USERNAME;
        username_vote_type = AutofillUploadContents::Field::USERNAME_EDITED;
      }
    } else {  // User reuses credentials.
      // If the saved username value was used, then send a confirmation vote for
      // username.
      if (!submitted_form.username_value.empty()) {
        DCHECK(submitted_form.username_value == form_to_upload.username_value);
        field_types[form_to_upload.username_element] = autofill::USERNAME;
        username_vote_type = AutofillUploadContents::Field::CREDENTIALS_REUSED;
      }
    }
    if (autofill_type == autofill::PASSWORD) {
      // The password attributes should be uploaded only on the first save.
      DCHECK(form_to_upload.times_used == 0);
      GeneratePasswordAttributesVote(form_to_upload.password_value,
                                     &form_structure);
    }
  } else {  // User overwrites username.
    field_types[form_to_upload.username_element] = autofill::USERNAME;
    field_types[form_to_upload.password_element] =
        autofill::ACCOUNT_CREATION_PASSWORD;
    username_vote_type = AutofillUploadContents::Field::USERNAME_OVERWRITTEN;
  }
  LabelFields(field_types,
              {{form_to_upload.username_element, username_vote_type}},
              &form_structure, &available_field_types);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_FORM_VOTES, form_structure);
  }

  bool success = autofill_manager->download_manager()->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      login_form_signature, true /* observed_submission */,
      nullptr /* prefs */);

  UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.UploadStarted", success);
  return success;
}

// TODO(crbug.com/840384): Share common code with UploadPasswordVote.
void VotesUploader::UploadFirstLoginVotes(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm& pending_credentials,
    const PasswordForm& form_to_upload) {
  AutofillManager* autofill_manager = client_->GetAutofillManagerForMainFrame();
  if (!autofill_manager || !autofill_manager->download_manager())
    return;

  if (form_to_upload.form_data.fields.empty()) {
    // List of fields may be empty in tests.
    return;
  }

  FormStructure form_structure(form_to_upload.form_data);
  form_structure.set_submission_event(form_to_upload.submission_event);

  FieldTypeMap field_types = {
      {form_to_upload.username_element, autofill::USERNAME}};
  VoteTypeMap vote_types = {{form_to_upload.username_element,
                             AutofillUploadContents::Field::FIRST_USE}};
  if (!password_overridden_) {
    field_types[form_to_upload.password_element] = autofill::PASSWORD;
    vote_types[form_to_upload.password_element] =
        AutofillUploadContents::Field::FIRST_USE;
  }

  ServerFieldTypeSet available_field_types;
  LabelFields(field_types, vote_types, &form_structure, &available_field_types);
  SetKnownValueFlag(pending_credentials, best_matches, &form_structure);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_FORM_VOTES, form_structure);
  }

  autofill_manager->download_manager()->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      std::string(), true /* observed_submission */, nullptr /* prefs */);
}

void VotesUploader::AddGeneratedVote(FormStructure* form_structure) {
  DCHECK(form_structure);
  DCHECK(generation_popup_was_shown_);

  if (generation_element_.empty())
    return;

  AutofillUploadContents::Field::PasswordGenerationType type =
      AutofillUploadContents::Field::NO_GENERATION;
  if (has_generated_password_) {
    UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.IsTriggeredManually",
                          is_manual_generation_);
    if (is_manual_generation_) {
      type = is_possible_change_password_form_
                 ? AutofillUploadContents::Field::
                       MANUALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM
                 : AutofillUploadContents::Field::
                       MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
    } else {
      type =
          is_possible_change_password_form_
              ? AutofillUploadContents::Field::
                    AUTOMATICALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM
              : AutofillUploadContents::Field::
                    AUTOMATICALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
    }
  } else
    type = AutofillUploadContents::Field::IGNORED_GENERATION_POPUP;

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);
    if (field->name == generation_element_) {
      field->set_generation_type(type);
      if (has_generated_password_) {
        field->set_generated_password_changed(generated_password_changed_);
        UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.GeneratedPasswordWasEdited",
                              generated_password_changed_);
      }
      break;
    }
  }
}

void VotesUploader::SetKnownValueFlag(
    const PasswordForm& pending_credentials,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    FormStructure* form) {
  DCHECK(!password_overridden_ ||
         best_matches.find(pending_credentials.username_value) !=
             best_matches.end())
      << "The credential is being overriden, but it does not exist in "
         "the best matches.";

  const base::string16& known_username = pending_credentials.username_value;
  // If we are updating a password, the known value is the old password, not
  // the new one.
  const base::string16& known_password =
      password_overridden_ ? best_matches.at(known_username)->password_value
                           : pending_credentials.password_value;

  for (auto& field : *form) {
    if (field->value.empty())
      continue;
    if (known_username == field->value || known_password == field->value) {
      field->properties_mask |= autofill::FieldPropertiesFlags::KNOWN_VALUE;
    }
  }
}

bool VotesUploader::FindUsernameInOtherPossibleUsernames(
    const PasswordForm& match,
    const base::string16& username) {
  DCHECK(!has_username_correction_vote_);

  for (const ValueElementPair& pair : match.other_possible_usernames) {
    if (pair.first == username) {
      username_correction_vote_ = match;
      username_correction_vote_.username_element = pair.second;
      has_username_correction_vote_ = true;
      return true;
    }
  }
  return false;
}

bool VotesUploader::FindCorrectedUsernameElement(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<const PasswordForm*>& not_best_matches,
    const base::string16& username,
    const base::string16& password) {
  if (username.empty())
    return false;
  for (const auto& key_value : best_matches) {
    const PasswordForm* match = key_value.second;
    if ((match->password_value == password) &&
        FindUsernameInOtherPossibleUsernames(*match, username))
      return true;
  }
  for (const PasswordForm* match : not_best_matches) {
    if ((match->password_value == password) &&
        FindUsernameInOtherPossibleUsernames(*match, username))
      return true;
  }
  return false;
}

void VotesUploader::GeneratePasswordAttributesVote(
    const base::string16& password_value,
    FormStructure* form_structure) {
  // Don't crowdsource password attributes for non-ascii passwords.
  for (const auto& e : password_value)
    if (!(IsUppercaseLetter(e) || IsLowercaseLetter(e) || IsNumeric(e) ||
          IsSpecialSymbol(e)))
      return;

  // Select a character class attribute to upload.
  int bucket = base::RandGenerator(9);
  bool (*predicate)(int c) = nullptr;
  autofill::PasswordAttribute character_class_attribute =
      autofill::PasswordAttribute::kHasSpecialSymbol;
  if (bucket == 0) {
    predicate = &IsLowercaseLetter;
    character_class_attribute =
        autofill::PasswordAttribute::kHasLowercaseLetter;
  } else if (bucket == 1) {
    predicate = &IsUppercaseLetter;
    character_class_attribute =
        autofill::PasswordAttribute::kHasUppercaseLetter;
  } else if (bucket == 2) {
    predicate = &IsNumeric;
    character_class_attribute = autofill::PasswordAttribute::kHasNumeric;
  } else {  //  3 <= bucket < 9
    // Upload symbols more often as 2/3rd of issues are because of missing
    // special symbols.
    predicate = &IsSpecialSymbol;
    character_class_attribute = autofill::PasswordAttribute::kHasSpecialSymbol;
  }
  bool actual_value_for_character_class =
      std::any_of(password_value.begin(), password_value.end(), predicate);

  // Apply the randomized response technique to noisify the actual value
  // (https://en.wikipedia.org/wiki/Randomized_response).
  bool randomized_value_for_character_class =
      base::RandGenerator(2) ? actual_value_for_character_class
                             : base::RandGenerator(2);
  form_structure->set_password_attributes_vote(std::make_pair(
      character_class_attribute, randomized_value_for_character_class));

  size_t actual_length = password_value.size();
  size_t randomized_length = actual_length <= 1 || base::RandGenerator(5) == 0
                                 ? actual_length
                                 : base::RandGenerator(actual_length - 1) + 1;
  form_structure->set_password_length_vote(randomized_length);
}

}  // namespace password_manager
