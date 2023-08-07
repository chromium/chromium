// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <iostream>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using autofill::AutofillDownloadManager;
using autofill::AutofillField;
using autofill::AutofillUploadContents;
using autofill::FieldRendererId;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::RandomizedEncoder;
using autofill::ServerFieldType;
using autofill::ServerFieldTypeSet;
using password_manager_util::FindFormByUsername;

using Logger = autofill::SavePasswordProgressLogger;
using StringID = autofill::SavePasswordProgressLogger::StringID;

namespace password_manager {

namespace {
// Number of distinct low-entropy hash values.
constexpr uint32_t kNumberOfLowEntropyHashValues = 64;

// Helper function that assigns |field_types[field_name]=type| and also sets
// |field_name_collision| if |field_types[field_name]| is already set.
// TODO(crbug/1260336): The function is needed to only detect a
// field name collision and report that in a metric. Once the bug is fixed, the
// metric becomes obsolete and the function can be inlined.
void SetFieldType(const FieldRendererId& field_renderer_id,
                  const ServerFieldType type,
                  FieldTypeMap& field_types,
                  bool& field_name_collision) {
  if (field_renderer_id.is_null()) {
    return;
  }

  std::pair<FieldTypeMap::iterator, bool> it = field_types.insert(
      std::pair<FieldRendererId, ServerFieldType>(field_renderer_id, type));
  if (!it.second) {
    field_name_collision = true;
    // To preserve the old behavior, overwrite the type.
    it.first->second = type;
  }
}

// Sets autofill types of password and new password fields in |field_types|.
// |password_type| (the autofill type of new password field) should be equal to
// NEW_PASSWORD, PROBABLY_NEW_PASSWORD or NOT_NEW_PASSWORD. These values
// correspond to cases when the user confirmed password update, did nothing or
// declined to update password respectively.
void SetFieldLabelsOnUpdate(const ServerFieldType password_type,
                            const PasswordForm& submitted_form,
                            FieldTypeMap& field_types,
                            bool& field_name_collision) {
  DCHECK(password_type == autofill::NEW_PASSWORD ||
         password_type == autofill::PROBABLY_NEW_PASSWORD ||
         password_type == autofill::NOT_NEW_PASSWORD)
      << password_type;
  if (submitted_form.new_password_element_renderer_id.is_null()) {
    return;
  }

  if (submitted_form.password_element_renderer_id) {
    SetFieldType(submitted_form.password_element_renderer_id,
                 autofill::PASSWORD, field_types, field_name_collision);
  }
  SetFieldType(submitted_form.new_password_element_renderer_id, password_type,
               field_types, field_name_collision);
}

// Sets the autofill type of the password field stored in |submitted_form| to
// |password_type| in |field_types| map.
void SetFieldLabelsOnSave(const ServerFieldType password_type,
                          const PasswordForm& form,
                          FieldTypeMap& field_types,
                          bool& field_name_collision) {
  DCHECK(password_type == autofill::PASSWORD ||
         password_type == autofill::ACCOUNT_CREATION_PASSWORD ||
         password_type == autofill::NOT_ACCOUNT_CREATION_PASSWORD)
      << password_type;

  if (!form.new_password_element_renderer_id.is_null()) {
    SetFieldType(form.new_password_element_renderer_id, password_type,
                 field_types, field_name_collision);
  } else if (!form.password_element_renderer_id.is_null()) {
    SetFieldType(form.password_element_renderer_id, password_type, field_types,
                 field_name_collision);
  }
}

// Label username and password fields with autofill types in |form_structure|
// based on |field_types|, and vote types based on |vote_types|. The function
// also adds the types to |available_field_types|. For fields of |USERNAME|
// type, a vote type must exist.
void LabelFields(const FieldTypeMap& field_types,
                 const bool field_name_collision,
                 const VoteTypeMap& vote_types,
                 FormStructure* form_structure,
                 ServerFieldTypeSet* available_field_types) {
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.FieldNameCollisionInVotes",
                        field_name_collision);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);

    ServerFieldType type = autofill::UNKNOWN_TYPE;
    if (auto iter = field_types.find(field->unique_renderer_id);
        iter != field_types.end()) {
      type = iter->second;
      available_field_types->insert(type);
    }

