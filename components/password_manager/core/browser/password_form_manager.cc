// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::ValueElementPair;
using base::Time;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace password_manager {

namespace {

// This namespace is for fixing Jumbo builds, because the same functions are
// defined in new_password_form_manager.cc.
namespace password_form_manager_helpers {

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

}  // namespace password_form_manager_helpers

bool DoesStringContainOnlyDigits(const base::string16& s) {
  for (auto c : s) {
    if (!base::IsAsciiDigit(c))
      return false;
  }
  return true;
}

// Heuristics to determine that a string is very unlikely to be a username.
bool IsProbablyNotUsername(const base::string16& s) {
  return !s.empty() && DoesStringContainOnlyDigits(s) && s.size() < 3;
}

// Copies field properties masks from the form |from| to the form |to|.
void CopyFieldPropertiesMasks(const PasswordForm& from, PasswordForm* to) {
  // Skip copying if the number of fields is different.
  if (from.form_data.fields.size() != to->form_data.fields.size())
    return;

  for (size_t i = 0; i < from.form_data.fields.size(); ++i) {
    to->form_data.fields[i].properties_mask =
        to->form_data.fields[i].name == from.form_data.fields[i].name
            ? from.form_data.fields[i].properties_mask
            : autofill::FieldPropertiesFlags::ERROR_OCCURRED;
  }
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManager* password_manager,
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const PasswordForm& observed_form,
    std::unique_ptr<FormSaver> form_saver,
    FormFetcher* form_fetcher)
    : observed_form_(observed_form),
      observed_form_signature_(CalculateFormSignature(observed_form.form_data)),
      is_new_login_(true),
      has_generated_password_(false),
      password_overridden_(false),
      retry_password_form_password_update_(false),
      password_manager_(password_manager),
      preferred_match_(nullptr),
      is_possible_change_password_form_without_username_(
          observed_form.IsPossibleChangePasswordFormWithoutUsername()),
      client_(client),
      user_action_(UserAction::kNone),
      form_saver_(std::move(form_saver)),
      owned_form_fetcher_(
          form_fetcher ? nullptr
                       : std::make_unique<FormFetcherImpl>(
                             PasswordStore::FormDigest(observed_form),
                             client,
                             true /* should_migrate_http_passwords */,
                             true /* should_query_suppressed_https_forms */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      votes_uploader_(client, observed_form.IsPossibleChangePasswordForm()) {
  // Non-HTML forms should not need any interaction with the renderer, and hence
  // no driver. Note that cloned PasswordFormManager instances can have HTML
  // forms without drivers as well.
  DCHECK((observed_form.scheme == PasswordForm::SCHEME_HTML) ||
         (driver == nullptr))
      << observed_form.scheme;
  if (driver)
    drivers_.push_back(driver);
}

void PasswordFormManager::Init(
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder) {
  DCHECK(!metrics_recorder_) << "Do not call Init twice.";
  metrics_recorder_ = std::move(metrics_recorder);
  if (!metrics_recorder_) {
    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_->IsMainFrameSecure(), client_->GetUkmSourceId());
  }

  metrics_recorder_->RecordFormSignature(observed_form_signature_);

  if (owned_form_fetcher_ &&
      !observed_form_.is_gaia_with_skip_save_password_form) {
    owned_form_fetcher_->Fetch();
  }
  form_fetcher_->AddConsumer(this);
}

PasswordFormManager::~PasswordFormManager() {
  form_fetcher_->RemoveConsumer(this);

  metrics_recorder_->RecordHistogramsOnSuppressedAccounts(
      observed_form_.origin.SchemeIsCryptographic(), *form_fetcher_,
      pending_credentials_);
}

// static
ValueElementPair PasswordFormManager::PasswordToSave(const PasswordForm& form) {
  if (form.new_password_element.empty() || form.new_password_value.empty())
    return {form.password_value, form.password_element};
  return {form.new_password_value, form.new_password_element};
}

// TODO(crbug.com/700420): Refactor this function, to make comparison more
// reliable.
PasswordFormManager::MatchResultMask PasswordFormManager::DoesManage(
    const PasswordForm& form,
    const password_manager::PasswordManagerDriver* driver) const {
  // Non-HTML form case.
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML ||
      form.scheme != PasswordForm::SCHEME_HTML) {
    const bool forms_match = observed_form_.signon_realm == form.signon_realm &&
                             observed_form_.scheme == form.scheme;
    return forms_match ? RESULT_COMPLETE_MATCH : RESULT_NO_MATCH;
  }

  // HTML form case.
  MatchResultMask result = RESULT_NO_MATCH;

  if (observed_form_.signon_realm != form.signon_realm)
    return result;

  // Easiest case of matching origins.
  bool origins_match = form.origin == observed_form_.origin;
  // If this is a replay of the same form in the case a user entered an invalid
  // password, the origin of the new form may equal the action of the "first"
  // form instead.
  origins_match = origins_match || (form.origin == observed_form_.action);
  // Otherwise, if action hosts are the same, the old URL scheme is HTTP while
  // the new one is HTTPS, and the new path equals to or extends the old path,
  // we also consider the actions a match. This is to accommodate cases where
  // the original login form is on an HTTP page, but a failed login attempt
  // redirects to HTTPS (as in http://example.org -> https://example.org/auth).
  if (!origins_match && !observed_form_.origin.SchemeIsCryptographic() &&
      form.origin.SchemeIsCryptographic()) {
    const base::StringPiece& old_path = observed_form_.origin.path_piece();
    const base::StringPiece& new_path = form.origin.path_piece();
    origins_match =
        observed_form_.origin.host_piece() == form.origin.host_piece() &&
        observed_form_.origin.port() == form.origin.port() &&
        base::StartsWith(new_path, old_path, base::CompareCase::SENSITIVE);
  }

  if (driver)
    origins_match =
        origins_match ||
        std::any_of(drivers_.begin(), drivers_.end(),
                    [driver](const base::WeakPtr<PasswordManagerDriver>& d) {
                      return d.get() == driver;
                    });

  if (!origins_match)
    return result;

  result |= RESULT_ORIGINS_OR_FRAMES_MATCH;

  if (CalculateFormSignature(form.form_data) == observed_form_signature_)
    result |= RESULT_SIGNATURE_MATCH;

  if (form.form_data.name == observed_form_.form_data.name)
    result |= RESULT_FORM_NAME_MATCH;

  // Note: although saved password forms might actually have an empty action
  // URL if they were imported (see bug 1107719), the |form| we see here comes
  // never from the password store, and should have an exactly matching action.
  if (form.action == observed_form_.action)
    result |= RESULT_ACTION_MATCH;

  return result;
}

void PasswordFormManager::PermanentlyBlacklist() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(!client_->IsIncognito());

  if (!new_blacklisted_) {
    new_blacklisted_ = std::make_unique<PasswordForm>(observed_form_);
    blacklisted_matches_.push_back(new_blacklisted_.get());
  }
  form_saver_->PermanentlyBlacklist(new_blacklisted_.get());
}

bool PasswordFormManager::IsNewLogin() const {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  return is_new_login_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.is_public_suffix_match;
}

void PasswordFormManager::ProvisionallySave(const PasswordForm& credentials) {
  std::unique_ptr<autofill::PasswordForm> mutable_submitted_form(
      new PasswordForm(credentials));
  if (credentials.IsPossibleChangePasswordForm() &&
      !credentials.username_value.empty() &&
      IsProbablyNotUsername(credentials.username_value)) {
    mutable_submitted_form->username_value.clear();
    mutable_submitted_form->username_element.clear();
    is_possible_change_password_form_without_username_ = true;
  }
  submitted_form_ = std::move(mutable_submitted_form);

  if (form_fetcher_->GetState() == FormFetcher::State::NOT_WAITING)
    CreatePendingCredentials();
}

void PasswordFormManager::Save() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(!client_->IsIncognito());

  metrics_util::LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
      submitted_form_->submission_event);
  metrics_recorder_->SetSubmissionIndicatorEvent(
      submitted_form_->submission_event);

  if ((user_action_ == UserAction::kNone) &&
      password_form_manager_helpers::DidPreferenceChange(
          best_matches_, pending_credentials_.username_value)) {
    SetUserAction(UserAction::kChoose);
  }
  if (user_action_ == UserAction::kOverridePassword &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED &&
      !HasGeneratedPassword()) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::TYPE_MANUAL;
  }

  if (is_new_login_) {
    UMA_HISTOGRAM_BOOLEAN(
        "PasswordManager.NewlySavedPasswordIsGenerated",
        pending_credentials_.type == PasswordForm::TYPE_GENERATED);
    password_form_manager_helpers::SanitizePossibleUsernames(
        &pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
    votes_uploader_.SendVotesOnSave(observed_form_.form_data, *submitted_form_,
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

  password_manager_->UpdateFormManagers();
}

void PasswordFormManager::Update(
    const autofill::PasswordForm& credentials_to_update) {
  metrics_util::LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
      submitted_form_->submission_event);
  metrics_recorder_->SetSubmissionIndicatorEvent(
      submitted_form_->submission_event);
  if (observed_form_.IsPossibleChangePasswordForm()) {
    FormStructure form_structure(credentials_to_update.form_data);
    votes_uploader_.UploadPasswordVote(observed_form_, *submitted_form_,
                                       autofill::NEW_PASSWORD,
                                       form_structure.FormSignatureAsStr());
  }
  base::string16 password_to_save = pending_credentials_.password_value;
  bool skip_zero_click = pending_credentials_.skip_zero_click;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.skip_zero_click = skip_zero_click;
  pending_credentials_.preferred = true;
  is_new_login_ = false;
  ProcessUpdate();
  std::vector<PasswordForm> more_credentials_to_update =
      FindOtherCredentialsToUpdate();
  form_saver_->Update(pending_credentials_, best_matches_,
                      &more_credentials_to_update, nullptr);

  password_manager_->UpdateFormManagers();
}

void PasswordFormManager::UpdateUsername(const base::string16& new_username) {
  PasswordForm credential(*submitted_form_);
  credential.username_value = new_username;
  // If |new_username| is not found in |other_possible_usernames|, store empty
  // |username_element|.
  credential.username_element.clear();

  // |has_username_edited_vote_| is true iff |new_username| was typed in another
  // field. Otherwise, |has_username_edited_vote_| is false and no vote will be
  // uploaded.
  votes_uploader_.set_has_username_edited_vote(false);
  if (!new_username.empty()) {
    for (size_t i = 0; i < credential.other_possible_usernames.size(); ++i) {
      if (credential.other_possible_usernames[i].first == new_username) {
        credential.username_element =
            credential.other_possible_usernames[i].second;

        credential.other_possible_usernames.erase(
            credential.other_possible_usernames.begin() + i);

        // Set |corrected_username_element_| to upload a username vote.
        votes_uploader_.set_has_username_edited_vote(true);
        break;
      }
    }
  }
  // A user may make a mistake and remove the correct username. So, save
  // |username_value| and |username_element| of the submitted form. When the
  // user has to override the username, Chrome will send a username vote.
  if (!submitted_form_->username_value.empty()) {
    credential.other_possible_usernames.push_back(ValueElementPair(
        submitted_form_->username_value, submitted_form_->username_element));
  }

  ProvisionallySave(credential);
}

void PasswordFormManager::UpdatePasswordValue(
    const base::string16& new_password) {
  DCHECK(!new_password.empty());

  PasswordForm credential(*submitted_form_);
  // Select whether to update |password_value| or |new_password_value|.
  base::string16* password_value_ptr;
  base::string16* password_element_ptr;
  if (credential.new_password_value.empty()) {
    DCHECK(!credential.password_value.empty());
    password_value_ptr = &credential.password_value;
    password_element_ptr = &credential.password_element;
  } else {
    password_value_ptr = &credential.new_password_value;
    password_element_ptr = &credential.new_password_element;
  }

  *password_value_ptr = new_password;
  // If |new_password| is not found among the known password fields, store an
  // empty field name.
  password_element_ptr->clear();
  for (const ValueElementPair& pair : credential.all_possible_passwords) {
    DCHECK(!pair.second.empty());
    if (pair.first == new_password) {
      *password_element_ptr = pair.second;
      break;
    }
  }

  ProvisionallySave(credential);
}

void PasswordFormManager::PresaveGeneratedPassword(
    const autofill::PasswordForm& form) {
  if ((best_matches_.find(form.username_value) == best_matches_.end()) ||
      form.username_value.empty()) {
    form_saver()->PresaveGeneratedPassword(form);
  } else {
    autofill::PasswordForm form_without_username(form);
    form_without_username.username_value.clear();
    form_saver()->PresaveGeneratedPassword(form_without_username);
  }
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

void PasswordFormManager::PasswordNoLongerGenerated() {
  DCHECK(has_generated_password_);
  form_saver()->RemovePresavedPassword();
  has_generated_password_ = false;
  votes_uploader_.set_has_generated_password(false);
  votes_uploader_.set_generated_password_changed(false);
  metrics_recorder_->SetGeneratedPasswordStatus(
      PasswordFormMetricsRecorder::GeneratedPasswordStatus::kPasswordDeleted);
}

void PasswordFormManager::SaveSubmittedFormTypeForMetrics(
    const autofill::PasswordForm& form) {
  bool is_change_password_form =
      !form.new_password_value.empty() && !form.password_value.empty();
  bool is_signup_form =
      !form.new_password_value.empty() && form.password_value.empty();
  bool no_username = form.username_value.empty();

  PasswordFormMetricsRecorder::SubmittedFormType type =
      PasswordFormMetricsRecorder::kSubmittedFormTypeUnspecified;
  if (is_change_password_form) {
    type = PasswordFormMetricsRecorder::kSubmittedFormTypeChangePasswordEnabled;
  } else if (is_signup_form) {
    if (no_username)
      type = PasswordFormMetricsRecorder::kSubmittedFormTypeSignupNoUsername;
    else
      type = PasswordFormMetricsRecorder::kSubmittedFormTypeSignup;
  } else if (no_username) {
    type = PasswordFormMetricsRecorder::kSubmittedFormTypeLoginNoUsername;
  } else {
    type = PasswordFormMetricsRecorder::kSubmittedFormTypeLogin;
  }
  metrics_recorder_->SetSubmittedFormType(type);
}

void PasswordFormManager::ProcessMatches(
    const std::vector<const PasswordForm*>& non_federated,
    size_t filtered_count) {
  blacklisted_matches_.clear();
  new_blacklisted_.reset();

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PROCESS_MATCHES_METHOD);
  }

  // Copy out and score non-blacklisted matches.
  std::vector<const PasswordForm*> matches;
  std::copy_if(non_federated.begin(), non_federated.end(),
               std::back_inserter(matches),
               [this](const PasswordForm* form) { return IsMatch(*form); });

  password_manager_util::FindBestMatches(std::move(matches), &best_matches_,
                                         &not_best_matches_, &preferred_match_);

  // Copy out blacklisted matches.
  blacklisted_matches_.clear();
  std::copy_if(
      non_federated.begin(), non_federated.end(),
      std::back_inserter(blacklisted_matches_), [](const PasswordForm* form) {
        return form->blacklisted_by_user && !form->is_public_suffix_match;
      });

  UMA_HISTOGRAM_COUNTS_1M(
      "PasswordManager.NumPasswordsNotShown",
      non_federated.size() + filtered_count - best_matches_.size());

  // If password store was slow and provisionally saved form is already here
  // then create pending credentials (see http://crbug.com/470322).
  if (submitted_form_)
    CreatePendingCredentials();

  for (auto const& driver : drivers_)
    ProcessFrameInternal(driver);
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML)
    ProcessLoginPrompt();
}

