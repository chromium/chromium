// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::ValueElementPair;
using base::TimeDelta;
using base::TimeTicks;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

bool NewPasswordFormManager::wait_for_server_predictions_for_filling_ = true;

namespace {

constexpr TimeDelta kMaxFillingDelayForServerPredictions =
    TimeDelta::FromMilliseconds(500);

ValueElementPair PasswordToSave(const PasswordForm& form) {
  if (form.new_password_element.empty() || form.new_password_value.empty())
    return {form.password_value, form.password_element};
  return {form.new_password_value, form.new_password_element};
}


// Copies field properties masks from the form |from| to the form |to|.
void CopyFieldPropertiesMasks(const FormData& from, FormData* to) {
  // Skip copying if the number of fields is different.
  if (from.fields.size() != to->fields.size())
    return;

  for (size_t i = 0; i < from.fields.size(); ++i) {
    to->fields[i].properties_mask =
        to->fields[i].name == from.fields[i].name
            ? from.fields[i].properties_mask
            : autofill::FieldPropertiesFlags::ERROR_OCCURRED;
  }
}

// Returns true iff |best_matches| contain a preferred credential with a
// username other than |preferred_username|.
bool DidPreferenceChange(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const base::string16& preferred_username) {
  for (const auto& key_value_pair : best_matches) {
    const PasswordForm& form = *key_value_pair.second;
    if (form.preferred && !form.is_public_suffix_match &&
        form.username_value != preferred_username) {
      return true;
    }
  }
  return false;
}

// Filter sensitive information, duplicates and |username_value| out from
// |form->other_possible_usernames|.
void SanitizePossibleUsernames(PasswordForm* form) {
  auto& usernames = form->other_possible_usernames;

  // Deduplicate.
  std::sort(usernames.begin(), usernames.end());
  usernames.erase(std::unique(usernames.begin(), usernames.end()),
                  usernames.end());

  // Filter out |form->username_value| and sensitive information.
  const base::string16& username_value = form->username_value;
  base::EraseIf(usernames, [&username_value](const ValueElementPair& pair) {
    return pair.first == username_value ||
           autofill::IsValidCreditCardNumber(pair.first) ||
           autofill::IsSSN(pair.first);
  });
}

}  // namespace

NewPasswordFormManager::NewPasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form,
    FormFetcher* form_fetcher,
    std::unique_ptr<FormSaver> form_saver,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : client_(client),
      driver_(driver),
      observed_form_(observed_form),
      metrics_recorder_(metrics_recorder),
      owned_form_fetcher_(
          form_fetcher ? nullptr
                       : std::make_unique<FormFetcherImpl>(
                             PasswordStore::FormDigest(observed_form),
                             client_,
                             true /* should_migrate_http_passwords */,
                             true /* should_query_suppressed_https_forms */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      form_saver_(std::move(form_saver)),
      // TODO(https://crbug.com/831123): set correctly
      // |is_possible_change_password_form| in |votes_uploader_| constructor
      votes_uploader_(client, false /* is_possible_change_password_form */),
      weak_ptr_factory_(this) {
  if (!metrics_recorder_) {
    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_->IsMainFrameSecure(), client_->GetUkmSourceId());
  }
  metrics_recorder_->RecordFormSignature(CalculateFormSignature(observed_form));

  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);

  // The following code is for development and debugging purposes.
  // TODO(https://crbug.com/831123): remove it when NewPasswordFormManager will
  // be production ready.
  if (password_manager_util::IsLoggingActive(client_))
    ParseFormAndMakeLogging(observed_form_, FormDataParser::Mode::kFilling);
}
NewPasswordFormManager::~NewPasswordFormManager() = default;

bool NewPasswordFormManager::DoesManage(
    const autofill::FormData& form,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;

  if (observed_form_.is_form_tag != form.is_form_tag)
    return false;
  // All unowned input elements are considered as one synthetic form.
  if (!observed_form_.is_form_tag && !form.is_form_tag)
    return true;
  return observed_form_.unique_renderer_id == form.unique_renderer_id;
}

bool NewPasswordFormManager::IsEqualToSubmittedForm(
    const autofill::FormData& form) const {
  if (!is_submitted_)
    return false;
  if (form.action == submitted_form_.action)
    return true;
  // TODO(https://crbug.com/831123): Implement other checks from a function
  // IsPasswordFormReappeared from password_manager.cc.
  return false;
}