    if (auto vote_type_iter = vote_types.find(field->unique_renderer_id);
        vote_type_iter != vote_types.end()) {
      field->set_vote_type(vote_type_iter->second);
    }
    CHECK(type != autofill::USERNAME ||
          field->vote_type() != AutofillUploadContents::Field::NO_INFORMATION);
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
  const PasswordForm* match = FindFormByUsername(matches, std::u16string());

  if (!match) {
    return false;
  }

  if (password_manager_util::GetMatchType(*match) ==
      password_manager_util::GetLoginMatchType::kPSL) {
    return false;
  }

  return match->password_value == credentials.password_value;
}

// Returns a uniformly distributed random symbol from the set of random symbols
// defined by the string |kSpecialSymbols|.
int GetRandomSpecialSymbol() {
  return password_manager_util::kSpecialSymbols[base::RandGenerator(
      std::size(password_manager_util::kSpecialSymbols))];
}

// Returns a random special symbol used in |password|.
// It is expected that |password| contains at least one special symbol.
int GetRandomSpecialSymbolFromPassword(const std::u16string& password) {
  std::vector<int> symbols;
  base::ranges::copy_if(password, std::back_inserter(symbols),
                        &password_manager_util::IsSpecialSymbol);
  DCHECK(!symbols.empty()) << "Password must contain at least one symbol.";
  return symbols[base::RandGenerator(symbols.size())];
}

size_t GetLowEntropyHashValue(const std::u16string& value) {
  return base::PersistentHash(base::UTF16ToUTF8(value)) %
         kNumberOfLowEntropyHashValues;
}

FieldSignature GetUsernameFieldSignature(
    const SingleUsernameVoteData& single_username_data) {
  for (const auto& field : single_username_data.form_predictions.fields) {
    if (field.renderer_id == single_username_data.renderer_id)
      return field.signature;
  }
  return FieldSignature();
}

AutofillUploadContents::ValueType GetValueType(
    const std::u16string& username_value,
    const std::vector<const PasswordForm*>& stored_credentials) {
  if (username_value.empty())
    return AutofillUploadContents::NO_VALUE_TYPE;

  // Check if |username_value| is an already stored username.
  // TODO(crbug.com/959776) Implement checking against usenames stored for all
  // domains and return STORED_FOR_ANOTHER_DOMAIN in that case.
  if (base::Contains(stored_credentials, username_value,
                     &PasswordForm::username_value)) {
    return AutofillUploadContents::STORED_FOR_CURRENT_DOMAIN;
  }

  if (autofill::MatchesRegex<autofill::kEmailValueRe>(username_value))
    return AutofillUploadContents::EMAIL;

  if (autofill::MatchesRegex<autofill::kPhoneValueRe>(username_value))
    return AutofillUploadContents::PHONE;

  if (autofill::MatchesRegex<autofill::kUsernameLikeValueRe>(username_value))
    return AutofillUploadContents::USERNAME_LIKE;

  if (username_value.find(' ') != std::u16string::npos)
    return AutofillUploadContents::VALUE_WITH_WHITESPACE;

  return AutofillUploadContents::VALUE_WITH_NO_WHITESPACE;
}

// Fills fake renderer id for to the significant field with `field_name` iff
// it isn't set.
void FillRendererIdIfNotSet(
    const std::u16string& field_name,
    autofill::FieldRendererId* element_renderer_id,
    const std::map<std::u16string, autofill::FieldRendererId>&
        field_name_to_renderer_id) {
  CHECK(element_renderer_id->is_null())
      << "Unexpected non-null renderer_id in a form deserialized from "
         "LoginDatabase.";
  if (!field_name.empty()) {
    auto iter = field_name_to_renderer_id.find(field_name);
    if (iter != field_name_to_renderer_id.end()) {
      *element_renderer_id = iter->second;
    }
  }
}

