// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <utility>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::AutofillDownloadManager;
using autofill::AutofillField;
using autofill::AutofillUploadContents;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::RandomizedEncoder;
using autofill::ServerFieldType;
using autofill::ServerFieldTypeSet;
using autofill::ValueElementPair;
using password_manager_util::FindFormByUsername;

using Logger = autofill::SavePasswordProgressLogger;
using StringID = autofill::SavePasswordProgressLogger::StringID;

namespace password_manager {

namespace {
// Number of distinct low-entropy hash values.
constexpr uint32_t kNumberOfLowEntropyHashValues = 64;

// Contains all special symbols considered for password-generation.
constexpr char kSpecialSymbols[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

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
      if (vote_type_iter != vote_types.end()) {
        field->set_vote_type(vote_type_iter->second);
      }
      DCHECK(type != autofill::USERNAME ||
             field->vote_type() !=
                 AutofillUploadContents::Field::NO_INFORMATION);
    }
    ServerFieldTypeSet types;
    types.insert(type);
    field->set_possible_types(types);
  }
}

// Returns true if |credentials| has the same password as an entry in |matches|
// which doesn't have a username.
bool IsAddingUsernameToExistingMatch(
    const PasswordForm& credentials,
    const std::vector<const PasswordForm*>& matches) {
  if (credentials.username_value.empty())
    return false;
  const PasswordForm* match = FindFormByUsername(matches, base::string16());
  return match && !match->is_public_suffix_match &&
         match->password_value == credentials.password_value;
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

// Checks if a supplied character |c| is a special symbol.
// Special symbols are defined by the string |kSpecialSymbols|.
bool IsSpecialSymbol(int c) {
  return std::find(std::begin(kSpecialSymbols), std::end(kSpecialSymbols), c) !=
         std::end(kSpecialSymbols);
}

// Returns a uniformly distributed random symbol from the set of random symbols
// defined by the string |kSpecialSymbols|.
int GetRandomSpecialSymbol() {
  return kSpecialSymbols[base::RandGenerator(base::size(kSpecialSymbols))];
}

// Returns a random special symbol used in |password|.
// It is expected that |password| contains at least one special symbol.
int GetRandomSpecialSymbolFromPassword(const base::string16& password) {
  std::vector<int> symbols;
  std::copy_if(password.begin(), password.end(), std::back_inserter(symbols),
               &IsSpecialSymbol);
  DCHECK(!symbols.empty()) << "Password must contain at least one symbol.";
  return symbols[base::RandGenerator(symbols.size())];
}

size_t GetLowEntropyHashValue(const base::string16& value) {
  return base::PersistentHash(base::UTF16ToUTF8(value)) %
         kNumberOfLowEntropyHashValues;
}

}  // namespace

SingleUsernameVoteData::SingleUsernameVoteData(
    autofill::FieldRendererId renderer_id,
    const FormPredictions& form_predictions)
    : renderer_id(renderer_id), form_predictions(form_predictions) {}

SingleUsernameVoteData::SingleUsernameVoteData(
    const SingleUsernameVoteData& other) = default;
SingleUsernameVoteData& SingleUsernameVoteData::operator=(
    const SingleUsernameVoteData&) = default;
SingleUsernameVoteData::SingleUsernameVoteData(SingleUsernameVoteData&& other) =
    default;
SingleUsernameVoteData::~SingleUsernameVoteData() = default;

VotesUploader::VotesUploader(PasswordManagerClient* client,
                             bool is_possible_change_password_form)
    : client_(client),
      is_possible_change_password_form_(is_possible_change_password_form) {}

VotesUploader::VotesUploader(const VotesUploader& other) = default;
VotesUploader::~VotesUploader() = default;

