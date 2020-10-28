// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::password_generation::PasswordGenerationType;

namespace password_manager {

namespace metrics_util {

std::string GetPasswordAccountStorageUserStateHistogramSuffix(
    PasswordAccountStorageUserState user_state) {
  switch (user_state) {
    case PasswordAccountStorageUserState::kSignedOutUser:
      return "SignedOutUser";
    case PasswordAccountStorageUserState::kSignedOutAccountStoreUser:
      return "SignedOutAccountStoreUser";
    case PasswordAccountStorageUserState::kSignedInUser:
      return "SignedInUser";
    case PasswordAccountStorageUserState::kSignedInUserSavingLocally:
      return "SignedInUserSavingLocally";
    case PasswordAccountStorageUserState::kSignedInAccountStoreUser:
      return "SignedInAccountStoreUser";
    case PasswordAccountStorageUserState::
        kSignedInAccountStoreUserSavingLocally:
      return "SignedInAccountStoreUserSavingLocally";
    case PasswordAccountStorageUserState::kSyncUser:
      return "SyncUser";
  }
  NOTREACHED();
  return std::string();
}

std::string GetPasswordAccountStorageUsageLevelHistogramSuffix(
    PasswordAccountStorageUsageLevel usage_level) {
  switch (usage_level) {
    case PasswordAccountStorageUsageLevel::kNotUsingAccountStorage:
      return "NotUsingAccountStorage";
    case PasswordAccountStorageUsageLevel::kUsingAccountStorage:
      return "UsingAccountStorage";
    case PasswordAccountStorageUsageLevel::kSyncing:
      return "Syncing";
  }
  NOTREACHED();
  return std::string();
}

void LogGeneralUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.UIDismissalReason", reason,
                                NUM_UI_RESPONSES);
}

void LogSaveUIDismissalReason(
    UIDismissalReason reason,
    base::Optional<PasswordAccountStorageUserState> user_state) {
  base::UmaHistogramEnumeration("PasswordManager.SaveUIDismissalReason", reason,
                                NUM_UI_RESPONSES);

  if (user_state.has_value()) {
    std::string suffix =
        GetPasswordAccountStorageUserStateHistogramSuffix(user_state.value());
    base::UmaHistogramEnumeration(
        "PasswordManager.SaveUIDismissalReason." + suffix, reason,
        NUM_UI_RESPONSES);
  }
}

void LogSaveUIDismissalReasonAfterUnblacklisting(UIDismissalReason reason) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SaveUIDismissalReasonAfterUnblacklisting", reason,
      NUM_UI_RESPONSES);
}

void LogUpdateUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.UpdateUIDismissalReason",
                                reason, NUM_UI_RESPONSES);
}

void LogMoveUIDismissalReason(UIDismissalReason reason,
                              PasswordAccountStorageUserState user_state) {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  base::UmaHistogramEnumeration("PasswordManager.MoveUIDismissalReason", reason,
                                NUM_UI_RESPONSES);

  std::string suffix =
      GetPasswordAccountStorageUserStateHistogramSuffix(user_state);
  base::UmaHistogramEnumeration(
      "PasswordManager.MoveUIDismissalReason." + suffix, reason,
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

void LogPasswordsCountFromAccountStoreAfterUnlock(
    int account_store_passwords_count) {
  base::UmaHistogramCounts100(
      "PasswordManager.CredentialsCountFromAccountStoreAfterUnlock",
      account_store_passwords_count);
}

void LogPasswordSettingsReauthResult(ReauthResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.ReauthToAccessPasswordInSettings", result);
}

void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.DeleteUndecryptableLoginsReturnValue", result);
}

void LogNewlySavedPasswordIsGenerated(
    bool value,
    PasswordAccountStorageUsageLevel account_storage_usage_level) {
  base::UmaHistogramBoolean("PasswordManager.NewlySavedPasswordIsGenerated",
                            value);
  std::string suffix = GetPasswordAccountStorageUsageLevelHistogramSuffix(
      account_storage_usage_level);
  base::UmaHistogramBoolean(
      "PasswordManager.NewlySavedPasswordIsGenerated." + suffix, value);
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

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
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
                                    size_t enterprise_hash_count,
                                    bool does_primary_account_exists,
                                    bool is_signed_in) {
  base::UmaHistogramCounts100("PasswordManager.SavedGaiaPasswordHashCount",
                              static_cast<int>(gaia_hash_count));
  base::UmaHistogramCounts100(
      "PasswordManager.SavedEnterprisePasswordHashCount",
      static_cast<int>(enterprise_hash_count));

  // Log parallel metrics for sync and signed-in non-sync accounts in addition
  // to above to be able to tell what fraction of signed-in non-sync users we
  // are protecting compared to syncing users.
  if (does_primary_account_exists) {
    base::UmaHistogramCounts100(
        "PasswordManager.SavedGaiaPasswordHashCount.Sync",
        static_cast<int>(gaia_hash_count));
  } else if (is_signed_in) {
    base::UmaHistogramCounts100(
        "PasswordManager.SavedGaiaPasswordHashCount.SignedInNonSync",
        static_cast<int>(gaia_hash_count));
  }
}

void LogProtectedPasswordReuse(PasswordType reused_password_type) {}
#endif

void LogPasswordEditResult(IsUsernameChanged username_changed,
                           IsPasswordChanged password_changed) {
  PasswordEditUpdatedValues values;
  if (username_changed && password_changed) {
    values = PasswordEditUpdatedValues::kBoth;
  } else if (username_changed) {
    values = PasswordEditUpdatedValues::kUsername;
  } else if (password_changed) {
    values = PasswordEditUpdatedValues::kPassword;
  } else {
    values = PasswordEditUpdatedValues::kNone;
  }
  base::UmaHistogramEnumeration("PasswordManager.PasswordEditUpdatedValues",
                                values);
}

}  // namespace metrics_util

}  // namespace password_manager