// Generates fake renderer ids for the `matched_form` deserialized from
// `LoginDatabase`. Field renderer id is used later in `UploadPasswordVote` to
// identify fields and generate votes for this form.
// TODO(crbug/1260336): The function is needed to only provide a way to identify
// fields using field renderer ids for forms from LoginDatabase as it doesn't
// store renderer ids for fields. It should be removed after migrating to a
// stable unique field identifier (e.g. FieldSignature).
void GenerateSyntheticRenderIdsAndAssignThem(PasswordForm& matched_form) {
  uint32_t renderer_id_counter_ = 1;

  std::map<std::u16string, autofill::FieldRendererId> field_name_to_renderer_id;
  for (autofill::FormFieldData& field : matched_form.form_data.fields) {
    CHECK(field.unique_renderer_id.is_null())
        << "Unexpected non-null unique_renderer_id in a from deserialized form "
           "LoginDatabase.";
    field.unique_renderer_id =
        autofill::FieldRendererId(renderer_id_counter_++);
    field_name_to_renderer_id.insert({field.name, field.unique_renderer_id});
  }

  FillRendererIdIfNotSet(matched_form.username_element,
                         &matched_form.username_element_renderer_id,
                         field_name_to_renderer_id);
  FillRendererIdIfNotSet(matched_form.password_element,
                         &matched_form.password_element_renderer_id,
                         field_name_to_renderer_id);
  FillRendererIdIfNotSet(matched_form.new_password_element,
                         &matched_form.new_password_element_renderer_id,
                         field_name_to_renderer_id);
  FillRendererIdIfNotSet(
      matched_form.confirmation_password_element,
      &matched_form.confirmation_password_element_renderer_id,
      field_name_to_renderer_id);
}

}  // namespace

