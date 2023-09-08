// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using autofill::password_generation::PasswordGenerationType;

namespace ukm::builders {
class PasswordManager_LeakWarningDialog;
}  // namespace ukm::builders

namespace password_manager::metrics_util {

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

LeakDialogMetricsRecorder::LeakDialogMetricsRecorder(ukm::SourceId source_id,
                                                     LeakDialogType type)
    : source_id_(source_id), type_(type) {}

void LeakDialogMetricsRecorder::LogLeakDialogTypeAndDismissalReason(
    LeakDialogDismissalReason reason) {
  // Always record UMA.
  base::UmaHistogramEnumeration(kHistogram, reason);
  base::UmaHistogramEnumeration(base::StrCat({kHistogram, ".", GetUMASuffix()}),
                                reason);

  // For UKM, sample the recorded events.
  if (base::RandDouble() > ukm_sampling_rate_)
    return;

  // The entire event is made up of these two fields, so we can build and
  // record it in one step.
  ukm ::builders::PasswordManager_LeakWarningDialog ukm_builder(source_id_);
  ukm_builder.SetPasswordLeakDetectionDialogType(static_cast<int64_t>(type_));
  ukm_builder.SetPasswordLeakDetectionDialogDismissalReason(
      static_cast<int64_t>(reason));
  ukm_builder.Record(ukm::UkmRecorder::Get());
}

const char* LeakDialogMetricsRecorder::GetUMASuffix() const {
  switch (type_) {
    case LeakDialogType::kCheckup:
      return "Checkup";
    case LeakDialogType::kChange:
      return "Change";
    case LeakDialogType::kCheckupAndChange:
      return "CheckupAndChange";
  }
}

void LogGeneralUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.UIDismissalReason", reason,
                                NUM_UI_RESPONSES);
}

void LogSaveUIDismissalReason(
    UIDismissalReason reason,
    autofill::mojom::SubmissionIndicatorEvent submission_event,
    absl::optional<PasswordAccountStorageUserState> user_state) {
  base::UmaHistogramEnumeration("PasswordManager.SaveUIDismissalReason", reason,
                                NUM_UI_RESPONSES);

  if (user_state.has_value()) {
    std::string suffix =
        GetPasswordAccountStorageUserStateHistogramSuffix(user_state.value());
    base::UmaHistogramEnumeration(
        "PasswordManager.SaveUIDismissalReason." + suffix, reason,
        NUM_UI_RESPONSES);
  }

  if (submission_event ==
      autofill::mojom::SubmissionIndicatorEvent::CHANGE_PASSWORD_FORM_CLEARED) {
    base::UmaHistogramEnumeration(
        "PasswordManager.SaveUIOnClearedPasswordChangeFormDismissalReason",
        reason, NUM_UI_RESPONSES);
  }
}

void LogUpdateUIDismissalReason(
    UIDismissalReason reason,
    autofill::mojom::SubmissionIndicatorEvent submission_event) {
  base::UmaHistogramEnumeration("PasswordManager.UpdateUIDismissalReason",
                                reason, NUM_UI_RESPONSES);

  if (submission_event ==
      autofill::mojom::SubmissionIndicatorEvent::CHANGE_PASSWORD_FORM_CLEARED) {
    base::UmaHistogramEnumeration(
        "PasswordManager.UpdateUIOnClearedPasswordChangeFormDismissalReason",
        reason, NUM_UI_RESPONSES);
  }
}

void LogMoveUIDismissalReason(UIDismissalReason reason,
                              PasswordAccountStorageUserState user_state) {
  base::UmaHistogramEnumeration("PasswordManager.MoveUIDismissalReason", reason,
                                NUM_UI_RESPONSES);

  std::string suffix =
      GetPasswordAccountStorageUserStateHistogramSuffix(user_state);
  base::UmaHistogramEnumeration(
      "PasswordManager.MoveUIDismissalReason." + suffix, reason,
      NUM_UI_RESPONSES);
}

void LogUIDisplayDisposition(UIDisplayDisposition disposition) {
  base::UmaHistogramEnumeration("PasswordBubble.DisplayDisposition",
                                disposition, NUM_DISPLAY_DISPOSITIONS);
}

void LogFilledPasswordFromAndroidApp(bool from_android) {
  base::UmaHistogramBoolean(
      "PasswordManager.FilledCredentialWasFromAndroidApp2", from_android);
}

void LogPasswordSyncState(PasswordSyncState state) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordSyncState3", state);
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