FormFetcher* NewPasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

const GURL& NewPasswordFormManager::GetOrigin() const {
  return observed_form_.origin;
}

const std::map<base::string16, const PasswordForm*>&
NewPasswordFormManager::GetBestMatches() const {
  return best_matches_;
}

const PasswordForm& NewPasswordFormManager::GetPendingCredentials() const {
  return pending_credentials_;
}

metrics_util::CredentialSourceType
NewPasswordFormManager::GetCredentialSource() {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* NewPasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

const std::vector<const PasswordForm*>&
NewPasswordFormManager::GetBlacklistedMatches() const {
  return blacklisted_matches_;
}

bool NewPasswordFormManager::IsBlacklisted() const {
  return !blacklisted_matches_.empty();
}

bool NewPasswordFormManager::IsPasswordOverridden() const {
  return password_overridden_;
}

const PasswordForm* NewPasswordFormManager::GetPreferredMatch() const {
  return preferred_match_;
}

void NewPasswordFormManager::Save() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(!client_->IsIncognito());

  // TODO(https://crbug.com/831123): Implement indicator event metrics.
  if (user_action_ == UserAction::kNone &&
      DidPreferenceChange(best_matches_, pending_credentials_.username_value)) {
    SetUserAction(UserAction::kChoose);
  }
  if (user_action_ == UserAction::kOverridePassword &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED &&
      !has_generated_password_) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::TYPE_MANUAL;
  }

  if (is_new_login_) {
    SanitizePossibleUsernames(&pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
    votes_uploader_.SendVotesOnSave(observed_form_, *parsed_submitted_form_,
                                    best_matches_, &pending_credentials_);
    form_saver_->Save(pending_credentials_, best_matches_);
  } else {
    ProcessUpdate();
    std::vector<PasswordForm> credentials_to_update =
        FindOtherCredentialsToUpdate();
    form_saver_->Update(pending_credentials_, best_matches_,
                        &credentials_to_update, nullptr);
  }

  if (pending_credentials_.times_used == 1 &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED) {
    // This also includes PSL matched credentials.
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_USED);
  }

  // TODO(https://crbug.com/831123): Implement updating Password Form Managers.
}

void NewPasswordFormManager::Update(const PasswordForm& credentials_to_update) {
}

void NewPasswordFormManager::UpdateUsername(
    const base::string16& new_username) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->username_value = new_username;
  parsed_submitted_form_->username_element.clear();

  // TODO(https://crbug.com/831123): Implement processing username editing votes
  // after implementation of |other_possible_usernames|.

  CreatePendingCredentials();
}

void NewPasswordFormManager::UpdatePasswordValue(
    const base::string16& new_password) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->password_value = new_password;
  parsed_submitted_form_->password_element.clear();

  // TODO(https://crbug.com/831123): Implement processing password editing votes
  // after implementation of |all_possible_passwords|.
  CreatePendingCredentials();
}

// TODO(https://crbug.com/831123): Implement all methods from
// PasswordFormManagerForUI.
void NewPasswordFormManager::OnNopeUpdateClicked() {}

void NewPasswordFormManager::OnNeverClicked() {
  PermanentlyBlacklist();
}

void NewPasswordFormManager::OnNoInteraction(bool is_update) {}

void NewPasswordFormManager::PermanentlyBlacklist() {
  DCHECK(!client_->IsIncognito());

  if (!new_blacklisted_) {
    new_blacklisted_ = std::make_unique<PasswordForm>();
    new_blacklisted_->origin = observed_form_.origin;
    new_blacklisted_->signon_realm = GetSignonRealm(observed_form_.origin);
    blacklisted_matches_.push_back(new_blacklisted_.get());
  }
  form_saver_->PermanentlyBlacklist(new_blacklisted_.get());
}

void NewPasswordFormManager::OnPasswordsRevealed() {}

bool NewPasswordFormManager::IsNewLogin() const {
  return is_new_login_;
}

bool NewPasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.is_public_suffix_match;
}

