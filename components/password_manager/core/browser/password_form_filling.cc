// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_filling.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::PasswordAndMetadata;
using autofill::PasswordFormFillData;
using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {
bool PreferredRealmIsFromAndroid(const PasswordFormFillData& fill_data) {
  return FacetURI::FromPotentiallyInvalidSpec(fill_data.preferred_realm)
      .IsValidAndroidFacetURI();
}

bool ContainsAndroidCredentials(const PasswordFormFillData& fill_data) {
  for (const auto& login : fill_data.additional_logins) {
    if (FacetURI::FromPotentiallyInvalidSpec(login.realm)
            .IsValidAndroidFacetURI()) {
      return true;
    }
  }

  return PreferredRealmIsFromAndroid(fill_data);
}

bool IsFillOnAccountSelectFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kFillOnAccountSelect);
}

bool IsPublicSuffixMatchOrAffiliationBasedMatch(const PasswordForm& form) {
  return form.is_public_suffix_match || form.is_affiliation_based_match;
}

// Finds any suggestion in |login| whose username and password match the |form|.
PasswordFormFillData::LoginCollection::iterator FindDuplicate(
    PasswordFormFillData::LoginCollection* logins,
    const PasswordForm& form) {
  return std::find_if(logins->begin(), logins->end(),
                      [&form](const PasswordAndMetadata& login) {
                        return (form.username_value == login.username &&
                                form.password_value == login.password);
                      });
}

// This function takes a |duplicate_form| and the realm and uses_account_store
// properties of an existing suggestion. Both suggestions have identical
// username and password.
// If the duplicate should replace the existing suggestion, this method
// overrides the realm and uses_account_store properties to achieve that.
void MaybeReplaceRealmAndStoreWithDuplicate(const PasswordForm& duplicate_form,
                                            std::string* existing_realm,
                                            bool* existing_uses_account_store) {
  DCHECK(existing_realm);
  DCHECK(existing_uses_account_store);
  if (*existing_uses_account_store)
    return;  // No need to replace existing account-stored suggestion.
  if (!duplicate_form.IsUsingAccountStore())
    return;  // No need to replace a local suggestion with identical other one.
  if (IsPublicSuffixMatchOrAffiliationBasedMatch(duplicate_form))
    return;  // Never replace a possibly exact match with a PSL match.
  *existing_uses_account_store = duplicate_form.IsUsingAccountStore();
  existing_realm->clear();  // Reset realm since form cannot be a psl match.
}

void Autofill(PasswordManagerClient* client,
              PasswordManagerDriver* driver,
              const PasswordForm& form_for_autofill,
              const std::vector<const PasswordForm*>& best_matches,
              const std::vector<const PasswordForm*>& federated_matches,
              const PasswordForm& preferred_match,
              bool wait_for_username) {
  DCHECK_EQ(PasswordForm::Scheme::kHtml, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client)) {
    logger = std::make_unique<BrowserSavePasswordProgressLogger>(
        client->GetLogManager());
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILL);
  }

  PasswordFormFillData fill_data = CreatePasswordFormFillData(
      form_for_autofill, best_matches, preferred_match, wait_for_username);
  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FillSuggestionsIncludeAndroidAppCredentials",
      ContainsAndroidCredentials(fill_data));
  metrics_util::LogFilledCredentialIsFromAndroidApp(
      PreferredRealmIsFromAndroid(fill_data));
  driver->FillPasswordForm(fill_data);

  client->PasswordWasAutofilled(best_matches,
                                url::Origin::Create(form_for_autofill.url),
                                &federated_matches);
}

}  // namespace