void LogPasswordReuse(int saved_passwords,
                      int number_matches,
                      bool password_field_detected,
                      PasswordType reused_password_type) {
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

void LogPasswordsCountFromAccountStoreAfterUnlock(
    int account_store_passwords_count) {
  base::UmaHistogramCounts100(
      "PasswordManager.CredentialsCountFromAccountStoreAfterUnlock",
      account_store_passwords_count);
}

void LogDownloadedPasswordsCountFromAccountStoreAfterUnlock(
    int account_store_passwords_count) {
  base::UmaHistogramCounts100(
      "PasswordManager.AccountStoreCredentialsAfterOptIn",
      account_store_passwords_count);
}

void LogDownloadedBlocklistedEntriesCountFromAccountStoreAfterUnlock(
    int blocklist_entries_count) {
  base::UmaHistogramCounts100(
      "PasswordManager.AccountStoreBlocklistedEntriesAfterOptIn",
      blocklist_entries_count);
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

void LogNewlySavedPasswordMetrics(
    bool is_generated_password,
    bool is_username_empty,
    PasswordAccountStorageUsageLevel account_storage_usage_level) {
  base::UmaHistogramBoolean("PasswordManager.NewlySavedPasswordIsGenerated",
                            is_generated_password);
  std::string suffix = GetPasswordAccountStorageUsageLevelHistogramSuffix(
      account_storage_usage_level);
  base::UmaHistogramBoolean(
      "PasswordManager.NewlySavedPasswordIsGenerated." + suffix,
      is_generated_password);

  base::UmaHistogramBoolean(
      "PasswordManager.NewlySavedPasswordHasEmptyUsername.Overall",
      is_username_empty);
  base::UmaHistogramBoolean(
      base::StrCat({"PasswordManager.NewlySavedPasswordHasEmptyUsername.",
                    is_generated_password ? "AutoGenerated" : "UserCreated"}),
      is_username_empty);
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

void LogIsPasswordProtected(bool is_password_protected) {
  // To preserve privacy of individual data points, add a 10% statistical noise
  bool log_value = is_password_protected;
  if (base::RandInt(0, 9) == 0) {
    log_value = !is_password_protected;
  }
  base::UmaHistogramBoolean("PasswordManager.IsPasswordProtected2", log_value);
}

void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    SignInState sign_in_state) {
  base::UmaHistogramCounts100("PasswordManager.SavedGaiaPasswordHashCount2",
                              static_cast<int>(gaia_hash_count));

  // Log parallel metrics for sync and signed-in non-sync accounts in addition
  // to above to be able to tell what fraction of signed-in non-sync users we
  // are protecting compared to syncing users.
  switch (sign_in_state) {
    case SignInState::kSignedOut:
      break;
    case SignInState::kSignedInSyncDisabled:
      base::UmaHistogramCounts100(
          "PasswordManager.SavedGaiaPasswordHashCount2.SignedInNonSync",
          static_cast<int>(gaia_hash_count));
      break;
    case SignInState::kSyncing:
      base::UmaHistogramCounts100(
          "PasswordManager.SavedGaiaPasswordHashCount2.Sync",
          static_cast<int>(gaia_hash_count));
      break;
  }
}

void LogProtectedPasswordReuse(PasswordType reused_password_type) {}

void LogUserInteractionsWhenAddingCredentialFromSettings(
    AddCredentialFromSettingsUserInteractions
        add_credential_from_settings_user_interaction) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AddCredentialFromSettings.UserAction2",
      add_credential_from_settings_user_interaction);
}

void LogPasswordNoteActionInSettings(PasswordNoteAction action) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordNoteActionInSettings2",
                                action);
}

void LogUserInteractionsInPasswordManagementBubble(
    PasswordManagementBubbleInteractions
        password_management_bubble_interaction) {
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordManagementBubble.UserAction",
      password_management_bubble_interaction);
}

void LogUserInteractionsInSharedPasswordsNotificationBubble(
    SharedPasswordsNotificationBubbleInteractions interaction) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SharedPasswordsNotificationBubble.UserAction",
      interaction);
}

void LogGroupedPasswordsResults(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        logins) {
  auto is_grouped_match =
      [](const std::unique_ptr<password_manager::PasswordForm>& form) {
        return form->match_type ==
               password_manager::PasswordForm::MatchType::kGrouped;
      };
  GroupedPasswordFetchResult result = GroupedPasswordFetchResult::kNoMatches;
  if (!logins.empty() && base::ranges::all_of(logins, is_grouped_match)) {
    result = GroupedPasswordFetchResult::kOnlyGroupedMatches;
  } else if (base::ranges::any_of(logins, is_grouped_match)) {
    result = GroupedPasswordFetchResult::kBetterMatchesExist;
  }
  base::UmaHistogramEnumeration(
      "PasswordManager.GetLogins.GroupedMatchesStatus", result);
}

#if BUILDFLAG(IS_IOS)
void RecordMigrationToOSCryptLatency(bool success,
                                     base::TimeDelta latency,
                                     base::StringPiece store_infix) {
  if (success) {
    base::UmaHistogramLongTimes(
        base::StrCat({"PasswordManager.MigrationToOSCrypt.", store_infix,
                      ".SuccessLatency"}),
        latency);
    return;
  }
  base::UmaHistogramLongTimes(
      base::StrCat({"PasswordManager.MigrationToOSCrypt.", store_infix,
                    ".ErrorLatency"}),
      latency);
}

void RecordMigrationToOSCryptStatus(base::TimeTicks migration_start_time,
                                    bool is_account_store,
                                    MigrationToOSCrypt status) {
  base::StringPiece infix_for_store =
      is_account_store ? "AccountStore" : "ProfileStore";
  if (status != MigrationToOSCrypt::kStarted) {
    RecordMigrationToOSCryptLatency(
        status == MigrationToOSCrypt::kSuccess,
        base::TimeTicks::Now() - migration_start_time, infix_for_store);
  }

  base::UmaHistogramEnumeration("PasswordManager.MigrationToOSCrypt", status);
  base::UmaHistogramEnumeration(
      base::StrCat({"PasswordManager.MigrationToOSCrypt.", infix_for_store}),
      status);
}

void RecordPasswordNotesMigrationToOSCryptStatus(
    bool is_account_store,
    PasswordNotesMigrationToOSCrypt status) {
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordNotesMigrationToOSCrypt", status);
  base::UmaHistogramEnumeration(
      base::StrCat({"PasswordManager.PasswordNotesMigrationToOSCrypt.",
                    is_account_store ? "AccountStore" : "ProfileStore"}),
      status);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace password_manager::metrics_util
