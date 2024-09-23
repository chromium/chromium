// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_constants.h"

using autofill::AutofillCrowdsourcingManager;
using autofill::AutofillField;
using autofill::AutofillUploadContents;
using autofill::FieldRendererId;
using autofill::FieldSignature;
using autofill::FieldType;
using autofill::FieldTypeSet;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::IsMostRecentSingleUsernameCandidate;
using autofill::RandomizedEncoder;
using password_manager_util::FindFormByUsername;

using Logger = autofill::SavePasswordProgressLogger;
using StringID = autofill::SavePasswordProgressLogger::StringID;

namespace password_manager {

namespace {
// Number of distinct low-entropy hash values.
constexpr uint32_t kNumberOfLowEntropyHashValues = 64;

// Helper function that assigns |field_types[field_name]=type| and also sets
// |field_name_collision| if |field_types[field_name]| is already set.
// TODO(crbug.com/40201826): The function is needed to only detect a
// field name collision and report that in a metric. Once the bug is fixed, the
// metric becomes obsolete and the function can be inlined.
void SetFieldType(const FieldRendererId& field_renderer_id,
                  const FieldType type,
                  FieldTypeMap& field_types,
                  bool& field_name_collision) {
  if (field_renderer_id.is_null()) {
    return;
  }

  std::pair<FieldTypeMap::iterator, bool> it = field_types.insert(
      std::pair<FieldRendererId, FieldType>(field_renderer_id, type));
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
void SetFieldLabelsOnUpdate(const FieldType password_type,
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
void SetFieldLabelsOnSave(const FieldType password_type,
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
                 FieldTypeSet* available_field_types) {
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.FieldNameCollisionInVotes",
                        field_name_collision);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    AutofillField* field = form_structure->field(i);

    FieldType type = autofill::UNKNOWN_TYPE;
    if (auto iter = field_types.find(field->renderer_id());
        iter != field_types.end()) {
      type = iter->second;
      available_field_types->insert(type);
    }

    if (auto vote_type_iter = vote_types.find(field->renderer_id());
        vote_type_iter != vote_types.end()) {
      field->set_vote_type(vote_type_iter->second);
    }
    CHECK(type != autofill::USERNAME ||
          field->vote_type() != AutofillUploadContents::Field::NO_INFORMATION);
    FieldTypeSet types;
    types.insert(type);
    field->set_possible_types(types);
  }
}

// Returns true if |credentials| has the same password as an entry in |matches|
// which doesn't have a username.
bool IsAddingUsernameToExistingMatch(
    const PasswordForm& credentials,
    const base::span<const PasswordForm>& matches) {
  if (credentials.username_value.empty()) {
    return false;
  }
  const PasswordForm* match = FindFormByUsername(matches, std::u16string());

  if (!match) {
    return false;
  }

  // TODO(b/331409076): investigate if affiliated and grouped matches should be
  // skipped as well.
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
      password_manager_util::kSpecialSymbols.size())];
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
    if (field.renderer_id == single_username_data.renderer_id) {
      return field.signature;
    }
  }
  return FieldSignature();
}