void VotesUploader::SendVotesOnSave(
    const FormData& observed,
    const PasswordForm& submitted_form,
    const std::vector<const PasswordForm*>& best_matches,
    PasswordForm* pending_credentials) {
  if (pending_credentials->times_used == 1 ||
      IsAddingUsernameToExistingMatch(*pending_credentials, best_matches)) {
    UploadFirstLoginVotes(best_matches, *pending_credentials, submitted_form);
  }

  // Upload credentials the first time they are saved. This data is used
  // by password generation to help determine account creation sites.
  // Credentials that have been previously used (e.g., PSL matches) are checked
  // to see if they are valid account creation forms.
  if (pending_credentials->times_used == 0) {
    UploadPasswordVote(*pending_credentials, submitted_form, autofill::PASSWORD,
                       std::string());
    if (username_correction_vote_) {
      UploadPasswordVote(*username_correction_vote_, submitted_form,
                         autofill::USERNAME,
                         FormStructure(observed).FormSignatureAsStr());
    }
    MaybeSendSingleUsernameVote(true /* credentials_saved */);
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
        pending->generation_upload_status =
            PasswordForm::GenerationUploadStatus::kPositiveSignalSent;
      }
    }
  } else if (pending->generation_upload_status ==
             PasswordForm::GenerationUploadStatus::kPositiveSignalSent) {
    // A signal was sent that this was an account creation form, but the
    // credential is now being used on the same form again. This cancels out
    // the previous vote.
    if (UploadPasswordVote(*pending, submitted_form,
                           autofill::NOT_ACCOUNT_CREATION_PASSWORD,
                           std::string())) {
      pending->generation_upload_status =
          PasswordForm::GenerationUploadStatus::kNegativeSignalSent;
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

  AutofillDownloadManager* download_manager =
      client_->GetAutofillDownloadManager();
  if (!download_manager)
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
      DCHECK_EQ(form_to_upload.times_used, 0);
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
    logger.LogFormStructure(Logger::STRING_PASSWORD_FORM_VOTE, form_structure);
  }

  // Annotate the form with the source language of the page.
  form_structure.set_page_language(client_->GetPageLanguage());

  // Attach the Randomized Encoder.
  form_structure.set_randomized_encoder(
      RandomizedEncoder::Create(client_->GetPrefs()));

  // TODO(crbug.com/875768): Use VotesUploader::StartUploadRequest for avoiding
  // code duplication.
  bool success = download_manager->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      login_form_signature, true /* observed_submission */,
      nullptr /* prefs */);

  UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.UploadStarted", success);
  return success;
}

// TODO(crbug.com/840384): Share common code with UploadPasswordVote.
void VotesUploader::UploadFirstLoginVotes(
    const std::vector<const PasswordForm*>& best_matches,
    const PasswordForm& pending_credentials,
    const PasswordForm& form_to_upload) {
  AutofillDownloadManager* download_manager =
      client_->GetAutofillDownloadManager();
  if (!download_manager)
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

  // Annotate the form with the source language of the page.
  form_structure.set_page_language(client_->GetPageLanguage());

  // Attach the Randomized Encoder.
  form_structure.set_randomized_encoder(
      RandomizedEncoder::Create(client_->GetPrefs()));

  SetInitialHashValueOfUsernameField(
      form_to_upload.username_element_renderer_id, &form_structure);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_FIRSTUSE_FORM_VOTE, form_structure);
  }

  // TODO(crbug.com/875768): Use VotesUploader::StartUploadRequest for avoiding
  // code duplication.
  download_manager->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      std::string(), true /* observed_submission */, nullptr /* prefs */);
}

void VotesUploader::SetInitialHashValueOfUsernameField(
    autofill::FieldRendererId username_element_renderer_id,
    FormStructure* form_structure) {
  auto it = initial_values_.find(username_element_renderer_id);

  if (it == initial_values_.end() || it->second.empty())
    return;

  for (const auto& field : *form_structure) {
    if (field && field->unique_renderer_id == username_element_renderer_id) {
      const base::string16 form_signature =
          base::UTF8ToUTF16(form_structure->FormSignatureAsStr());
      const base::string16 seeded_input = it->second.append(form_signature);
      field->set_initial_value_hash(GetLowEntropyHashValue(seeded_input));
      break;
    }
  }
}