SingleUsernameVoteData::SingleUsernameVoteData(
    FieldRendererId renderer_id,
    const std::u16string& username_value,
    const FormPredictions& form_predictions,
    const std::vector<const PasswordForm*>& stored_credentials,
    bool password_form_had_username_field)
    : renderer_id(renderer_id),
      form_predictions(form_predictions),
      password_form_had_username_field(password_form_had_username_field) {
  base::TrimWhitespace(username_value, base::TrimPositions::TRIM_ALL,
                       &username_candidate_value);
  value_type = GetValueType(username_candidate_value, stored_credentials);
  prompt_edit = autofill::AutofillUploadContents::EDIT_UNSPECIFIED;
}

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
  if (pending_credentials->times_used_in_html_form == 1 ||
      IsAddingUsernameToExistingMatch(*pending_credentials, best_matches)) {
    UploadFirstLoginVotes(best_matches, *pending_credentials, submitted_form);
  }

  // Upload credentials the first time they are saved. This data is used
  // by password generation to help determine account creation sites.
  // Credentials that have been previously used (e.g., PSL matches) are checked
  // to see if they are valid account creation forms.
  if (pending_credentials->times_used_in_html_form == 0) {
    MaybeSendSingleUsernameVote();
    UploadPasswordVote(*pending_credentials, submitted_form, autofill::PASSWORD,
                       std::string());
    if (username_correction_vote_) {
      UploadPasswordVote(
          *username_correction_vote_, submitted_form, autofill::USERNAME,
          base::NumberToString(*autofill::CalculateFormSignature(observed)));
      username_correction_vote_.reset();
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
    if (pending->times_used_in_html_form == 1) {
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
  // Used to detect whether the vote is corrupted because of duplicate field
  // names.
  bool field_name_collision = false;
  auto username_vote_type = AutofillUploadContents::Field::NO_INFORMATION;
  if (autofill_type != autofill::USERNAME) {
    if (has_autofill_vote) {
      bool is_update = autofill_type == autofill::NEW_PASSWORD ||
                       autofill_type == autofill::PROBABLY_NEW_PASSWORD ||
                       autofill_type == autofill::NOT_NEW_PASSWORD;

      if (is_update) {
        if (form_to_upload.new_password_element_renderer_id.is_null()) {
          return false;
        }
        SetFieldLabelsOnUpdate(autofill_type, form_to_upload, field_types,
                               field_name_collision);
      } else {  // Saving.
        SetFieldLabelsOnSave(autofill_type, form_to_upload, field_types,
                             field_name_collision);
      }
      if (autofill_type != autofill::ACCOUNT_CREATION_PASSWORD) {
        // If |autofill_type| == autofill::ACCOUNT_CREATION_PASSWORD, Chrome
        // will upload a vote for another form: the one that the credential was
        // saved on.
        SetFieldType(submitted_form.confirmation_password_element_renderer_id,
                     autofill::CONFIRMATION_PASSWORD, field_types,
                     field_name_collision);
        form_structure.set_passwords_were_revealed(
            has_passwords_revealed_vote_);
      }
      // If a user accepts a save or update prompt, send a single username vote.
      if ((autofill_type == autofill::PASSWORD ||
           autofill_type == autofill::NEW_PASSWORD) &&
          single_username_vote_data_) {
        SetSingleUsernameVoteOnPasswordForm(form_structure);
      }
    }
    if (autofill_type != autofill::ACCOUNT_CREATION_PASSWORD) {
      if (generation_popup_was_shown_)
        AddGeneratedVote(&form_structure);
      if (username_change_state_ == UsernameChangeState::kChangedToKnownValue) {
        SetFieldType(form_to_upload.username_element_renderer_id,
                     autofill::USERNAME, field_types, field_name_collision);
        username_vote_type = AutofillUploadContents::Field::USERNAME_EDITED;
      }
    } else {  // User reuses credentials.
      // If the saved username value was used, then send a confirmation vote for
      // username.
      if (!submitted_form.username_value.empty()) {
        DCHECK(submitted_form.username_value == form_to_upload.username_value);
        SetFieldType(form_to_upload.username_element_renderer_id,
                     autofill::USERNAME, field_types, field_name_collision);
        username_vote_type = AutofillUploadContents::Field::CREDENTIALS_REUSED;
      }
    }
    if (autofill_type == autofill::PASSWORD ||
        autofill_type == autofill::NEW_PASSWORD) {
      // The password attributes should be uploaded only on the first save or an
      // update.
      DCHECK_EQ(form_to_upload.times_used_in_html_form, 0);
      GeneratePasswordAttributesVote(autofill_type == autofill::PASSWORD
                                         ? form_to_upload.password_value
                                         : form_to_upload.new_password_value,
                                     &form_structure);
    }
  } else {  // User overwrites username.
    SetFieldType(form_to_upload.username_element_renderer_id,
                 autofill::USERNAME, field_types, field_name_collision);
    SetFieldType(form_to_upload.password_element_renderer_id,
                 autofill::ACCOUNT_CREATION_PASSWORD, field_types,
                 field_name_collision);
    username_vote_type = AutofillUploadContents::Field::USERNAME_OVERWRITTEN;
  }
  LabelFields(
      field_types, field_name_collision,
      {{form_to_upload.username_element_renderer_id, username_vote_type}},
      &form_structure, &available_field_types);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_PASSWORD_FORM_VOTE, form_structure);
  }

  // Annotate the form with the source language of the page.
  form_structure.set_current_page_language(client_->GetPageLanguage());

  // Attach the Randomized Encoder.
  form_structure.set_randomized_encoder(
      RandomizedEncoder::Create(client_->GetPrefs()));

  // TODO(crbug.com/875768): Use VotesUploader::StartUploadRequest for avoiding
  // code duplication.
  return download_manager->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      login_form_signature, true /* observed_submission */, nullptr /* prefs */,
      nullptr /* observer */);
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

  FieldTypeMap field_types;
  bool field_name_collision = false;
  SetFieldType(form_to_upload.username_element_renderer_id, autofill::USERNAME,
               field_types, field_name_collision);
  VoteTypeMap vote_types = {{form_to_upload.username_element_renderer_id,
                             AutofillUploadContents::Field::FIRST_USE}};
  if (!password_overridden_) {
    SetFieldType(form_to_upload.password_element_renderer_id,
                 autofill::PASSWORD, field_types, field_name_collision);
    vote_types[form_to_upload.password_element_renderer_id] =
        AutofillUploadContents::Field::FIRST_USE;
  }

  ServerFieldTypeSet available_field_types;
  LabelFields(field_types, field_name_collision, vote_types, &form_structure,
              &available_field_types);
  SetKnownValueFlag(pending_credentials, best_matches, &form_structure);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  // Annotate the form with the source language of the page.
  form_structure.set_current_page_language(client_->GetPageLanguage());

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
      std::string(), true /* observed_submission */, nullptr /* prefs */,
      nullptr);
}

