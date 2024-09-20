// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using autofill::password_generation::PasswordGenerationType;

namespace ukm::builders {
class PasswordManager_LeakWarningDialog;
}  // namespace ukm::builders

namespace password_manager::metrics_util {

namespace {

struct PasswordAndPasskeyCounts {
  size_t password_count = 0;
  size_t passkey_count = 0;
  bool has_another_device = false;
};

PasswordAndPasskeyCounts GetPasswordPasskeyCountsAndUseAnotherDeviceShown(
    const std::vector<autofill::Suggestion>& suggestions) {
  using autofill::SuggestionType;
  PasswordAndPasskeyCounts counts;
  for (const auto& suggestion : suggestions) {
    switch (suggestion.type) {
      case SuggestionType::kPasswordEntry:
      case SuggestionType::kAccountStoragePasswordEntry:
        counts.password_count++;
        break;
      case SuggestionType::kWebauthnCredential:
        counts.passkey_count++;
        break;
      case SuggestionType::kWebauthnSignInWithAnotherDevice:
        counts.has_another_device = true;
        break;
      default:
        break;
    }
  }
  return counts;
}

}  // namespace

std::string GetPasswordAccountStorageUserStateHistogramSuffix(
    password_manager::features_util::PasswordAccountStorageUserState
        user_state) {
  switch (user_state) {
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSignedOutUser:
      return "SignedOutUser";
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSignedOutAccountStoreUser:
      return "SignedOutAccountStoreUser";
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSignedInUser:
      return "SignedInUser";
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSignedInUserSavingLocally:
      return "SignedInUserSavingLocally";
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSignedInAccountStoreUser:
      return "SignedInAccountStoreUser";
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSignedInAccountStoreUserSavingLocally:
      return "SignedInAccountStoreUserSavingLocally";
    case password_manager::features_util::PasswordAccountStorageUserState::
        kSyncUser:
      return "SyncUser";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string GetPasswordAccountStorageUsageLevelHistogramSuffix(
    password_manager::features_util::PasswordAccountStorageUsageLevel
        usage_level) {
  switch (usage_level) {
    case password_manager::features_util::PasswordAccountStorageUsageLevel::
        kNotUsingAccountStorage:
      return "NotUsingAccountStorage";
    case password_manager::features_util::PasswordAccountStorageUsageLevel::
        kUsingAccountStorage:
      return "UsingAccountStorage";
    case password_manager::features_util::PasswordAccountStorageUsageLevel::
        kSyncing:
      return "Syncing";
  }
  NOTREACHED_IN_MIGRATION();
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
  if (base::RandDouble() > ukm_sampling_rate_) {
    return;
  }

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
    std::optional<
        password_manager::features_util::PasswordAccountStorageUserState>
        user_state,
    bool log_adoption_metric) {
  base::UmaHistogramEnumeration("PasswordManager.SaveUIDismissalReason", reason,
                                NUM_UI_RESPONSES);

  if (user_state.has_value()) {
    std::string suffix =
        GetPasswordAccountStorageUserStateHistogramSuffix(user_state.value());
    base::UmaHistogramEnumeration(
        "PasswordManager.SaveUIDismissalReason." + suffix, reason,
        NUM_UI_RESPONSES);
  }

  if (log_adoption_metric) {
    base::UmaHistogramEnumeration(
        "PasswordManager.SaveUIDismissalReason.UsersWithNoCredentials", reason,
        NUM_UI_RESPONSES);
  }
}

void LogUpdateUIDismissalReason(UIDismissalReason reason) {
  base::UmaHistogramEnumeration("PasswordManager.UpdateUIDismissalReason",
                                reason, NUM_UI_RESPONSES);
}

void LogMoveUIDismissalReason(
    UIDismissalReason reason,
    password_manager::features_util::PasswordAccountStorageUserState
        user_state) {
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

void LogPasswordDropdownShown(
    const std::vector<autofill::Suggestion>& suggestions) {
  std::optional<PasswordDropdownState> dropdown_state;
  if (suggestions.size() > 0) {
    dropdown_state = PasswordDropdownState::kStandard;
  }
  for (const auto& suggestion : suggestions) {
    switch (suggestion.type) {
      case autofill::SuggestionType::kGeneratePasswordEntry:
        // TODO(crbug.com/40122999): Revisit metrics for the "opt in and
        // generate" button.
      case autofill::SuggestionType::kPasswordAccountStorageOptInAndGenerate:
        dropdown_state = PasswordDropdownState::kStandardGenerate;
        break;
      default:
        break;
    }
  }
  if (dropdown_state.has_value()) {
    base::UmaHistogramEnumeration("PasswordManager.PasswordDropdownShown",
                                  dropdown_state.value());
  }
}

void LogPasswordDropdownItemSelected(PasswordDropdownSelectedOption type,
                                     bool off_the_record) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordDropdownItemSelected",
                                type);
  base::UmaHistogramBoolean("PasswordManager.ItemSelected.OffTheRecord",
                            off_the_record);

  switch (type) {
    case PasswordDropdownSelectedOption::kPassword:
      base::RecordAction(base::UserMetricsAction(
          "PasswordManager.PasswordDropdownSelected.Password"));
      break;
    case PasswordDropdownSelectedOption::kWebAuthn:
      base::RecordAction(base::UserMetricsAction(
          "PasswordManager.PasswordDropdownSelected.Passkey"));
      break;
    case PasswordDropdownSelectedOption::kWebAuthnSignInWithAnotherDevice:
      base::RecordAction(base::UserMetricsAction(
          "PasswordManager.PasswordDropdownSelected.UseAnotherDevice"));
      break;
    case PasswordDropdownSelectedOption::kShowAll:
    case PasswordDropdownSelectedOption::kGenerate:
    case PasswordDropdownSelectedOption::kUnlockAccountStorePasswords:
    case PasswordDropdownSelectedOption::kResigninToUnlockAccountStore:
    case PasswordDropdownSelectedOption::kUnlockAccountStoreGeneration:
    default:
      base::RecordAction(base::UserMetricsAction(
          "PasswordManager.PasswordDropdownSelected.Others"));
      break;
  }
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

void LogPasswordSettingsReauthResult(device_reauth::ReauthResult result) {
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
    password_manager::features_util::PasswordAccountStorageUsageLevel
        account_storage_usage_level,
    ukm::SourceId ukm_source_id) {
  ukm::builders::PasswordManager_NewlySavedPassword ukm_entry_builder(
      ukm_source_id);

  base::UmaHistogramBoolean("PasswordManager.NewlySavedPasswordIsGenerated",
                            is_generated_password);
  ukm_entry_builder.SetIsPasswordGenerated(is_generated_password);
  std::string suffix = GetPasswordAccountStorageUsageLevelHistogramSuffix(
      account_storage_usage_level);
  base::UmaHistogramBoolean(
      "PasswordManager.NewlySavedPasswordIsGenerated." + suffix,
      is_generated_password);

  base::UmaHistogramBoolean(
      "PasswordManager.NewlySavedPasswordHasEmptyUsername.Overall",
      is_username_empty);
  ukm_entry_builder.SetHasEmptyUsername(is_username_empty);
  base::UmaHistogramBoolean(
      base::StrCat({"PasswordManager.NewlySavedPasswordHasEmptyUsername.",
                    is_generated_password ? "AutoGenerated" : "UserCreated"}),
      is_username_empty);

  ukm_entry_builder.Record(ukm::UkmRecorder::Get());
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
    case PasswordGenerationType::kTouchToFill:
      base::UmaHistogramEnumeration(
          "PasswordManager.TouchToFill.PasswordGeneration.UserChoice", choice);
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

void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state) {
  base::UmaHistogramEnumeration("PasswordManager.IsSyncPasswordHashSaved",
                                state);
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

void LogProcessIncomingPasswordSharingInvitationResult(
    ProcessIncomingPasswordSharingInvitationResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult", result);
}

#if BUILDFLAG(IS_ANDROID)
void LogLocalPwdMigrationProgressState(
    LocalPwdMigrationProgressState scheduling_state) {
  base::UmaHistogramEnumeration(
      "PasswordManager.UnifiedPasswordManager.MigrationForLocalUsers."
      "ProgressState",
      scheduling_state);
}

void LogTouchToFillPasswordGenerationTriggerOutcome(
    TouchToFillPasswordGenerationTriggerOutcome outcome) {
  base::UmaHistogramEnumeration(
      "PasswordManager.TouchToFill.PasswordGeneration.TriggerOutcome", outcome);
}

void LogFormSubmissionsVsSavePromptsHistogram(SaveFlowStep save_flow_step) {
  base::UmaHistogramEnumeration("PasswordManager.FormSubmissionsVsSavePrompts",
                                save_flow_step);
}
#endif

void AddPasswordRemovalReason(
    PrefService* prefs,
    IsAccountStore is_account_store,
    PasswordManagerCredentialRemovalReason removal_reason) {
  static_assert(
      static_cast<int>(PasswordManagerCredentialRemovalReason::kMaxValue) < 31);
  const std::string pref = is_account_store.value()
                               ? prefs::kPasswordRemovalReasonForAccount
                               : prefs::kPasswordRemovalReasonForProfile;
  int pwd_removal_reasons = prefs->GetInteger(pref);
  pwd_removal_reasons |= 1 << static_cast<int>(removal_reason);
  prefs->SetInteger(pref, pwd_removal_reasons);
}

void MaybeLogMetricsForPasswordAndWebauthnCounts(
    const std::vector<autofill::Suggestion>& suggestions,
    bool is_for_webauthn_request) {
  PasswordAndPasskeyCounts counts =
      GetPasswordPasskeyCountsAndUseAnotherDeviceShown(suggestions);

  // If there are no passwords or passkeys, then this is a dropdown with other
  // elements. Examples include :
  // - a dropdown with only password generation
  // - a dropdown with opt-in option to account storage
  if ((counts.password_count + counts.passkey_count == 0) &&
      !counts.has_another_device) {
    return;
  }

  std::string_view prefix = "PasswordManager.PasswordDropdownShown.";
  base::UmaHistogramCounts100(base::StrCat({prefix, "TotalCount"}),
                              counts.password_count + counts.passkey_count);
  if (is_for_webauthn_request) {
    std::string_view webauthn_request = "WebAuthnRequest.";
    base::UmaHistogramCounts100(
        base::StrCat({prefix, webauthn_request, "PasswordCount"}),
        counts.password_count);
    base::UmaHistogramCounts100(
        base::StrCat({prefix, webauthn_request, "PasskeyCount"}),
        counts.passkey_count);
    base::UmaHistogramCounts100(
        base::StrCat({prefix, webauthn_request, "TotalCount"}),
        counts.password_count + counts.passkey_count);
    base::UmaHistogramBoolean(
        base::StrCat({prefix, webauthn_request, "UseAnotherDeviceShown"}),
        counts.has_another_device);
    if (counts.password_count > 0 && counts.passkey_count > 0) {
      base::RecordAction(
          base::UserMetricsAction("PasswordManager.PasswordDropdownShown."
                                  "WebAuthnRequest.PasswordsAndPasskeys"));
    } else if (counts.password_count > 0) {
      base::RecordAction(
          base::UserMetricsAction("PasswordManager.PasswordDropdownShown."
                                  "WebAuthnRequest.OnlyPasswords"));
    } else if (counts.passkey_count > 0) {
      base::RecordAction(
          base::UserMetricsAction("PasswordManager.PasswordDropdownShown."
                                  "WebAuthnRequest.OnlyPasskeys"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("PasswordManager.PasswordDropdownShown."
                                  "WebAuthnRequest.OnlyUseAnotherDevice"));
    }
  } else {
    std::string_view non_webauthn_request = "NonWebAuthnRequest.";
    base::UmaHistogramCounts100(
        base::StrCat({prefix, non_webauthn_request, "TotalCount"}),
        counts.password_count);
    base::RecordAction(base::UserMetricsAction(
        "PasswordManager.PasswordDropdownShown.NonWebAuthnRequest"));
    // Non-WebAuthn requests cannot have passkeys or use another device options.
  }
}

void LogPasswordDropdownHidden() {
  base::RecordAction(
      base::UserMetricsAction("PasswordManager.PasswordDropdownHidden"));
}
}  // namespace password_manager::metrics_util