void VotesUploader::MaybeSendSingleUsernameVote(bool credentials_saved) {
  if (!single_username_vote_data_)
    return;

  FieldInfoManager* field_info_manager = client_->GetFieldInfoManager();
  if (!field_info_manager)
    return;

  const FormPredictions& predictions =
      single_username_vote_data_->form_predictions;
  std::vector<FieldSignature> field_signatures;
  for (const auto& field : predictions.fields)
    field_signatures.push_back(field.signature);

  std::unique_ptr<FormStructure> form_to_upload =
      FormStructure::CreateForPasswordManagerUpload(predictions.form_signature,
                                                    field_signatures);

  // Label the username field with a SINGLE_USERNAME or NOT_USERNAME vote.
  // TODO(crbug.com/552420): Use LabelFields() when cl/1667453 is landed.
  ServerFieldTypeSet available_field_types;
  for (size_t i = 0; i < form_to_upload->field_count(); ++i) {
    AutofillField* field = form_to_upload->field(i);

    ServerFieldType type = autofill::UNKNOWN_TYPE;
    autofill::FieldRendererId field_renderer_id =
        predictions.fields[i].renderer_id;
    if (field_renderer_id == single_username_vote_data_->renderer_id) {
      if (field_info_manager->GetFieldType(predictions.form_signature,
                                           predictions.fields[i].signature) !=
          autofill::UNKNOWN_TYPE) {
        // The vote for this field has been already sent. Don't send again.
        return;
      }
      type = credentials_saved && !has_username_edited_vote_
                 ? autofill::SINGLE_USERNAME
                 : autofill::NOT_USERNAME;
      if (has_username_edited_vote_)
        field->set_vote_type(AutofillUploadContents::Field::USERNAME_EDITED);
      available_field_types.insert(type);
      SaveFieldVote(form_to_upload->form_signature(),
                    field->GetFieldSignature(), type);
    }
    field->set_possible_types({type});
  }

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_USERNAME_FIRST_FLOW_VOTE,
                            *form_to_upload);
  }

  StartUploadRequest(std::move(form_to_upload), available_field_types);
}

void VotesUploader::AddGeneratedVote(FormStructure* form_structure) {
  DCHECK(form_structure);
  DCHECK(generation_popup_was_shown_);

  if (!generation_element_)
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
  } else {
    type = AutofillUploadContents::Field::IGNORED_GENERATION_POPUP;
  }

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);
    if (field->unique_renderer_id == generation_element_) {
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
    const std::vector<const PasswordForm*>& best_matches,
    FormStructure* form) {
  const base::string16& known_username = pending_credentials.username_value;
  base::string16 known_password;
  if (password_overridden_) {
    // If we are updating a password, the known value should be the old
    // password, not the new one.
    const PasswordForm* match =
        FindFormByUsername(best_matches, known_username);
    if (!match) {
      // Username was not found, do nothing.
      return;
    }
    known_password = match->password_value;
  } else {
    known_password = pending_credentials.password_value;
  }

  // If we are updating a password, the known value is the old password, not
  // the new one.
  for (auto& field : *form) {
    if (field->value.empty())
      continue;
    if (known_username == field->value || known_password == field->value) {
      field->properties_mask |= autofill::FieldPropertiesFlags::kKnownValue;
    }
  }
}

bool VotesUploader::FindUsernameInOtherPossibleUsernames(
    const PasswordForm& match,
    const base::string16& username) {
  DCHECK(!username_correction_vote_);

  for (const ValueElementPair& pair : match.all_possible_usernames) {
    if (pair.first == username) {
      username_correction_vote_ = match;
      username_correction_vote_->username_element = pair.second;
      return true;
    }
  }
  return false;
}