void VotesUploader::SetInitialHashValueOfUsernameField(
    FieldRendererId username_element_renderer_id,
    FormStructure* form_structure) {
  auto it = initial_values_.find(username_element_renderer_id);

  if (it == initial_values_.end() || it->second.empty())
    return;

  for (const auto& field : *form_structure) {
    if (field && field->unique_renderer_id == username_element_renderer_id) {
      const std::u16string form_signature =
          base::UTF8ToUTF16(form_structure->FormSignatureAsStr());
      const std::u16string seeded_input = it->second.append(form_signature);
      field->set_initial_value_hash(GetLowEntropyHashValue(seeded_input));
      break;
    }
  }
}

void VotesUploader::MaybeSendSingleUsernameVote() {
  if (!single_username_vote_data_)
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
    FieldRendererId field_renderer_id = predictions.fields[i].renderer_id;

    if (field_renderer_id != single_username_vote_data_->renderer_id) {
      field->set_possible_types({autofill::UNKNOWN_TYPE});
      continue;
    }
    if (!SetSingleUsernameVoteOnUsernameForm(field, &available_field_types,
                                             predictions.form_signature)) {
      // The single username field has no field type. Don't send vote.
      return;
    }
  }

  // Upload a vote on the username form if available.
  if (!available_field_types.empty()) {
    if (password_manager_util::IsLoggingActive(client_)) {
      BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
      logger.LogFormStructure(Logger::STRING_USERNAME_FIRST_FLOW_VOTE,
                              *form_to_upload);
    }

    if (StartUploadRequest(std::move(form_to_upload), available_field_types)) {
      base::UmaHistogramBoolean(
          "PasswordManager.SingleUsername.PasswordFormHadUsernameField",
          single_username_vote_data_->password_form_had_username_field);
    }
  }
}

