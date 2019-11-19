// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

using autofill::password_generation::PasswordGenerationType;
using base::ListValue;
using base::Value;

namespace password_manager {

namespace metrics_util {

void LogGeneralUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.UIDismissalReason", reason,
                                NUM_UI_RESPONSES);
}

void LogSaveUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.SaveUIDismissalReason", reason,
                                NUM_UI_RESPONSES);
}

void LogUpdateUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.UpdateUIDismissalReason",
                                reason, NUM_UI_RESPONSES);
}

void LogPresavedUpdateUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration(
      "PasswordManager.PresavedUpdateUIDismissalReason", reason,
      NUM_UI_RESPONSES);
}

void LogLeakDialogTypeAndDismissalReason(LeakDialogType type,
                                         LeakDialogDismissalReason reason) {
  static constexpr char kHistogram[] =
      "PasswordManager.LeakDetection.DialogDismissalReason";
  auto GetSuffix = [type] {
    switch (type) {
      case LeakDialogType::kCheckup:
        return "Checkup";
      case LeakDialogType::kChange:
        return "Change";
      case LeakDialogType::kCheckupAndChange:
        return "CheckupAndChange";
    }
  };

  base::UmaHistogramEnumeration(kHistogram, reason);
  base::UmaHistogramEnumeration(base::StrCat({kHistogram, ".", GetSuffix()}),
                                reason);
}

void LogOnboardingState(OnboardingState state) {
  base::UmaHistogramEnumeration("PasswordManager.Onboarding.State", state);
}

void LogOnboardingUIDismissalReason(OnboardingUIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.Onboarding.UIDismissalReason",
                                reason);
}

void LogResultOfSavingFlow(OnboardingResultOfSavingFlow result) {
  base::UmaHistogramEnumeration("PasswordManager.Onboarding.ResultOfSavingFlow",
                                result);
}

void LogResultOfOnboardingSavingFlow(OnboardingResultOfSavingFlow result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding", result);
}

void LogUIDisplayDisposition(UIDisplayDisposition disposition) {
  base::UmaHistogramEnumeration("PasswordBubble.DisplayDisposition",
                                disposition, NUM_DISPLAY_DISPOSITIONS);
}

void LogFormDataDeserializationStatus(FormDeserializationStatus status) {
  base::UmaHistogramEnumeration("PasswordManager.FormDataDeserializationStatus",
                                status, NUM_DESERIALIZATION_STATUSES);
}

void LogFilledCredentialIsFromAndroidApp(bool from_android) {
  base::UmaHistogramBoolean("PasswordManager.FilledCredentialWasFromAndroidApp",
                            from_android);
}

void LogPasswordSyncState(PasswordSyncState state) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordSyncState", state,
                                NUM_SYNC_STATES);
}

void LogApplySyncChangesState(ApplySyncChangesState state) {
  base::UmaHistogramEnumeration("PasswordManager.ApplySyncChangesState", state);
}

void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event) {
  base::UmaHistogramEnumeration("PasswordGeneration.SubmissionEvent", event,
                                SUBMISSION_EVENT_ENUM_COUNT);
}

void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event) {
  base::UmaHistogramEnumeration("PasswordGeneration.SubmissionAvailableEvent",
                                event, SUBMISSION_EVENT_ENUM_COUNT);
}

void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action) {
  base::UmaHistogramEnumeration("PasswordManager.AutoSigninFirstRunDialog",
                                action, AUTO_SIGNIN_PROMO_ACTION_COUNT);
}

void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AccountChooserDialogOneAccount", action,
      ACCOUNT_CHOOSER_ACTION_COUNT);
}

void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AccountChooserDialogMultipleAccounts", action,
      ACCOUNT_CHOOSER_ACTION_COUNT);
}

void LogCountHttpMigratedPasswords(int count) {
  base::UmaHistogramCounts100("PasswordManager.HttpPasswordMigrationCount",
                              count);
}

void LogHttpPasswordMigrationMode(HttpPasswordMigrationMode mode) {
  base::UmaHistogramEnumeration("PasswordManager.HttpPasswordMigrationMode",
                                mode, HTTP_PASSWORD_MIGRATION_MODE_COUNT);
}

void LogCredentialManagerGetResult(CredentialManagerGetResult result,
                                   CredentialMediationRequirement mediation) {
  switch (mediation) {
    case CredentialMediationRequirement::kSilent:
      base::UmaHistogramEnumeration("PasswordManager.MediationSilent", result);
      break;
    case CredentialMediationRequirement::kOptional:
      base::UmaHistogramEnumeration("PasswordManager.MediationOptional",
                                    result);
      break;
    case CredentialMediationRequirement::kRequired:
      base::UmaHistogramEnumeration("PasswordManager.MediationRequired",
                                    result);
      break;
  }
}