void NewPasswordFormManager::PresaveGeneratedPassword(
    const PasswordForm& form) {
  std::unique_ptr<PasswordForm> parsed_form =
      ParseFormAndMakeLogging(form.form_data, FormDataParser::Mode::kSaving);

  if (!parsed_form)
    return;

  // Clear the username value if there are already saved credentials with the
  // same username in order to prevent overwriting.
  if (base::ContainsKey(best_matches_, parsed_form->username_value))
    parsed_form->username_value.clear();

  form_saver_->PresaveGeneratedPassword(*parsed_form);

  // If a password had been generated already, a call to
  // PresaveGeneratedPassword() implies that this password was modified.
  if (!has_generated_password_) {
    votes_uploader_.set_generated_password_changed(false);
    metrics_recorder_->SetGeneratedPasswordStatus(
        PasswordFormMetricsRecorder::GeneratedPasswordStatus::
            kPasswordAccepted);
  } else {
    votes_uploader_.set_generated_password_changed(true);
    metrics_recorder_->SetGeneratedPasswordStatus(
        PasswordFormMetricsRecorder::GeneratedPasswordStatus::kPasswordEdited);
  }
  has_generated_password_ = true;
  votes_uploader_.set_has_generated_password(true);
}

void NewPasswordFormManager::PasswordNoLongerGenerated() {
  DCHECK(has_generated_password_);
  form_saver_->RemovePresavedPassword();
  has_generated_password_ = false;
  votes_uploader_.set_has_generated_password(false);
  votes_uploader_.set_generated_password_changed(false);
  metrics_recorder_->SetGeneratedPasswordStatus(
      PasswordFormMetricsRecorder::GeneratedPasswordStatus::kPasswordDeleted);
}

bool NewPasswordFormManager::HasGeneratedPassword() const {
  return has_generated_password_;
}

void NewPasswordFormManager::SetGenerationPopupWasShown(
    bool generation_popup_was_shown,
    bool is_manual_generation) {
  votes_uploader_.set_generation_popup_was_shown(generation_popup_was_shown);
  votes_uploader_.set_is_manual_generation(is_manual_generation);
  metrics_recorder_->SetPasswordGenerationPopupShown(generation_popup_was_shown,
                                                     is_manual_generation);
}

void NewPasswordFormManager::SetGenerationElement(
    const base::string16& generation_element) {
  votes_uploader_.set_generation_element(generation_element);
}

bool NewPasswordFormManager::IsPossibleChangePasswordFormWithoutUsername()
    const {
  // TODO(https://crbug.com/831123): Implement as in PasswordFormManager.
  return false;
}

bool NewPasswordFormManager::RetryPasswordFormPasswordUpdate() const {
  // TODO(https://crbug.com/831123): Implement as in PasswordFormManager.
  return false;
}

std::vector<base::WeakPtr<PasswordManagerDriver>>
NewPasswordFormManager::GetDrivers() const {
  return {driver_};
}

const PasswordForm* NewPasswordFormManager::GetSubmittedForm() const {
  return parsed_submitted_form_.get();
}

std::unique_ptr<NewPasswordFormManager> NewPasswordFormManager::Clone() {
  // Fetcher is cloned to avoid re-fetching data from PasswordStore.
  std::unique_ptr<FormFetcher> fetcher = form_fetcher_->Clone();

  // Some data is filled through the constructor. No PasswordManagerDriver is
  // needed, because the UI does not need any functionality related to the
  // renderer process, to which the driver serves as an interface. The full
  // |observed_form_| needs to be copied, because it is used to create the
  // blacklisting entry if needed.
  auto result = std::make_unique<NewPasswordFormManager>(
      client_, base::WeakPtr<PasswordManagerDriver>(), observed_form_,
      fetcher.get(), form_saver_->Clone(), metrics_recorder_);

  // The constructor only can take a weak pointer to the fetcher, so moving the
  // owning one needs to happen explicitly.
  result->owned_form_fetcher_ = std::move(fetcher);

  // These data members all satisfy:
  //   (1) They could have been changed by |*this| between its construction and
  //       calling Clone().
  //   (2) They are potentially used in the clone as the clone is used in the UI
  //       code.
  //   (3) They are not changed during ProcessMatches, triggered at some point
  //       by the cloned FormFetcher.
  result->has_generated_password_ = has_generated_password_;
  result->user_action_ = user_action_;
  result->votes_uploader_ = votes_uploader_;
  if (parser_.predictions())
    result->parser_.set_predictions(*parser_.predictions());

  return result;
}