LikelyFormFilling SendFillInformationToRenderer(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver,
    const PasswordForm& observed_form,
    const std::vector<const PasswordForm*>& best_matches,
    const std::vector<const PasswordForm*>& federated_matches,
    const PasswordForm* preferred_match,
    PasswordFormMetricsRecorder* metrics_recorder) {
  DCHECK(driver);
  DCHECK_EQ(PasswordForm::Scheme::kHtml, observed_form.scheme);

  if (autofill::IsShowAutofillSignaturesEnabled()) {
    driver->AnnotateFieldsWithParsingResult(
        {.username_renderer_id = observed_form.username_element_renderer_id,
         .password_renderer_id = observed_form.password_element_renderer_id,
         .new_password_renderer_id =
             observed_form.new_password_element_renderer_id,
         .confirm_password_renderer_id =
             observed_form.confirmation_password_element_renderer_id});
  }

  if (best_matches.empty()) {
    bool should_show_popup_without_passwords =
        client->GetPasswordFeatureManager()->ShouldShowAccountStorageOptIn() ||
        client->GetPasswordFeatureManager()->ShouldShowAccountStorageReSignin(
            client->GetLastCommittedURL());
    driver->InformNoSavedCredentials(should_show_popup_without_passwords);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventNoCredential);
    return LikelyFormFilling::kNoFilling;
  }
  DCHECK(preferred_match);

  // If the parser of the PasswordFormManager decides that there is no
  // current password field, no filling attempt will be made. In this case the
  // renderer won't treat this as the "first filling" and won't record metrics
  // accordingly. The browser should not do that either.
  const bool no_sign_in_form =
      !observed_form.HasPasswordElement() && !observed_form.IsSingleUsername();

  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, or
  // (2) if it matched using public suffix domain matching, or
  // (3) it would result in unexpected filling in a form with new password
  //     fields.
  using WaitForUsernameReason =
      PasswordFormMetricsRecorder::WaitForUsernameReason;
  WaitForUsernameReason wait_for_username_reason =
      WaitForUsernameReason::kDontWait;
  if (client->RequiresReauthToFill()) {
    wait_for_username_reason = WaitForUsernameReason::kReauthRequired;
  } else if (client->IsIncognito()) {
    wait_for_username_reason = WaitForUsernameReason::kIncognitoMode;
  } else if (preferred_match->is_public_suffix_match) {
    wait_for_username_reason = WaitForUsernameReason::kPublicSuffixMatch;
  } else if (no_sign_in_form) {
    // If the parser did not find a current password element, don't fill.
    wait_for_username_reason = WaitForUsernameReason::kFormNotGoodForFilling;
  } else if (observed_form.HasUsernameElement() &&
             observed_form.HasNonEmptyPasswordValue() &&
             observed_form.server_side_classification_successful &&
             !observed_form.username_may_use_prefilled_placeholder) {
    // Password is already filled in and we don't think the username is a
    // placeholder, so don't overwrite.
    wait_for_username_reason = WaitForUsernameReason::kPasswordPrefilled;
  } else if (!client->IsCommittedMainFrameSecure()) {
    wait_for_username_reason = WaitForUsernameReason::kInsecureOrigin;
  } else if (autofill::IsTouchToFillEnabled()) {
    wait_for_username_reason = WaitForUsernameReason::kTouchToFill;
  } else if (IsFillOnAccountSelectFeatureEnabled()) {
    wait_for_username_reason = WaitForUsernameReason::kFoasFeature;
  }

  // Record no "FirstWaitForUsernameReason" metrics for a form that is not meant
  // for filling. The renderer won't record a "FirstFillingResult" either.
  if (!no_sign_in_form) {
    metrics_recorder->RecordFirstWaitForUsernameReason(
        wait_for_username_reason);
  }

  bool wait_for_username =
      wait_for_username_reason != WaitForUsernameReason::kDontWait;

  if (wait_for_username) {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionNone);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventBlockedOnInteraction);
  } else {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionAutofilled);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventAutofilled);
    base::RecordAction(base::UserMetricsAction("PasswordManager_Autofilled"));
  }

  // Continue with autofilling any password forms as traditionally has been
  // done.
  Autofill(client, driver, observed_form, best_matches, federated_matches,
           *preferred_match, wait_for_username);
  return wait_for_username ? LikelyFormFilling::kFillOnAccountSelect
                           : LikelyFormFilling::kFillOnPageLoad;
}

PasswordFormFillData CreatePasswordFormFillData(
    const PasswordForm& form_on_page,
    const std::vector<const PasswordForm*>& matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) {
  PasswordFormFillData result;

  result.form_renderer_id = form_on_page.form_data.unique_renderer_id;
  result.name = form_on_page.form_data.name;
  result.url = form_on_page.url;
  result.action = form_on_page.action;
  result.uses_account_store = preferred_match.IsUsingAccountStore();
  result.wait_for_username = wait_for_username;

  // Note that many of the |FormFieldData| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  result.username_field.value = preferred_match.username_value;
  result.password_field.value = preferred_match.password_value;
  if (!form_on_page.only_for_fallback &&
      (form_on_page.HasPasswordElement() || form_on_page.IsSingleUsername())) {
    // Fill fields identifying information only for non-fallback case when
    // password element is found. In other cases a fill popup is shown on
    // clicking on each password field so no need in any field identifiers.
    result.username_field.name = form_on_page.username_element;
    result.username_field.unique_renderer_id =
        form_on_page.username_element_renderer_id;
    result.username_may_use_prefilled_placeholder =
        form_on_page.username_may_use_prefilled_placeholder;

    result.password_field.name = form_on_page.password_element;
    result.password_field.unique_renderer_id =
        form_on_page.password_element_renderer_id;
    result.password_field.form_control_type = "password";

    // On iOS, use the unique_id field to refer to elements.
#if defined(OS_IOS)
    result.username_field.unique_id = form_on_page.username_element;
    result.password_field.unique_id = form_on_page.password_element;
#endif
  }

  if (IsPublicSuffixMatchOrAffiliationBasedMatch(preferred_match))
    result.preferred_realm = preferred_match.signon_realm;

  // Copy additional username/value pairs.
  for (const PasswordForm* match : matches) {
    // If any already retained suggestion matches the login, discard the login
    // or override the existing duplicate with the account-stored match.
    if (match->username_value == preferred_match.username_value &&
        match->password_value == preferred_match.password_value) {
      MaybeReplaceRealmAndStoreWithDuplicate(*match, &result.preferred_realm,
                                             &result.uses_account_store);
      continue;
    }
    auto duplicate_iter = FindDuplicate(&result.additional_logins, *match);
    if (duplicate_iter != result.additional_logins.end()) {
      MaybeReplaceRealmAndStoreWithDuplicate(
          *match, &duplicate_iter->realm, &duplicate_iter->uses_account_store);
      continue;
    }
    PasswordAndMetadata value;
    value.username = match->username_value;
    value.password = match->password_value;
    value.uses_account_store = match->IsUsingAccountStore();
    if (IsPublicSuffixMatchOrAffiliationBasedMatch(*match))
      value.realm = match->signon_realm;
    result.additional_logins.push_back(std::move(value));
  }

  return result;
}

}  // namespace password_manager