void LogPasswordReuse(int password_length,
                      int saved_passwords,
                      int number_matches,
                      bool password_field_detected,
                      PasswordType reused_password_type) {
  base::UmaHistogramCounts100("PasswordManager.PasswordReuse.PasswordLength",
                              password_length);
  base::UmaHistogramCounts1000("PasswordManager.PasswordReuse.TotalPasswords",
                               saved_passwords);
  base::UmaHistogramCounts1000("PasswordManager.PasswordReuse.NumberOfMatches",
                               number_matches);
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordReuse.PasswordFieldDetected",
      password_field_detected ? HAS_PASSWORD_FIELD : NO_PASSWORD_FIELD,
      PASSWORD_REUSE_PASSWORD_FIELD_DETECTED_COUNT);
  base::UmaHistogramEnumeration("PasswordManager.ReusedPasswordType",
                                reused_password_type,
                                PasswordType::PASSWORD_TYPE_COUNT);
}

void LogContextOfShowAllSavedPasswordsShown(
    ShowAllSavedPasswordsContext context) {
  base::UmaHistogramEnumeration(
      "PasswordManager.ShowAllSavedPasswordsShownContext", context,
      SHOW_ALL_SAVED_PASSWORDS_CONTEXT_COUNT);
}

void LogContextOfShowAllSavedPasswordsAccepted(
    ShowAllSavedPasswordsContext context) {
  base::UmaHistogramEnumeration(
      "PasswordManager.ShowAllSavedPasswordsAcceptedContext", context,
      SHOW_ALL_SAVED_PASSWORDS_CONTEXT_COUNT);
}

void LogPasswordDropdownShown(PasswordDropdownState state,
                              bool off_the_record) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordDropdownShown", state);

  base::UmaHistogramBoolean("PasswordManager.DropdownShown.OffTheRecord",
                            off_the_record);
}

void LogPasswordDropdownItemSelected(PasswordDropdownSelectedOption type,
                                     bool off_the_record) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordDropdownItemSelected",
                                type);
  base::UmaHistogramBoolean("PasswordManager.ItemSelected.OffTheRecord",
                            off_the_record);
}

void LogPasswordSuccessfulSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SuccessfulSubmissionIndicatorEvent", event);
}

void LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AcceptedSaveUpdateSubmissionIndicatorEvent", event);
}

void LogSubmittedFormFrame(SubmittedFormFrame frame) {
  base::UmaHistogramEnumeration("PasswordManager.SubmittedFormFrame", frame,
                                SubmittedFormFrame::SUBMITTED_FORM_FRAME_COUNT);
}

void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", result);
}

void LogDeleteCorruptedPasswordsResult(DeleteCorruptedPasswordsResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.DeleteCorruptedPasswordsResult", result);
}

void LogNewlySavedPasswordIsGenerated(bool value) {
  base::UmaHistogramBoolean("PasswordManager.NewlySavedPasswordIsGenerated",
                            value);
}

void LogGenerationPresaveConflict(GenerationPresaveConflict value) {
  base::UmaHistogramEnumeration("PasswordGeneration.PresaveConflict", value);
}

void LogGenerationDialogChoice(GenerationDialogChoice choice,
                               PasswordGenerationType type) {
  switch (type) {
    case PasswordGenerationType::kAutomatic:
      base::UmaHistogramEnumeration(
          "KeyboardAccessory.GenerationDialogChoice.Automatic", choice);
      break;
    case PasswordGenerationType::kManual:
      base::UmaHistogramEnumeration(
          "KeyboardAccessory.GenerationDialogChoice.Manual", choice);
      break;
  };
}  // namespace metrics_util

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
void LogGaiaPasswordHashChange(GaiaPasswordHashChange event,
                               bool is_sync_password) {
  if (is_sync_password) {
    base::UmaHistogramEnumeration("PasswordManager.SyncPasswordHashChange",
                                  event);
  } else {
    base::UmaHistogramEnumeration("PasswordManager.NonSyncPasswordHashChange",
                                  event);
  }
}

void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state,
                                bool is_under_advanced_protection) {
  base::UmaHistogramEnumeration("PasswordManager.IsSyncPasswordHashSaved",
                                state);
  if (is_under_advanced_protection) {
    base::UmaHistogramEnumeration(
        "PasswordManager.IsSyncPasswordHashSavedForAdvancedProtectionUser",
        state);
  }
}

void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    size_t enterprise_hash_count) {
  base::UmaHistogramCounts100("PasswordManager.SavedGaiaPasswordHashCount",
                              static_cast<int>(gaia_hash_count));
  base::UmaHistogramCounts100(
      "PasswordManager.SavedEnterprisePasswordHashCount",
      static_cast<int>(enterprise_hash_count));
}

void LogProtectedPasswordReuse(PasswordType reused_password_type) {}
#endif

}  // namespace metrics_util

}  // namespace password_manager