void NewPasswordFormManager::ProcessMatches(
    const std::vector<const PasswordForm*>& non_federated,
    size_t filtered_count) {
  received_stored_credentials_time_ = TimeTicks::Now();
  std::vector<const PasswordForm*> matches;
  std::copy_if(non_federated.begin(), non_federated.end(),
               std::back_inserter(matches), [](const PasswordForm* form) {
                 return !form->blacklisted_by_user &&
                        form->scheme == PasswordForm::SCHEME_HTML;
               });

  password_manager_util::FindBestMatches(matches, &best_matches_,
                                         &not_best_matches_, &preferred_match_);

  // Copy out blacklisted matches.
  blacklisted_matches_.clear();
  new_blacklisted_.reset();
  std::copy_if(
      non_federated.begin(), non_federated.end(),
      std::back_inserter(blacklisted_matches_), [](const PasswordForm* form) {
        return form->blacklisted_by_user && !form->is_public_suffix_match;
      });

  autofills_left_ = kMaxTimesAutofill;

  if (parser_.predictions() || !wait_for_server_predictions_for_filling_) {
    ReportTimeBetweenStoreAndServerUMA();
    Fill();
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NewPasswordFormManager::Fill,
                       weak_ptr_factory_.GetWeakPtr()),
        kMaxFillingDelayForServerPredictions);
  }
}

bool NewPasswordFormManager::SetSubmittedFormIfIsManaged(
    const autofill::FormData& submitted_form,
    const PasswordManagerDriver* driver) {
  if (!DoesManage(submitted_form, driver))
    return false;
  parsed_submitted_form_ =
      ParseFormAndMakeLogging(submitted_form, FormDataParser::Mode::kSaving);

  RecordMetricOnReadonly(parser_.readonly_status(), !!parsed_submitted_form_,
                         FormDataParser::Mode::kSaving);
  if (!parsed_submitted_form_)
    return false;

  submitted_form_ = submitted_form;
  is_submitted_ = true;

  CreatePendingCredentials();
  return true;
}

void NewPasswordFormManager::ProcessServerPredictions(
    const std::vector<FormStructure*>& predictions) {
  FormSignature observed_form_signature =
      CalculateFormSignature(observed_form_);
  for (const FormStructure* form_predictions : predictions) {
    if (form_predictions->form_signature() != observed_form_signature)
      continue;
    ReportTimeBetweenStoreAndServerUMA();
    parser_.set_predictions(ConvertToFormPredictions(*form_predictions));
    Fill();
    break;
  }
}

void NewPasswordFormManager::Fill() {
  if (autofills_left_ <= 0)
    return;
  autofills_left_--;

  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  std::unique_ptr<PasswordForm> observed_password_form =
      ParseFormAndMakeLogging(observed_form_, FormDataParser::Mode::kFilling);
  RecordMetricOnReadonly(parser_.readonly_status(), !!observed_password_form,
                         FormDataParser::Mode::kFilling);
  if (!observed_password_form)
    return;

  RecordMetricOnCompareParsingResult(*observed_password_form);

  // TODO(https://crbug.com/831123). Move this lines to the beginning of the
  // function when the old parsing is removed.
  if (!driver_)
    return;

  // TODO(https://crbug.com/831123). Implement correct treating of federated
  // matches.
  std::vector<const PasswordForm*> federated_matches;
  SendFillInformationToRenderer(*client_, driver_.get(), IsBlacklisted(),
                                *observed_password_form.get(), best_matches_,
                                federated_matches, preferred_match_,
                                metrics_recorder_.get());
}

void NewPasswordFormManager::RecordMetricOnCompareParsingResult(
    const PasswordForm& parsed_form) {
  bool same =
      parsed_form.username_element == old_parsing_result_.username_element &&
      parsed_form.password_element == old_parsing_result_.password_element &&
      parsed_form.new_password_element ==
          old_parsing_result_.new_password_element &&
      parsed_form.confirmation_password_element ==
          old_parsing_result_.confirmation_password_element;
  if (same) {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kSame);
    return;
  }

  // In the old parsing for fields with empty name, placeholders are used. The
  // reason for this is that an empty "..._element" attribute in a PasswordForm
  // means that no corresponding input element exists. The new form parsing sets
  // empty string in that case because renderer ids are used instead of element
  // names for fields identification. Hence in case of anonymous fields, the
  // results will be different for sure. Compare to placeholders and record this
  // case.
  if (old_parsing_result_.username_element ==
          base::ASCIIToUTF16("anonymous_username") ||
      old_parsing_result_.password_element ==
          base::ASCIIToUTF16("anonymous_password") ||
      old_parsing_result_.new_password_element ==
          base::ASCIIToUTF16("anonymous_new_password") ||
      old_parsing_result_.confirmation_password_element ==
          base::ASCIIToUTF16("anonymous_confirmation_password")) {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kAnonymousFields);
  } else {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kDifferent);
  }
}