void PasswordFormManager::ProcessFrame(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, observed_form_.scheme);

  // Don't keep processing the same form.
  if (autofills_left_ <= 0)
    return;
  autofills_left_--;

  if (form_fetcher_->GetState() == FormFetcher::State::NOT_WAITING)
    ProcessFrameInternal(driver);

  for (auto const& old_driver : drivers_) {
    // |drivers_| is not a set because WeakPtr has no good candidate for a key
    // (the address may change to null). So let's weed out duplicates in O(N).
    if (old_driver.get() == driver.get())
      return;
  }

  drivers_.push_back(driver);
}

void PasswordFormManager::ProcessFrameInternal(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kNewPasswordFormParsing))
    return;
  if (!driver)
    return;
  SendFillInformationToRenderer(*client_, driver.get(), IsBlacklisted(),
                                observed_form_, best_matches_,
                                form_fetcher_->GetFederatedMatches(),
                                preferred_match_, GetMetricsRecorder());
}

void PasswordFormManager::ProcessLoginPrompt() {
  DCHECK_NE(PasswordForm::SCHEME_HTML, observed_form_.scheme);
  if (!preferred_match_) {
    DCHECK(best_matches_.empty());
    metrics_recorder_->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventNoCredential);
    return;
  }

  metrics_recorder_->SetManagerAction(
      PasswordFormMetricsRecorder::kManagerActionAutofilled);
  metrics_recorder_->RecordFillEvent(
      PasswordFormMetricsRecorder::kManagerFillEventAutofilled);
  password_manager_->AutofillHttpAuth(best_matches_, *preferred_match_);
}