AutofillUploadContents::ValueType GetValueType(
    const std::u16string& username_value,
    const base::span<const PasswordForm>& stored_credentials) {
  if (username_value.empty()) {
    return AutofillUploadContents::NO_VALUE_TYPE;
  }

  // Check if |username_value| is an already stored username.
  // TODO(crbug.com/40626063) Implement checking against usenames stored for all
  // domains and return STORED_FOR_ANOTHER_DOMAIN in that case.
  if (base::Contains(stored_credentials, username_value,
                     &PasswordForm::username_value)) {
    return AutofillUploadContents::STORED_FOR_CURRENT_DOMAIN;
  }

  if (autofill::MatchesRegex<constants::kEmailValueRe>(username_value)) {
    return AutofillUploadContents::EMAIL;
  }

  if (autofill::MatchesRegex<constants::kPhoneValueRe>(username_value)) {
    return AutofillUploadContents::PHONE;
  }

  if (autofill::MatchesRegex<constants::kUsernameLikeValueRe>(username_value)) {
    return AutofillUploadContents::USERNAME_LIKE;
  }

  if (username_value.find(' ') != std::u16string::npos) {
    return AutofillUploadContents::VALUE_WITH_WHITESPACE;
  }

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
// TODO(crbug.com/40201826): The function is needed to only provide a way to
// identify fields using field renderer ids for forms from LoginDatabase as it
// doesn't store renderer ids for fields. It should be removed after migrating
// to a stable unique field identifier (e.g. FieldSignature).
void GenerateSyntheticRenderIdsAndAssignThem(PasswordForm& matched_form) {
  uint32_t renderer_id_counter_ = 1;

  std::map<std::u16string, autofill::FieldRendererId> field_name_to_renderer_id;
  std::vector<FormFieldData> fields = matched_form.form_data.ExtractFields();
  for (autofill::FormFieldData& field : fields) {
    CHECK(field.renderer_id().is_null())
        << "Unexpected non-null renderer_id in a from deserialized form "
           "LoginDatabase.";
    field.set_renderer_id(autofill::FieldRendererId(renderer_id_counter_++));
    field_name_to_renderer_id.insert({field.name(), field.renderer_id()});
  }
  matched_form.form_data.set_fields(std::move(fields));

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

// Search if the username (can be suggested or saved value in the save prompt)
// can be found among text fields of the password form.
bool IsUsernameInAlternativeUsernames(
    const std::u16string& username,
    const AlternativeElementVector& all_alternative_usernames) {
  return base::ranges::any_of(
      all_alternative_usernames,
      [username](const AlternativeElement& alternative_username) {
        return alternative_username.value == username;
      });
}

// Encode password attributes and length into `upload`.
void EncodePasswordAttributesMetadata(
    const PasswordAttributesMetadata& password_attributes,
    AutofillUploadContents& upload) {
  switch (password_attributes.password_attributes_vote.first) {
    case PasswordAttribute::kHasLetter:
      upload.set_password_has_letter(
          password_attributes.password_attributes_vote.second);
      break;
    case PasswordAttribute::kHasSpecialSymbol:
      upload.set_password_has_special_symbol(
          password_attributes.password_attributes_vote.second);
      if (password_attributes.password_attributes_vote.second) {
        upload.set_password_special_symbol(
            password_attributes.password_symbol_vote);
      }
      break;
    case PasswordAttribute::kPasswordAttributesCount:
      NOTREACHED_IN_MIGRATION();
  }
  upload.set_password_length(password_attributes.password_length_vote);
}

void AdjustTypesForForgotPasswordFormVotes(
    FieldType& field_type,
    autofill::AutofillUploadContents_Field_SingleUsernameVoteType& vote_type) {
  if (field_type == autofill::SINGLE_USERNAME) {
    field_type = autofill::SINGLE_USERNAME_FORGOT_PASSWORD;
  }

  if (vote_type == AutofillUploadContents::Field::STRONG) {
    vote_type = AutofillUploadContents::Field::STRONG_FORGOT_PASSWORD;
  } else if (vote_type == AutofillUploadContents::Field::WEAK) {
    vote_type = AutofillUploadContents::Field::WEAK_FORGOT_PASSWORD;
  }
}

}  // namespace

SingleUsernameVoteData::SingleUsernameVoteData()
    : SingleUsernameVoteData(FieldRendererId(),
                             /*username_value=*/std::u16string(),
                             FormPredictions(),
                             /*stored_credentials=*/{},
                             PasswordFormHadMatchingUsername(false)) {}

SingleUsernameVoteData::SingleUsernameVoteData(
    FieldRendererId renderer_id,
    const std::u16string& username_value,
    const FormPredictions& form_predictions,
    const base::span<const PasswordForm>& stored_credentials,
    PasswordFormHadMatchingUsername password_form_had_matching_username)
    : renderer_id(renderer_id),
      form_predictions(form_predictions),
      password_form_had_matching_username(password_form_had_matching_username) {
  base::TrimWhitespace(username_value, base::TrimPositions::TRIM_ALL,
                       &username_candidate_value);
  value_type = GetValueType(username_candidate_value, stored_credentials);
  prompt_edit = AutofillUploadContents::EDIT_UNSPECIFIED;
  is_form_overrule = false;
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
    const base::span<const PasswordForm>& best_matches,
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
    MaybeSendSingleUsernameVotes();
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
  if (pending->form_data.fields().empty()) {
    return;
  }

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
    const FieldType autofill_type,
    const std::string& login_form_signature) {
  // Check if there is any vote to be sent.
  bool has_autofill_vote = autofill_type != autofill::UNKNOWN_TYPE;
  bool has_password_generation_vote = generation_popup_was_shown_;
  if (!has_autofill_vote && !has_password_generation_vote) {
    return false;
  }

  if (form_to_upload.form_data.fields().empty()) {
    // List of fields may be empty in tests.
    return false;
  }

  AutofillCrowdsourcingManager* crowdsourcing_manager =
      client_->GetAutofillCrowdsourcingManager();
  if (!crowdsourcing_manager) {
    return false;
  }

  // If this is an update, a vote about the observed form is sent. If the user
  // re-uses credentials, a vote about the saved form is sent. If the user saves
  // credentials, the observed and pending forms are the same.
  FormStructure form_structure(form_to_upload.form_data);
  form_structure.set_submission_event(submitted_form.submission_event);

  FieldTypeSet available_field_types;
  // A map from field names to field types.
  FieldTypeMap field_types;
  // Used to detect whether the vote is corrupted because of duplicate field
  // names.
  bool field_name_collision = false;
  auto username_vote_type = AutofillUploadContents::Field::NO_INFORMATION;
  bool should_set_passwords_were_revealed = false;
  std::optional<PasswordAttributesMetadata> password_attributes;
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
        should_set_passwords_were_revealed = true;
      }
      // If a user accepts a save or update prompt, send a single username vote.
      if (autofill_type == autofill::PASSWORD ||
          autofill_type == autofill::NEW_PASSWORD) {
        if (!single_username_votes_data_.empty() &&
            base::FeatureList::IsEnabled(
                features::kUsernameFirstFlowFallbackCrowdsourcing)) {
          // Send single username vote only on the most recent user modified
          // field outside of the password form.
          // TODO(crbug.com/40925827): Send votes for fallback crowdsourcing on
          // all single username field candidates.
          SetSingleUsernameVoteOnPasswordForm(single_username_votes_data_[0],
                                              form_structure);
        }
      }
    }
    if (autofill_type != autofill::ACCOUNT_CREATION_PASSWORD) {
      if (generation_popup_was_shown_) {
        AddGeneratedVote(&form_structure);
      }
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
      password_attributes = GeneratePasswordAttributesMetadata(
          autofill_type == autofill::PASSWORD
              ? form_to_upload.password_value
              : form_to_upload.new_password_value);
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

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_PASSWORD_FORM_VOTE, form_structure,
                            password_attributes);
  }

  return SendUploadRequest(form_structure, available_field_types,
                           login_form_signature, password_attributes,
                           should_set_passwords_were_revealed);
}