bool VotesUploader::FindCorrectedUsernameElement(
    const std::vector<const PasswordForm*>& matches,
    const base::string16& username,
    const base::string16& password) {
  if (username.empty())
    return false;
  for (const PasswordForm* match : matches) {
    if ((match->password_value == password) &&
        FindUsernameInOtherPossibleUsernames(*match, username)) {
      return true;
    }
  }
  return false;
}

void VotesUploader::GeneratePasswordAttributesVote(
    const base::string16& password_value,
    FormStructure* form_structure) {
  // Don't crowdsource password attributes for non-ascii passwords.
  for (const auto& e : password_value) {
    if (!(IsUppercaseLetter(e) || IsLowercaseLetter(e) || IsNumeric(e) ||
          IsSpecialSymbol(e))) {
      return;
    }
  }

  // Select a character class attribute to upload. Upload special symbols more
  // often (8 in 9 cases) as most issues are due to missing or wrong special
  // symbols. Don't upload info about uppercase and numeric characters as all
  // sites that allow lowercase letters also uppercase letters, and all sites
  // allow numeric symbols in passwords.
  autofill::PasswordAttribute character_class_attribute;
  bool (*predicate)(int c) = nullptr;
  if (base::RandGenerator(9) == 0) {
    predicate = &IsLowercaseLetter;
    character_class_attribute =
        autofill::PasswordAttribute::kHasLowercaseLetter;
  } else {
    predicate = &IsSpecialSymbol;
    character_class_attribute = autofill::PasswordAttribute::kHasSpecialSymbol;
  }

  // Apply the randomized response technique to noisify the actual value
  // (https://en.wikipedia.org/wiki/Randomized_response).
  bool respond_randomly = base::RandGenerator(2);
  bool randomized_value_for_character_class =
      respond_randomly ? base::RandGenerator(2)
                       : base::ranges::any_of(password_value, predicate);
  form_structure->set_password_attributes_vote(std::make_pair(
      character_class_attribute, randomized_value_for_character_class));

  if (character_class_attribute ==
          autofill::PasswordAttribute::kHasSpecialSymbol &&
      randomized_value_for_character_class) {
    form_structure->set_password_symbol_vote(
        respond_randomly ? GetRandomSpecialSymbol()
                         : GetRandomSpecialSymbolFromPassword(password_value));
  }

  size_t actual_length = password_value.size();
  size_t randomized_length = actual_length <= 1 || base::RandGenerator(5) == 0
                                 ? actual_length
                                 : base::RandGenerator(actual_length - 1) + 1;

  form_structure->set_password_length_vote(randomized_length);
}

void VotesUploader::StoreInitialFieldValues(
    const autofill::FormData& observed_form) {
  for (const auto& field : observed_form.fields) {
    if (!field.value.empty()) {
      initial_values_.insert(
          std::make_pair(field.unique_renderer_id, field.value));
    }
  }
}

bool VotesUploader::StartUploadRequest(
    std::unique_ptr<autofill::FormStructure> form_to_upload,
    const ServerFieldTypeSet& available_field_types) {
  AutofillDownloadManager* download_manager =
      client_->GetAutofillDownloadManager();
  if (!download_manager)
    return false;

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them.
  form_to_upload->set_upload_required(UPLOAD_REQUIRED);

  // Attach the Randomized Encoder.
  form_to_upload->set_randomized_encoder(
      RandomizedEncoder::Create(client_->GetPrefs()));

  return download_manager->StartUploadRequest(
      *form_to_upload, false /* was_autofilled */, available_field_types,
      std::string(), true /* observed_submission */, nullptr /* prefs */);
}

void VotesUploader::SaveFieldVote(autofill::FormSignature form_signature,
                                  autofill::FieldSignature field_signature,
                                  autofill::ServerFieldType field_type) {
  FieldInfoManager* field_info_manager = client_->GetFieldInfoManager();
  if (!field_info_manager)
    return;
  field_info_manager->AddFieldType(form_signature, field_signature, field_type);
}

}  // namespace password_manager