void NewPasswordFormManager::RecordMetricOnReadonly(
    FormDataParser::ReadonlyPasswordFields readonly_status,
    bool parsing_successful,
    FormDataParser::Mode mode) {
  // The reported value is combined of the |readonly_status| shifted by one bit
  // to the left, and the success bit put in the least significant bit. Note:
  // C++ guarantees that bool->int conversions map false to 0 and true to 1.
  uint64_t value = static_cast<uint64_t>(parsing_successful) +
                   (static_cast<uint64_t>(readonly_status) << 1);
  switch (mode) {
    case FormDataParser::Mode::kSaving:
      metrics_recorder_->RecordReadonlyWhenSaving(value);
      break;
    case FormDataParser::Mode::kFilling:
      metrics_recorder_->RecordReadonlyWhenFilling(value);
      break;
  }
}

void NewPasswordFormManager::ReportTimeBetweenStoreAndServerUMA() {
  if (!received_stored_credentials_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PasswordManager.TimeBetweenStoreAndServer",
                        TimeTicks::Now() - received_stored_credentials_time_);
  }
}

void NewPasswordFormManager::CreatePendingCredentials() {
  DCHECK(is_submitted_);
  // TODO(https://crbug.com/831123): Process correctly the case when saved
  // credentials are not received from the store yet.
  if (!parsed_submitted_form_)
    return;

  ValueElementPair password_to_save(PasswordToSave(*parsed_submitted_form_));

  // Look for the actually submitted credentials in the list of previously saved
  // credentials that were available to autofilling.
  const PasswordForm* saved_form =
      FindBestSavedMatch(parsed_submitted_form_.get());
  if (saved_form) {
    // The user signed in with a login we autofilled.
    pending_credentials_ = *saved_form;
    SetPasswordOverridden(pending_credentials_.password_value !=
                          password_to_save.first);

    if (pending_credentials_.is_public_suffix_match) {
      // If the autofilled credentials were a PSL match or credentials stored
      // from Android apps, store a copy with the current origin and signon
      // realm. This ensures that on the next visit, a precise match is found.
      is_new_login_ = true;
      SetUserAction(password_overridden_ ? UserAction::kOverridePassword
                                         : UserAction::kChoosePslMatch);

      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

      // Update |pending_credentials_| in order to be able correctly save it.
      pending_credentials_.origin = submitted_form_.origin;
      pending_credentials_.signon_realm = parsed_submitted_form_->signon_realm;

      // Normally, the copy of the PSL matched credentials, adapted for the
      // current domain, is saved automatically without asking the user, because
      // the copy likely represents the same account, i.e., the one for which
      // the user already agreed to store a password.
      //
      // However, if the user changes the suggested password, it might indicate
      // that the autofilled credentials and |submitted_password_form|
      // actually correspond to two different accounts (see
      // http://crbug.com/385619). In that case the user should be asked again
      // before saving the password. This is ensured by setting
      // |password_overriden_| on |pending_credentials_| to false.
      //
      // There is still the edge case when the autofilled credentials represent
      // the same account as |submitted_password_form| but the stored password
      // was out of date. In that case, the user just had to manually enter the
      // new password, which is now in |submitted_password_form|. The best
      // thing would be to save automatically, and also update the original
      // credentials. However, we have no way to tell if this is the case.
      // This will likely happen infrequently, and the inconvenience put on the
      // user by asking them is not significant, so we are fine with asking
      // here again.
      if (password_overridden_) {
        pending_credentials_.is_public_suffix_match = false;
        SetPasswordOverridden(false);
      }
    } else {  // Not a PSL match but a match of an already stored credential.
      is_new_login_ = false;
      if (password_overridden_)
        SetUserAction(UserAction::kOverridePassword);
    }
  } else if (!best_matches_.empty() &&
             parsed_submitted_form_->type != PasswordForm::TYPE_API &&
             parsed_submitted_form_->username_value.empty()) {
    // This branch deals with the case that the submitted form has no username
    // element and needs to decide whether to offer to update any credentials.
    // In that case, the user can select any previously stored credential as
    // the one to update, but we still try to find the best candidate.

    // Find the best candidate to select by default in the password update
    // bubble. If no best candidate is found, any one can be offered.
    const PasswordForm* best_update_match =
        FindBestMatchForUpdatePassword(parsed_submitted_form_->password_value);

    // A retry password form is one that consists of only an "old password"
    // field, i.e. one that is not a "new password".
    retry_password_form_password_update_ =
        parsed_submitted_form_->username_value.empty() &&
        parsed_submitted_form_->new_password_value.empty();

    is_new_login_ = false;
    if (best_update_match) {
      // Chose |best_update_match| to be updated.
      pending_credentials_ = *best_update_match;
    } else if (has_generated_password_) {
      // If a password was generated and we didn't find a match, we have to save
      // it in a separate entry since we have to store it but we don't know
      // where.
      CreatePendingCredentialsForNewCredentials(*parsed_submitted_form_,
                                                password_to_save.second);
      is_new_login_ = true;
    } else {
      // We don't have a good candidate to choose as the default credential for
      // the update bubble and the user has to pick one.
      // We set |pending_credentials_| to the bare minimum, which is the correct
      // origin.
      pending_credentials_.origin = submitted_form_.origin;
    }
  } else {
    is_new_login_ = true;
    // No stored credentials can be matched to the submitted form. Offer to
    // save new credentials.
    CreatePendingCredentialsForNewCredentials(*parsed_submitted_form_,
                                              password_to_save.second);
    // Generate username correction votes.
    bool username_correction_found =
        votes_uploader_.FindCorrectedUsernameElement(
            best_matches_, not_best_matches_,
            parsed_submitted_form_->username_value,
            parsed_submitted_form_->password_value);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.UsernameCorrectionFound",
                          username_correction_found);
    if (username_correction_found) {
      metrics_recorder_->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kCorrectedUsernameInForm);
    }
  }

  if (!IsValidAndroidFacetURI(pending_credentials_.signon_realm))
    pending_credentials_.action = submitted_form_.action;

  pending_credentials_.password_value = password_to_save.first;
  pending_credentials_.preferred = true;
  pending_credentials_.form_has_autofilled_value =
      parsed_submitted_form_->form_has_autofilled_value;
  pending_credentials_.all_possible_passwords =
      parsed_submitted_form_->all_possible_passwords;
  CopyFieldPropertiesMasks(submitted_form_, &pending_credentials_.form_data);

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (parsed_submitted_form_->type == PasswordForm::TYPE_API) {
    pending_credentials_.skip_zero_click =
        parsed_submitted_form_->skip_zero_click;
    pending_credentials_.display_name = parsed_submitted_form_->display_name;
    pending_credentials_.federation_origin =
        parsed_submitted_form_->federation_origin;
    pending_credentials_.icon_url = parsed_submitted_form_->icon_url;
    // It's important to override |signon_realm| for federated credentials
    // because it has format "federation://" + origin_host + "/" +
    // federation_host
    pending_credentials_.signon_realm = parsed_submitted_form_->signon_realm;
  }

  if (has_generated_password_)
    pending_credentials_.type = PasswordForm::TYPE_GENERATED;
}