// TODO(crbug.com/40575167): Share common code with UploadPasswordVote.
void VotesUploader::UploadFirstLoginVotes(
    const base::span<const PasswordForm>& best_matches,
    const PasswordForm& pending_credentials,
    const PasswordForm& form_to_upload) {
  AutofillCrowdsourcingManager* crowdsourcing_manager =
      client_->GetAutofillCrowdsourcingManager();
  if (!crowdsourcing_manager) {
    return;
  }

  if (form_to_upload.form_data.fields().empty()) {
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

  FieldTypeSet available_field_types;
  LabelFields(field_types, field_name_collision, vote_types, &form_structure,
              &available_field_types);
  SetKnownValueFlag(pending_credentials, best_matches, &form_structure);

  // Annotate the form with the source language of the page.
  form_structure.set_current_page_language(client_->GetPageLanguage());

  SetInitialHashValueOfUsernameField(
      form_to_upload.username_element_renderer_id, &form_structure);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormStructure(Logger::STRING_FIRSTUSE_FORM_VOTE, form_structure,
                            std::nullopt);
  }

  SendUploadRequest(form_structure, available_field_types,
                    /*login_form_signature=*/std::string(),
                    /*password_attributes=*/std::nullopt,
                    /*should_set_passwords_were_revealed=*/false);
}

void VotesUploader::SetInitialHashValueOfUsernameField(
    FieldRendererId username_element_renderer_id,
    FormStructure* form_structure) {
  auto it = initial_values_.find(username_element_renderer_id);

  if (it == initial_values_.end() || it->second.empty()) {
    return;
  }

  for (const auto& field : *form_structure) {
    if (field && field->renderer_id() == username_element_renderer_id) {
      const std::u16string form_signature =
          base::UTF8ToUTF16(form_structure->FormSignatureAsStr());
      const std::u16string seeded_input = it->second.append(form_signature);
      field->set_initial_value_hash(GetLowEntropyHashValue(seeded_input));
      break;
    }
  }
}