#if !BUILDFLAG(IS_ANDROID)
void VotesUploader::CalculateUsernamePromptEditState(
    const std::u16string& saved_username) {
  if (!single_username_vote_data_ ||
      single_username_vote_data_->username_candidate_value.empty()) {
    return;
  }
  const auto& single_username_value =
      single_username_vote_data_->username_candidate_value;

  autofill::AutofillUploadContents::SingleUsernamePromptEdit prompt_edit =
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED;
  if (saved_username != suggested_username_) {
    // In this branch, the user edited the username in a prompt before accepting
    // it.

    // The user removed some suggested username and that username wasn't the
    // possible single username (|single_username_value|) => this is neither
    // negative nor positive vote. If the user removes |single_username_value|,
    // then it is a negative signal and will be reported below.
    if (saved_username.empty() &&
        suggested_username_ != single_username_value) {
      return;
    }

    if (saved_username == single_username_value)
      prompt_edit = autofill::AutofillUploadContents::EDITED_POSITIVE;
    else
      prompt_edit = autofill::AutofillUploadContents::EDITED_NEGATIVE;

  } else {  // saved_username == suggested_username
    // In this branch the user did NOT edit the username in prompt and accepted
    // it as it is.

    if (saved_username == single_username_value)
      prompt_edit = autofill::AutofillUploadContents::NOT_EDITED_POSITIVE;
    else
      prompt_edit = autofill::AutofillUploadContents::NOT_EDITED_NEGATIVE;
  }
  single_username_vote_data_->prompt_edit = prompt_edit;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void VotesUploader::AddGeneratedVote(FormStructure* form_structure) {
  DCHECK(form_structure);
  DCHECK(generation_popup_was_shown_);

  if (!generation_element_)
    return;

  AutofillUploadContents::Field::PasswordGenerationType type =
      AutofillUploadContents::Field::NO_GENERATION;
  if (has_generated_password_) {
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
  const std::u16string& known_username = pending_credentials.username_value;
  std::u16string known_password;
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

bool VotesUploader::FindUsernameInOtherAlternativeUsernames(
    const PasswordForm& match,
    const std::u16string& username) {
  for (const AlternativeElement& element : match.all_alternative_usernames) {
    if (element.value == username) {
      username_correction_vote_ = match;
      username_correction_vote_->username_element = element.name;
      GenerateSyntheticRenderIdsAndAssignThem(*username_correction_vote_);
      return true;
    }
  }
  return false;
}

bool VotesUploader::FindCorrectedUsernameElement(
    const std::vector<const PasswordForm*>& matches,
    const std::u16string& username,
    const std::u16string& password) {
  // As the username may have changed, re-compute |username_correction_vote_|.
  username_correction_vote_.reset();
  if (username.empty())
    return false;
  for (const PasswordForm* match : matches) {
    if ((match->password_value == password) &&
        FindUsernameInOtherAlternativeUsernames(*match, username)) {
      return true;
    }
  }
  return false;
}

void VotesUploader::GeneratePasswordAttributesVote(
    const std::u16string& password_value,
    FormStructure* form_structure) {
  if (password_value.empty()) {
    NOTREACHED() << "GeneratePasswordAttributesVote cannot take an empty "
                    "password value.";
    return;
  }

  // Don't crowdsource password attributes for non-ascii passwords.
  for (const auto& e : password_value) {
    if (!(password_manager_util::IsLetter(e) ||
          password_manager_util::IsNumeric(e) ||
          password_manager_util::IsSpecialSymbol(e))) {
      return;
    }
  }

  // Select a character class attribute to upload. Upload special symbols more
  // often (8 in 9 cases) as most issues are due to missing or wrong special
  // symbols. Upload info about letters existence otherwise.
  autofill::PasswordAttribute character_class_attribute;
  bool (*predicate)(char16_t c) = nullptr;
  if (base::RandGenerator(9) == 0) {
    predicate = &password_manager_util::IsLetter;
    character_class_attribute = autofill::PasswordAttribute::kHasLetter;
  } else {
    predicate = &password_manager_util::IsSpecialSymbol;
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
      std::string(), true /* observed_submission */, nullptr /* prefs */,
      nullptr);
}

bool VotesUploader::SetSingleUsernameVoteOnUsernameForm(
    AutofillField* field,
    ServerFieldTypeSet* available_field_types,
    FormSignature form_signature) {
  ServerFieldType type = autofill::UNKNOWN_TYPE;
  autofill::AutofillUploadContents_Field_SingleUsernameVoteType vote_type =
      AutofillUploadContents::Field::DEFAULT;

  // Send a negative vote if the possible username value contains whitespaces.
  const std::u16string single_username_value =
      single_username_vote_data_->username_candidate_value;
  if (single_username_value.find(' ') != std::u16string::npos) {
    type = autofill::NOT_USERNAME;
    vote_type = AutofillUploadContents::Field::STRONG;
  } else {
// It's not possible to edit username in the save prompt on Android, thus it's
// not possible to rely on this heuristic.
#if !BUILDFLAG(IS_ANDROID)
    const auto& prompt_edit = single_username_vote_data_->prompt_edit;
    // There is no meaningful data on prompt edit, the vote should not be sent.
    if (prompt_edit == AutofillUploadContents::EDIT_UNSPECIFIED)
      return false;
    type = (prompt_edit == AutofillUploadContents::EDITED_POSITIVE ||
            prompt_edit == AutofillUploadContents::NOT_EDITED_POSITIVE)
               ? autofill::SINGLE_USERNAME
               : autofill::NOT_USERNAME;
    vote_type = (prompt_edit == AutofillUploadContents::EDITED_POSITIVE ||
                 prompt_edit == AutofillUploadContents::EDITED_NEGATIVE)
                    ? AutofillUploadContents::Field::STRONG
                    : AutofillUploadContents::Field::WEAK;
#else
    return false;
#endif  // !BUILDFLAG(IS_ANDROID)
  }
  available_field_types->insert(type);
  field->set_possible_types({type});
  field->set_single_username_vote_type(vote_type);
  return true;
}

void VotesUploader::SetSingleUsernameVoteOnPasswordForm(
    FormStructure& form_structure) {
  if (!base::FeatureList::IsEnabled(
          features::kUsernameFirstFlowFallbackCrowdsourcing)) {
    return;
  }
  AutofillUploadContents::SingleUsernameData single_username_data;
  single_username_data.set_username_form_signature(
      single_username_vote_data_->form_predictions.form_signature.value());
  single_username_data.set_username_field_signature(
      GetUsernameFieldSignature(*single_username_vote_data_).value());
  single_username_data.set_value_type(single_username_vote_data_->value_type);
  single_username_data.set_prompt_edit(single_username_vote_data_->prompt_edit);

  form_structure.set_single_username_data(single_username_data);
}

}  // namespace password_manager
