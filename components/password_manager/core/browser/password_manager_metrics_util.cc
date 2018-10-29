// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

using base::ListValue;
using base::Value;

namespace password_manager {

namespace metrics_util {

void LogUMAHistogramBoolean(const std::string& name, bool sample) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      name, base::Histogram::kUmaTargetedHistogramFlag);
  histogram->AddBoolean(sample);
}

void LogGeneralUIDismissalReason(UIDismissalReason reason) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UIDismissalReason",
                            reason,
                            NUM_UI_RESPONSES);
}

void LogSaveUIDismissalReason(UIDismissalReason reason) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SaveUIDismissalReason", reason,
                            NUM_UI_RESPONSES);
}

void LogUpdateUIDismissalReason(UIDismissalReason reason) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UpdateUIDismissalReason", reason,
                            NUM_UI_RESPONSES);
}

void LogUIDisplayDisposition(UIDisplayDisposition disposition) {
  UMA_HISTOGRAM_ENUMERATION("PasswordBubble.DisplayDisposition",
                            disposition,
                            NUM_DISPLAY_DISPOSITIONS);
}

void LogFormDataDeserializationStatus(FormDeserializationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FormDataDeserializationStatus",
                            status, NUM_DESERIALIZATION_STATUSES);
}

void LogFilledCredentialIsFromAndroidApp(bool from_android) {
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FilledCredentialWasFromAndroidApp",
      from_android);
}

void LogPasswordSyncState(PasswordSyncState state) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PasswordSyncState", state,
                            NUM_SYNC_STATES);
}

void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionEvent", event,
                            SUBMISSION_EVENT_ENUM_COUNT);
}

void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionAvailableEvent",
                            event, SUBMISSION_EVENT_ENUM_COUNT);
}

void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AutoSigninFirstRunDialog", action,
                            AUTO_SIGNIN_PROMO_ACTION_COUNT);
}

void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AccountChooserDialogOneAccount",
                            action, ACCOUNT_CHOOSER_ACTION_COUNT);
}

void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccountChooserDialogMultipleAccounts", action,
      ACCOUNT_CHOOSER_ACTION_COUNT);
}

void LogSyncSigninPromoUserAction(SyncSignInUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SignInPromo", action,
                            CHROME_SIGNIN_ACTION_COUNT);
}

void LogShouldBlockPasswordForSameOriginButDifferentScheme(bool should_block) {
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.ShouldBlockPasswordForSameOriginButDifferentScheme",
      should_block);
}

void LogCountHttpMigratedPasswords(int count) {
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.HttpPasswordMigrationCount", count);
}

void LogHttpPasswordMigrationMode(HttpPasswordMigrationMode mode) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.HttpPasswordMigrationMode", mode,
                            HTTP_PASSWORD_MIGRATION_MODE_COUNT);
}

void LogAccountChooserUsability(AccountChooserUsabilityMetric usability,
                                int count_empty_icons,
                                int count_accounts) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.AccountChooserDialogUsability",
                            usability, ACCOUNT_CHOOSER_USABILITY_COUNT);
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.AccountChooserDialogEmptyAvatars",
                           count_empty_icons);
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.AccountChooserDialogAccounts",
                           count_accounts);
}

void LogCredentialManagerGetResult(CredentialManagerGetResult result,
                                   CredentialMediationRequirement mediation) {
  switch (mediation) {
    case CredentialMediationRequirement::kSilent:
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.MediationSilent", result,
                                CREDENTIAL_MANAGER_GET_COUNT);
      break;
    case CredentialMediationRequirement::kOptional:
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.MediationOptional", result,
                                CREDENTIAL_MANAGER_GET_COUNT);
      break;
    case CredentialMediationRequirement::kRequired:
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.MediationRequired", result,
                                CREDENTIAL_MANAGER_GET_COUNT);
      break;
  }
}

void LogPasswordReuse(int password_length,
                      int saved_passwords,
                      int number_matches,
                      bool password_field_detected,
                      PasswordType reused_password_type) {
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.PasswordReuse.PasswordLength",
                           password_length);
  UMA_HISTOGRAM_COUNTS_1000("PasswordManager.PasswordReuse.TotalPasswords",
                            saved_passwords);
  UMA_HISTOGRAM_COUNTS_1000("PasswordManager.PasswordReuse.NumberOfMatches",
                            number_matches);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.PasswordReuse.PasswordFieldDetected",
      password_field_detected ? HAS_PASSWORD_FIELD : NO_PASSWORD_FIELD,
      PASSWORD_REUSE_PASSWORD_FIELD_DETECTED_COUNT);
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ReusedPasswordType",
                            reused_password_type,
                            PasswordType::PASSWORD_TYPE_COUNT);
}

void LogContextOfShowAllSavedPasswordsShown(
    ShowAllSavedPasswordsContext context) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ShowAllSavedPasswordsShownContext",
                            context, SHOW_ALL_SAVED_PASSWORDS_CONTEXT_COUNT);
}

void LogContextOfShowAllSavedPasswordsAccepted(
    ShowAllSavedPasswordsContext context) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ShowAllSavedPasswordsAcceptedContext", context,
      SHOW_ALL_SAVED_PASSWORDS_CONTEXT_COUNT);
}

void LogPasswordSuccessfulSubmissionIndicatorEvent(
    autofill::PasswordForm::SubmissionIndicatorEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SuccessfulSubmissionIndicatorEvent", event,
      autofill::PasswordForm::SubmissionIndicatorEvent::
          SUBMISSION_INDICATOR_EVENT_COUNT);
}

void LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
    autofill::PasswordForm::SubmissionIndicatorEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AcceptedSaveUpdateSubmissionIndicatorEvent", event,
      autofill::PasswordForm::SubmissionIndicatorEvent::
          SUBMISSION_INDICATOR_EVENT_COUNT);
}

void LogSubmittedFormFrame(SubmittedFormFrame frame) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedFormFrame", frame,
                            SubmittedFormFrame::SUBMITTED_FORM_FRAME_COUNT);
}

void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", result);
}

void LogDeleteCorruptedPasswordsResult(DeleteCorruptedPasswordsResult result) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.DeleteCorruptedPasswordsResult",
                            result);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
void LogSyncPasswordHashChange(SyncPasswordHashChange event) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.SyncPasswordHashChange", event,
      SyncPasswordHashChange::SAVED_SYNC_PASSWORD_CHANGE_COUNT);
}

void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state,
                                bool is_under_advanced_protection) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.IsSyncPasswordHashSaved", state,
      IsSyncPasswordHashSaved::IS_SYNC_PASSWORD_HASH_SAVED_COUNT);
  if (is_under_advanced_protection) {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.IsSyncPasswordHashSavedForAdvancedProtectionUser",
        state, IsSyncPasswordHashSaved::IS_SYNC_PASSWORD_HASH_SAVED_COUNT);
  }
}

void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    size_t enterprise_hash_count) {
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.SavedGaiaPasswordHashCount",
                           static_cast<int>(gaia_hash_count));
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.SavedEnterprisePasswordHashCount",
                           static_cast<int>(enterprise_hash_count));
}

void LogProtectedPasswordReuse(PasswordType reused_password_type) {}
#endif

}  // namespace metrics_util

}  // namespace password_manager