const PasswordForm* NewPasswordFormManager::FindBestMatchForUpdatePassword(
    const base::string16& password) const {
  // This function is called for forms that do not contain a username field.
  // This means that we cannot update credentials based on a matching username
  // and that we may need to show an update prompt.
  if (best_matches_.size() == 1 && !has_generated_password_) {
    // In case the submitted form contained no username but a password, and if
    // the user has only one credential stored, return it as the one that should
    // be updated.
    return best_matches_.begin()->second;
  }
  if (password.empty())
    return nullptr;

  // Return any existing credential that has the same |password| saved already.
  for (const auto& key_value : best_matches_) {
    if (key_value.second->password_value == password)
      return key_value.second;
  }
  return nullptr;
}

const PasswordForm* NewPasswordFormManager::FindBestSavedMatch(
    const PasswordForm* submitted_form) const {
  if (!submitted_form->federation_origin.opaque())
    return nullptr;

  // Return form with matching |username_value|.
  auto it = best_matches_.find(submitted_form->username_value);
  if (it != best_matches_.end())
    return it->second;

  // Match Credential API forms only by username. Stop here if nothing was found
  // above.
  if (submitted_form->type == PasswordForm::TYPE_API)
    return nullptr;

  // Verify that the submitted form has no username and no "new password"
  // and bail out with a nullptr otherwise.
  bool submitted_form_has_username = !submitted_form->username_value.empty();
  bool submitted_form_has_new_password_element =
      !submitted_form->new_password_value.empty();
  if (submitted_form_has_username || submitted_form_has_new_password_element)
    return nullptr;

  // At this line we are certain that the submitted form contains only a
  // password field that is not a "new password". Now we can check whether we
  // have a match by password of an already saved credential.
  for (const auto& stored_match : best_matches_) {
    if (stored_match.second->password_value == submitted_form->password_value)
      return stored_match.second;
  }
  return nullptr;
}