void PasswordFormManager::ProcessUpdate() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(preferred_match_ || !pending_credentials_.federation_origin.opaque());
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(!IsNewLogin() && pending_credentials_.preferred);
  DCHECK(!client_->IsIncognito());

  password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

  base::RecordAction(
      base::UserMetricsAction("PasswordManager_LoginFollowingAutofill"));

  // Check to see if this form is a candidate for password generation.
  // Do not send votes on change password forms, since they were already sent in
  // Update() method.
  if (!observed_form_.IsPossibleChangePasswordForm()) {
    votes_uploader_.SendVoteOnCredentialsReuse(
        observed_form_.form_data, *submitted_form_, &pending_credentials_);
  }

  if (pending_credentials_.times_used == 1) {
    votes_uploader_.UploadFirstLoginVotes(best_matches_, pending_credentials_,
                                          *submitted_form_);
  }
}

void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(submitted_form_);
  ValueElementPair password_to_save(PasswordToSave(*submitted_form_));

  // Look for the actually submitted credentials in the list of previously saved
  // credentials that were available to autofilling.
  const PasswordForm* saved_form = FindBestSavedMatch(submitted_form_.get());
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
      pending_credentials_.origin = submitted_form_->origin;
      pending_credentials_.signon_realm = submitted_form_->signon_realm;

      // Normally, the copy of the PSL matched credentials, adapted for the
      // current domain, is saved automatically without asking the user, because
      // the copy likely represents the same account, i.e., the one for which
      // the user already agreed to store a password.
      //
      // However, if the user changes the suggested password, it might indicate
      // that the autofilled credentials and |submitted_form_|
      // actually correspond to two different accounts (see
      // http://crbug.com/385619). In that case the user should be asked again
      // before saving the password. This is ensured by setting
      // |password_overriden_| on |pending_credentials_| to false.
      //
      // There is still the edge case when the autofilled credentials represent
      // the same account as |submitted_form_| but the stored password
      // was out of date. In that case, the user just had to manually enter the
      // new password, which is now in |submitted_form_|. The best
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
      if (password_overridden_) {
        // Stored credential matched by username but with mismatching password.
        // This means the user has overridden the password.
        SetUserAction(UserAction::kOverridePassword);
      }
    }
  } else if (!best_matches_.empty() &&
             submitted_form_->type != autofill::PasswordForm::TYPE_API &&
             submitted_form_->username_value.empty()) {
    // This branch deals with the case that the submitted form has no username
    // element and needs to decide whether to offer to update any credentials.
    // In that case, the user can select any previously stored credential as
    // the one to update, but we still try to find the best candidate.

    // Find the best candidate to select by default in the password update
    // bubble. If no best candidate is found, any one can be offered.
    const PasswordForm* best_update_match =
        FindBestMatchForUpdatePassword(submitted_form_->password_value);

    // A retry password form is one that consists of only an "old password"
    // field, i.e. one that is not a "new password".
    retry_password_form_password_update_ =
        submitted_form_->username_value.empty() &&
        submitted_form_->new_password_value.empty();

    is_new_login_ = false;
    if (best_update_match) {
      // Chose |best_update_match| to be updated.
      pending_credentials_ = *best_update_match;
    } else if (HasGeneratedPassword()) {
      // If a password was generated and we didn't find a match, we have to save
      // it in a separate entry since we have to store it but we don't know
      // where.
      CreatePendingCredentialsForNewCredentials(password_to_save.second);
      is_new_login_ = true;
    } else {
      // We don't have a good candidate to choose as the default credential for
      // the update bubble and the user has to pick one.
      // We set |pending_credentials_| to the bare minimum, which is the correct
      // origin.
      pending_credentials_.origin = submitted_form_->origin;
    }
  } else {
    is_new_login_ = true;
    // No stored credentials can be matched to the submitted form. Offer to
    // save new credentials.
    CreatePendingCredentialsForNewCredentials(password_to_save.second);
    // Generate username correction votes.
    bool username_correction_found =
        votes_uploader_.FindCorrectedUsernameElement(
            best_matches_, not_best_matches_, submitted_form_->username_value,
            submitted_form_->password_value);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.UsernameCorrectionFound",
                          username_correction_found);
    if (username_correction_found) {
      metrics_recorder_->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kCorrectedUsernameInForm);
    }
  }

  if (!IsValidAndroidFacetURI(pending_credentials_.signon_realm))
    pending_credentials_.action = submitted_form_->action;

  pending_credentials_.password_value = password_to_save.first;
  pending_credentials_.preferred = submitted_form_->preferred;
  pending_credentials_.form_has_autofilled_value =
      submitted_form_->form_has_autofilled_value;
  pending_credentials_.all_possible_passwords =
      submitted_form_->all_possible_passwords;
  CopyFieldPropertiesMasks(*submitted_form_, &pending_credentials_);

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (submitted_form_->type == autofill::PasswordForm::TYPE_API) {
    pending_credentials_.skip_zero_click = submitted_form_->skip_zero_click;
    pending_credentials_.display_name = submitted_form_->display_name;
    pending_credentials_.federation_origin = submitted_form_->federation_origin;
    pending_credentials_.icon_url = submitted_form_->icon_url;
    // It's important to override |signon_realm| for federated credentials
    // because it has format "federation://" + origin_host + "/" +
    // federation_host
    pending_credentials_.signon_realm = submitted_form_->signon_realm;
  }

  if (HasGeneratedPassword())
    pending_credentials_.type = PasswordForm::TYPE_GENERATED;
}