void VotesUploader::MaybeSendSingleUsernameVotes() {
// UFF votes are not sent on Android, since it wasn't possible to edit the
// username in prompt before UFF was launched. Later, password edit dialog
// was added, but Android votes were never evaluated.
// TODO(crbug.com/40279590): Verify if the votes are produced as expected on
// Android and enable UFF voting.
#if !BUILDFLAG(IS_ANDROID)
  bool should_send_votes =
      (should_send_username_first_flow_votes_ ||
       base::ranges::any_of(single_username_votes_data_,
                            [](const SingleUsernameVoteData& vote_data) {
                              return vote_data.is_form_overrule;
                            }));
  // Send single username votes in two cases:
  // (1) `should_send_username_first_flow_votes_` is true, meaning Username
  // First Flow was observed.
  // (2) There is an `IN_FORM_OVERRULE` vote. This means that the flow was not
  // initially predicted as a Username First Flow, but user's action signal
  // that it is Username First Flow.
  if (should_send_votes) {
    for (size_t i = 0; i < single_username_votes_data_.size(); ++i) {
      const SingleUsernameVoteData& vote_data = single_username_votes_data_[i];
      if (MaybeSendSingleUsernameVote(
              vote_data, vote_data.form_predictions,
              i == 0 ? IsMostRecentSingleUsernameCandidate::kMostRecentCandidate
                     : IsMostRecentSingleUsernameCandidate::
                           kHasIntermediateValuesInBetween,
              /*is_forgot_password_vote=*/false)) {
        base::UmaHistogramBoolean(
            "PasswordManager.SingleUsername.PasswordFormHadUsernameField",
            vote_data.password_form_had_matching_username.value());
        // TODO(crbug.com/40925827): Implement UMA metric logging the index in
        // LRU cache if `IN_FORM_OVERRULE` is sent.
      }
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Don't set is_most_recent_single_username_candidate for FPF votes, since
  // this is unrelated to FPF.
  for (auto& [field_id, vote_data] : forgot_password_vote_data_) {
    MaybeSendSingleUsernameVote(
        vote_data, vote_data.form_predictions,
        IsMostRecentSingleUsernameCandidate::kNotPartOfUsernameFirstFlow,
        /*is_forgot_password_vote=*/true);
  }

  SingleUsernameVoteDataAvailability availability =
      SingleUsernameVoteDataAvailability::kNone;
  if (!single_username_votes_data_.empty() &&
      forgot_password_vote_data_.size() > 0) {
    availability = SingleUsernameVoteDataAvailability::kBothNoOverlap;
    for (auto vote_data : single_username_votes_data_) {
      if (forgot_password_vote_data_.contains(vote_data.renderer_id)) {
        availability = SingleUsernameVoteDataAvailability::kBothWithOverlap;
        break;
      }
    }
  } else if (!single_username_votes_data_.empty()) {
    availability = SingleUsernameVoteDataAvailability::kUsernameFirstOnly;
  } else if (forgot_password_vote_data_.size() > 0) {
    availability = SingleUsernameVoteDataAvailability::kForgotPasswordOnly;
  }
  base::UmaHistogramEnumeration(
      "PasswordManager.SingleUsername.VoteDataAvailability", availability);
}

void VotesUploader::CalculateUsernamePromptEditState(
    const std::u16string& saved_username,
    const AlternativeElementVector& all_alternative_usernames) {
  for (auto& vote_data : single_username_votes_data_) {
    if (!vote_data.username_candidate_value.empty()) {
      vote_data.prompt_edit = CalculateUsernamePromptEdit(
          saved_username, vote_data.username_candidate_value);
      vote_data.is_form_overrule =
          CalculateInFormOverrule(saved_username,
                                  vote_data.username_candidate_value,
                                  all_alternative_usernames) &&
          base::FeatureList::IsEnabled(
              features::kUsernameFirstFlowWithIntermediateValuesVoting);
    }
  }
  for (auto& [field_id, vote_data] : forgot_password_vote_data_) {
    // For FPF, IN_FORM_OVERRULE votes are not sent, do not calculate
    // `is_form_overrule`.
    vote_data.prompt_edit = CalculateUsernamePromptEdit(
        saved_username, vote_data.username_candidate_value);
  }
}

void VotesUploader::AddForgotPasswordVoteData(
    const SingleUsernameVoteData& vote_data) {
  // TODO(crbug.com/40277063): Implement votes uploading based on this.
  forgot_password_vote_data_[vote_data.renderer_id] = vote_data;
}

void VotesUploader::AddGeneratedVote(FormStructure* form_structure) {
  DCHECK(form_structure);
  DCHECK(generation_popup_was_shown_);

  if (!generation_element_) {
    return;
  }

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
    if (field->renderer_id() == generation_element_) {
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
    const base::span<const PasswordForm>& best_matches,
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
    if (field->value(autofill::ValueSemantics::kCurrent).empty()) {
      continue;
    }
    if (known_username == field->value(autofill::ValueSemantics::kCurrent) ||
        known_password == field->value(autofill::ValueSemantics::kCurrent)) {
      field->set_properties_mask(field->properties_mask() |
                                 autofill::FieldPropertiesFlags::kKnownValue);
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
    base::span<const PasswordForm> matches,
    const std::u16string& username,
    const std::u16string& password) {
  // As the username may have changed, re-compute |username_correction_vote_|.
  username_correction_vote_.reset();
  if (username.empty()) {
    return false;
  }
  for (const PasswordForm& match : matches) {
    if ((match.password_value == password) &&
        FindUsernameInOtherAlternativeUsernames(match, username)) {
      return true;
    }
  }
  return false;
}

std::optional<PasswordAttributesMetadata>
VotesUploader::GeneratePasswordAttributesMetadata(
    const std::u16string& password_value) {
  if (password_value.empty()) {
    NOTREACHED() << "GeneratePasswordAttributesMetadata cannot take an empty "
                    "password value.";
  }

  // Don't crowdsource password attributes for non-ascii passwords.
  for (const auto& e : password_value) {
    if (!(password_manager_util::IsLetter(e) ||
          password_manager_util::IsNumeric(e) ||
          password_manager_util::IsSpecialSymbol(e))) {
      return std::nullopt;
    }
  }

  // Select a character class attribute to upload. Upload special symbols more
  // often (8 in 9 cases) as most issues are due to missing or wrong special
  // symbols. Upload info about letters existence otherwise.
  PasswordAttribute character_class_attribute;
  bool (*predicate)(char16_t c) = nullptr;
  if (base::RandGenerator(9) == 0) {
    predicate = &password_manager_util::IsLetter;
    character_class_attribute = PasswordAttribute::kHasLetter;
  } else {
    predicate = &password_manager_util::IsSpecialSymbol;
    character_class_attribute = PasswordAttribute::kHasSpecialSymbol;
  }

  // Apply the randomized response technique to noisify the actual value
  // (https://en.wikipedia.org/wiki/Randomized_response).
  bool respond_randomly = base::RandGenerator(2);
  bool randomized_value_for_character_class =
      respond_randomly ? base::RandGenerator(2)
                       : base::ranges::any_of(password_value, predicate);
  PasswordAttributesMetadata password_attributes;
  password_attributes.password_attributes_vote = std::make_pair(
      character_class_attribute, randomized_value_for_character_class);

  if (character_class_attribute == PasswordAttribute::kHasSpecialSymbol &&
      randomized_value_for_character_class) {
    password_attributes.password_symbol_vote =
        respond_randomly ? GetRandomSpecialSymbol()
                         : GetRandomSpecialSymbolFromPassword(password_value);
  }

  size_t actual_length = password_value.size();
  password_attributes.password_length_vote =
      actual_length <= 1 || base::RandGenerator(5) == 0
          ? actual_length
          : base::RandGenerator(actual_length - 1) + 1;
  return password_attributes;
}

void VotesUploader::StoreInitialFieldValues(
    const autofill::FormData& observed_form) {
  for (const auto& field : observed_form.fields()) {
    if (!field.value().empty()) {
      initial_values_.insert(
          std::make_pair(field.renderer_id(), field.value()));
    }
  }
}

std::vector<autofill::AutofillUploadContents>
VotesUploader::EncodeUploadRequest(
    autofill::FormStructure& form,
    const autofill::FieldTypeSet& available_field_types,
    std::string_view login_form_signature,
    std::optional<PasswordAttributesMetadata> password_attributes,
    bool should_set_passwords_were_revealed) {
  // Annotate the form with the source language of the page.
  form.set_current_page_language(client_->GetPageLanguage());
  // Attach the Randomized Encoder.
  form.set_randomized_encoder(RandomizedEncoder::Create(client_->GetPrefs()));

  std::vector<AutofillUploadContents> upload_contents =
      autofill::EncodeUploadRequest(form, available_field_types,
                                    login_form_signature,
                                    /*observed_submission=*/true);
  CHECK(!upload_contents.empty());

  upload_contents[0].set_passwords_revealed(
      should_set_passwords_were_revealed && has_passwords_revealed_vote_);

  if (password_attributes) {
    EncodePasswordAttributesMetadata(*password_attributes, upload_contents[0]);
  }

  return upload_contents;
}

bool VotesUploader::SendUploadRequest(
    autofill::FormStructure& form_to_upload,
    const FieldTypeSet& available_field_types,
    const std::string& login_form_signature,
    std::optional<PasswordAttributesMetadata> password_attributes,
    bool should_set_passwords_were_revealed) {
  AutofillCrowdsourcingManager* crowdsourcing_manager =
      client_->GetAutofillCrowdsourcingManager();
  if (!crowdsourcing_manager) {
    return false;
  }

  return crowdsourcing_manager->StartUploadRequest(
      EncodeUploadRequest(form_to_upload, available_field_types,
                          login_form_signature, password_attributes,
                          should_set_passwords_were_revealed),
      form_to_upload.submission_source(),
      /*is_password_manager_upload=*/true);
}

bool VotesUploader::SetSingleUsernameVoteOnUsernameForm(
    AutofillField* field,
    const SingleUsernameVoteData& single_username,
    FieldTypeSet* available_field_types,
    FormSignature form_signature,
    IsMostRecentSingleUsernameCandidate
        is_most_recent_single_username_candidate,
    bool is_forgot_password_vote) {
  FieldType field_type = autofill::UNKNOWN_TYPE;
  autofill::AutofillUploadContents_Field_SingleUsernameVoteType vote_type =
      AutofillUploadContents::Field::DEFAULT;

  // Send a negative vote if the possible username value contains whitespaces.
  if (single_username.username_candidate_value.find(' ') !=
      std::u16string::npos) {
    field_type = autofill::NOT_USERNAME;
    vote_type = AutofillUploadContents::Field::STRONG;
  } else {
    const auto& prompt_edit = single_username.prompt_edit;
    const auto& is_form_overrule = single_username.is_form_overrule;
    // There is no meaningful data on prompt edit, the vote should not be sent.
    if (prompt_edit == AutofillUploadContents::EDIT_UNSPECIFIED) {
      return false;
    }

    if (prompt_edit == AutofillUploadContents::EDITED_POSITIVE ||
        prompt_edit == AutofillUploadContents::NOT_EDITED_POSITIVE) {
      field_type = autofill::SINGLE_USERNAME;
    } else {
      field_type = autofill::NOT_USERNAME;
    }

    if (is_form_overrule) {
      vote_type = AutofillUploadContents::Field::IN_FORM_OVERRULE;
    } else if (prompt_edit == AutofillUploadContents::EDITED_POSITIVE ||
               prompt_edit == AutofillUploadContents::EDITED_NEGATIVE) {
      vote_type = AutofillUploadContents::Field::STRONG;
    } else {
      vote_type = AutofillUploadContents::Field::WEAK;
    }
  }

  if (is_forgot_password_vote) {
    AdjustTypesForForgotPasswordFormVotes(field_type, vote_type);
  }

  CHECK_NE(field_type, autofill::UNKNOWN_TYPE);
  CHECK_NE(vote_type, AutofillUploadContents::Field::DEFAULT);
  available_field_types->insert(field_type);
  field->set_possible_types({field_type});
  field->set_single_username_vote_type(vote_type);
  field->set_is_most_recent_single_username_candidate(
      is_most_recent_single_username_candidate);
  return true;
}

void VotesUploader::SetSingleUsernameVoteOnPasswordForm(
    const SingleUsernameVoteData& vote_data,
    FormStructure& form_structure) {
  AutofillUploadContents::SingleUsernameData single_username_data;
  single_username_data.set_username_form_signature(
      vote_data.form_predictions.form_signature.value());
  single_username_data.set_username_field_signature(
      GetUsernameFieldSignature(vote_data).value());
  single_username_data.set_value_type(vote_data.value_type);
  single_username_data.set_prompt_edit(vote_data.prompt_edit);

  form_structure.AddSingleUsernameData(single_username_data);
}

bool VotesUploader::CalculateInFormOverrule(
    const std::u16string& saved_username,
    const std::u16string& potential_username,
    const AlternativeElementVector& all_alternative_usernames) {
  if (saved_username == suggested_username_) {
    return false;
  }
  if (saved_username == potential_username &&
      IsUsernameInAlternativeUsernames(suggested_username_,
                                       all_alternative_usernames)) {
    // Username found inside of the password form was suggested in the
    // Save/Update prompt. However, user picked some text field outside of the
    // password form as the username - positive IN_FORM_OVERRULE vote must be
    // sent.
    return true;
  }
  if (suggested_username_ == potential_username &&
      IsUsernameInAlternativeUsernames(saved_username,
                                       all_alternative_usernames)) {
    // Username field found outside of the form was suggested in the
    // Save/Update prompt. However, user picked some text field inside of the
    // password form as the username - negative IN_FORM_OVERRULE vote must be
    // sent.
    return true;
  }
  return false;
}

AutofillUploadContents::SingleUsernamePromptEdit
VotesUploader::CalculateUsernamePromptEdit(
    const std::u16string& saved_username,
    const std::u16string& potential_username) {
  AutofillUploadContents::SingleUsernamePromptEdit prompt_edit =
      AutofillUploadContents::EDIT_UNSPECIFIED;
  if (saved_username != suggested_username_) {
    // In this branch, the user edited the username in a prompt before accepting
    // it.

    // The user removed some suggested username and that username wasn't the
    // |potential_username| => this is neither negative nor positive vote. If
    // the user removes |potential_username|, then it is a negative signal and
    // will be reported below.
    if (saved_username.empty() && suggested_username_ != potential_username) {
      return prompt_edit;
    }

    if (saved_username == potential_username) {
      prompt_edit = AutofillUploadContents::EDITED_POSITIVE;
    } else {
      prompt_edit = AutofillUploadContents::EDITED_NEGATIVE;
    }

  } else {  // saved_username == suggested_username
    // In this branch the user did NOT edit the username in prompt and accepted
    // it as it is.

    if (saved_username == potential_username) {
      prompt_edit = AutofillUploadContents::NOT_EDITED_POSITIVE;
    } else {
      prompt_edit = AutofillUploadContents::NOT_EDITED_NEGATIVE;
    }
  }
  return prompt_edit;
}

bool VotesUploader::MaybeSendSingleUsernameVote(
    const SingleUsernameVoteData& single_username,
    const FormPredictions& predictions,
    IsMostRecentSingleUsernameCandidate
        is_most_recent_single_username_candidate,
    bool is_forgot_password_vote) {
  std::vector<FieldSignature> field_signatures;
  for (const auto& field : predictions.fields) {
    field_signatures.push_back(field.signature);
  }

  std::unique_ptr<FormStructure> form_to_upload =
      FormStructure::CreateForPasswordManagerUpload(predictions.form_signature,
                                                    field_signatures);

  // Label the username field with a SINGLE_USERNAME or NOT_USERNAME vote.
  FieldTypeSet available_field_types;
  for (size_t i = 0; i < form_to_upload->field_count(); ++i) {
    AutofillField* field = form_to_upload->field(i);
    FieldRendererId field_renderer_id = predictions.fields[i].renderer_id;

    if (field_renderer_id != single_username.renderer_id) {
      field->set_possible_types({autofill::UNKNOWN_TYPE});
      continue;
    }
    if (!SetSingleUsernameVoteOnUsernameForm(
            field, single_username, &available_field_types,
            predictions.form_signature,
            is_most_recent_single_username_candidate,
            is_forgot_password_vote)) {
      // The single username field has no field type. Don't send vote.
      return false;
    }
  }

  // Upload a vote on the username form if available.
  if (!available_field_types.empty()) {
    if (password_manager_util::IsLoggingActive(client_)) {
      BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
      logger.LogFormStructure(Logger::STRING_USERNAME_FIRST_FLOW_VOTE,
                              *form_to_upload, std::nullopt);
    }

    if (SendUploadRequest(*form_to_upload, available_field_types,
                          /*login_form_signature=*/std::string(),
                          /*password_attributes=*/std::nullopt,
                          /*should_set_passwords_were_revealed=*/false)) {
      return true;
    }
  }
  return false;
}

}  // namespace password_manager