void NewPasswordFormManager::CreatePendingCredentialsForNewCredentials(
    const PasswordForm& submitted_password_form,
    const base::string16& password_element) {
  // User typed in a new, unknown username.
  SetUserAction(UserAction::kOverrideUsernameAndPassword);
  // TODO(https://crbug.com/831123): Replace parsing of the observed form with
  // usage of already parsed submitted form.
  std::unique_ptr<PasswordForm> parsed_observed_form =
      ParseFormAndMakeLogging(observed_form_, FormDataParser::Mode::kFilling);
  if (!parsed_observed_form)
    return;
  pending_credentials_ = *parsed_observed_form;
  pending_credentials_.username_element =
      submitted_password_form.username_element;
  pending_credentials_.username_value = submitted_password_form.username_value;
  pending_credentials_.other_possible_usernames =
      submitted_password_form.other_possible_usernames;
  pending_credentials_.all_possible_passwords =
      submitted_password_form.all_possible_passwords;

  // The password value will be filled in later, remove any garbage for now.
  pending_credentials_.password_value.clear();
  // The password element should be determined earlier in |PasswordToSave|.
  pending_credentials_.password_element = password_element;
  // The new password's value and element name should be empty.
  pending_credentials_.new_password_value.clear();
  pending_credentials_.new_password_element.clear();
}

void NewPasswordFormManager::SetUserAction(UserAction user_action) {
  user_action_ = user_action;
  metrics_recorder_->SetUserAction(user_action);
}

void NewPasswordFormManager::ProcessUpdate() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(preferred_match_ || !pending_credentials_.federation_origin.opaque());
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(!is_new_login_ && pending_credentials_.preferred);
  DCHECK(!client_->IsIncognito());
  DCHECK(parsed_submitted_form_);

  password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

  base::RecordAction(
      base::UserMetricsAction("PasswordManager_LoginFollowingAutofill"));

  // Check to see if this form is a candidate for password generation.
  // Do not send votes on change password forms, since they were already sent in
  // Update() method.
  if (!parsed_submitted_form_->IsPossibleChangePasswordForm()) {
    votes_uploader_.SendVoteOnCredentialsReuse(
        observed_form_, *parsed_submitted_form_, &pending_credentials_);
  }

  if (pending_credentials_.times_used == 1) {
    votes_uploader_.UploadFirstLoginVotes(best_matches_, pending_credentials_,
                                          *parsed_submitted_form_);
  }
}

std::vector<PasswordForm>
NewPasswordFormManager::FindOtherCredentialsToUpdate() {
  std::vector<autofill::PasswordForm> credentials_to_update;
  if (!pending_credentials_.federation_origin.opaque())
    return credentials_to_update;

  auto updated_password_it =
      best_matches_.find(pending_credentials_.username_value);
  DCHECK(best_matches_.end() != updated_password_it);
  const base::string16& old_password =
      updated_password_it->second->password_value;
  for (auto* not_best_match : not_best_matches_) {
    if (not_best_match->username_value == pending_credentials_.username_value &&
        not_best_match->password_value == old_password) {
      credentials_to_update.push_back(*not_best_match);
      credentials_to_update.back().password_value =
          pending_credentials_.password_value;
    }
  }

  return credentials_to_update;
}

std::unique_ptr<PasswordForm> NewPasswordFormManager::ParseFormAndMakeLogging(
    const FormData& form,
    FormDataParser::Mode mode) {
  std::unique_ptr<PasswordForm> password_form = parser_.Parse(form, mode);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form)
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
  }
  return password_form;
}

}  // namespace password_manager