bool PasswordFormManager::IsMatch(const autofill::PasswordForm& form) const {
  return !form.blacklisted_by_user && form.scheme == observed_form_.scheme;
}

const PasswordForm* PasswordFormManager::FindBestMatchForUpdatePassword(
    const base::string16& password) const {
  // This function is called for forms that do not contain a username field.
  // This means that we cannot update credentials based on a matching username
  // and that we may need to show an update prompt.
  if (best_matches_.size() == 1 && !HasGeneratedPassword()) {
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

const PasswordForm* PasswordFormManager::FindBestSavedMatch(
    const PasswordForm* submitted_form) const {
  if (!submitted_form->federation_origin.opaque())
    return nullptr;

  // Return form with matching |username_value|.
  auto it = best_matches_.find(submitted_form->username_value);
  if (it != best_matches_.end())
    return it->second;

  // Match Credential API forms only by username. Stop here if nothing was found
  // above.
  if (submitted_form->type == autofill::PasswordForm::TYPE_API)
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

void PasswordFormManager::CreatePendingCredentialsForNewCredentials(
    const base::string16& password_element) {
  // User typed in a new, unknown username.
  SetUserAction(UserAction::kOverrideUsernameAndPassword);
  pending_credentials_ = observed_form_;
  pending_credentials_.username_element = submitted_form_->username_element;
  pending_credentials_.username_value = submitted_form_->username_value;
  pending_credentials_.other_possible_usernames =
      submitted_form_->other_possible_usernames;
  pending_credentials_.all_possible_passwords =
      submitted_form_->all_possible_passwords;

  // The password value will be filled in later, remove any garbage for now.
  pending_credentials_.password_value.clear();
  // The password element should be determined earlier in |PasswordToSave|.
  pending_credentials_.password_element = password_element;
  // The new password's value and element name should be empty.
  pending_credentials_.new_password_value.clear();
  pending_credentials_.new_password_element.clear();
}

void PasswordFormManager::OnNopeUpdateClicked() {
  votes_uploader_.UploadPasswordVote(observed_form_, *submitted_form_,
                                     autofill::NOT_NEW_PASSWORD, std::string());
}

void PasswordFormManager::OnNeverClicked() {
  votes_uploader_.UploadPasswordVote(pending_credentials_, *submitted_form_,
                                     autofill::UNKNOWN_TYPE, std::string());
  PermanentlyBlacklist();
}

void PasswordFormManager::OnNoInteraction(bool is_update) {
  if (is_update) {
    votes_uploader_.UploadPasswordVote(observed_form_, *submitted_form_,
                                       autofill::PROBABLY_NEW_PASSWORD,
                                       std::string());
  } else {
    votes_uploader_.UploadPasswordVote(pending_credentials_, *submitted_form_,
                                       autofill::UNKNOWN_TYPE, std::string());
  }
}

void PasswordFormManager::OnPasswordsRevealed() {
  votes_uploader_.set_has_passwords_revealed_vote(true);
}

bool PasswordFormManager::HasGeneratedPassword() const {
  return has_generated_password_;
}

void PasswordFormManager::SetGenerationElement(
    const base::string16& generation_element) {
  votes_uploader_.set_generation_element(generation_element);
}

void PasswordFormManager::SetGenerationPopupWasShown(
    bool generation_popup_was_shown,
    bool is_manual_generation) {
  votes_uploader_.set_generation_popup_was_shown(generation_popup_was_shown);
  votes_uploader_.set_is_manual_generation(is_manual_generation);
  metrics_recorder_->SetPasswordGenerationPopupShown(generation_popup_was_shown,
                                                     is_manual_generation);
}

bool PasswordFormManager::RetryPasswordFormPasswordUpdate() const {
  return retry_password_form_password_update_;
}

void PasswordFormManager::LogSubmitPassed() {
  metrics_recorder_->LogSubmitPassed();
}

void PasswordFormManager::LogSubmitFailed() {
  metrics_recorder_->LogSubmitFailed();
}

void PasswordFormManager::MarkGenerationAvailable() {
  metrics_recorder_->MarkGenerationAvailable();
}

bool PasswordFormManager::IsPossibleChangePasswordFormWithoutUsername() const {
  return is_possible_change_password_form_without_username_;
}

std::vector<base::WeakPtr<PasswordManagerDriver>>
PasswordFormManager::GetDrivers() const {
  return drivers_;
}

const PasswordForm* PasswordFormManager::GetSubmittedForm() const {
  return submitted_form_.get();
}

FormFetcher* PasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

const std::map<base::string16, const autofill::PasswordForm*>&
PasswordFormManager::GetBestMatches() const {
  return best_matches_;
}

const GURL& PasswordFormManager::GetOrigin() const {
  return observed_form_.origin;
}

const autofill::PasswordForm& PasswordFormManager::GetPendingCredentials()
    const {
  return pending_credentials_;
}

metrics_util::CredentialSourceType PasswordFormManager::GetCredentialSource() {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* PasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

const std::vector<const autofill::PasswordForm*>&
PasswordFormManager::GetBlacklistedMatches() const {
  return blacklisted_matches_;
}

bool PasswordFormManager::IsBlacklisted() const {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  return !blacklisted_matches_.empty();
}

bool PasswordFormManager::IsPasswordOverridden() const {
  return password_overridden_;
}

const autofill::PasswordForm* PasswordFormManager::GetPreferredMatch() const {
  return preferred_match_;
}

void PasswordFormManager::ResetStoredMatches() {
  preferred_match_ = nullptr;
  best_matches_.clear();
  not_best_matches_.clear();
  blacklisted_matches_.clear();
  new_blacklisted_.reset();
}

void PasswordFormManager::GrabFetcher(std::unique_ptr<FormFetcher> fetcher) {
  DCHECK(!owned_form_fetcher_);
  owned_form_fetcher_ = std::move(fetcher);
  if (owned_form_fetcher_.get() == form_fetcher_)
    return;
  ResetStoredMatches();
  form_fetcher_->RemoveConsumer(this);
  form_fetcher_ = owned_form_fetcher_.get();
  form_fetcher_->AddConsumer(this);
}

std::unique_ptr<PasswordFormManager> PasswordFormManager::Clone() {
  // Fetcher is cloned to avoid re-fetching data from PasswordStore.
  std::unique_ptr<FormFetcher> fetcher = form_fetcher_->Clone();

  // Some data is filled through the constructor. No PasswordManagerDriver is
  // needed, because the UI does not need any functionality related to the
  // renderer process, to which the driver serves as an interface. The full
  // |observed_form_| needs to be copied, because it is used to created the
  // blacklisting entry if needed.
  auto result = std::make_unique<PasswordFormManager>(
      password_manager_, client_, base::WeakPtr<PasswordManagerDriver>(),
      observed_form_, form_saver_->Clone(), fetcher.get());
  result->Init(metrics_recorder_);

  // The constructor only can take a weak pointer to the fetcher, so moving the
  // owning one needs to happen explicitly.
  result->GrabFetcher(std::move(fetcher));

  // |best_matches_| are skipped, because those are regenerated from the new
  // fetcher automatically.

  // These data members all satisfy:
  //   (1) They could have been changed by |*this| between its construction and
  //       calling Clone().
  //   (2) They are potentially used in the clone as the clone is used in the UI
  //       code.
  //   (3) They are not changed during ProcessMatches, triggered at some point
  //       by the cloned FormFetcher.
  if (submitted_form_)
    result->submitted_form_ = std::make_unique<PasswordForm>(*submitted_form_);

  result->pending_credentials_ = pending_credentials_;
  result->is_new_login_ = is_new_login_;
  result->has_generated_password_ = has_generated_password_;
  result->password_overridden_ = password_overridden_;
  result->retry_password_form_password_update_ =
      retry_password_form_password_update_;
  result->is_possible_change_password_form_without_username_ =
      is_possible_change_password_form_without_username_;
  result->user_action_ = user_action_;
  result->votes_uploader_ = votes_uploader_;

  return result;
}

void PasswordFormManager::SetUserAction(UserAction user_action) {
  user_action_ = user_action;
  metrics_recorder_->SetUserAction(user_action);
}

std::vector<PasswordForm> PasswordFormManager::FindOtherCredentialsToUpdate() {
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

}  // namespace password_manager
