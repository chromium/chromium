// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <stddef.h>

#include <string>

#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"

namespace password_manager {

namespace metrics_util {

using IsUsernameChanged = util::StrongAlias<class IsUsernameChangedTag, bool>;
using IsPasswordChanged = util::StrongAlias<class IsPasswordChangedTag, bool>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordBubble.DisplayDisposition"
enum UIDisplayDisposition {
  AUTOMATIC_WITH_PASSWORD_PENDING = 0,
  MANUAL_WITH_PASSWORD_PENDING = 1,
  MANUAL_MANAGE_PASSWORDS = 2,
  MANUAL_BLACKLISTED_OBSOLETE = 3,  // obsolete.
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION = 4,
  AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE = 5,  // obsolete
  AUTOMATIC_SIGNIN_TOAST = 6,
  MANUAL_WITH_PASSWORD_PENDING_UPDATE = 7,
  AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE = 8,
  MANUAL_GENERATED_PASSWORD_CONFIRMATION = 9,
  AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY = 10,
  AUTOMATIC_COMPROMISED_CREDENTIALS_REMINDER = 11,
  AUTOMATIC_MOVE_TO_ACCOUNT_STORE = 12,
  NUM_DISPLAY_DISPOSITIONS,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.UIDismissalReason"
enum UIDismissalReason {
  // We use this to mean both "Bubble lost focus" and "No interaction with the
  // infobar".
  NO_DIRECT_INTERACTION = 0,
  CLICKED_ACCEPT = 1,
  CLICKED_CANCEL = 2,
  CLICKED_NEVER = 3,
  CLICKED_MANAGE = 4,
  CLICKED_DONE_OBSOLETE = 5,         // obsolete
  CLICKED_UNBLACKLIST_OBSOLETE = 6,  // obsolete.
  CLICKED_OK_OBSOLETE = 7,           // obsolete
  CLICKED_CREDENTIAL_OBSOLETE = 8,   // obsolete.
  AUTO_SIGNIN_TOAST_TIMEOUT = 9,
  AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE = 10,  // obsolete.
  CLICKED_BRAND_NAME_OBSOLETE = 11,         // obsolete.
  CLICKED_PASSWORDS_DASHBOARD = 12,
  NUM_UI_RESPONSES,
};

// Enum representing the different leak detection dialogs shown to the user.
// Corresponds to LeakDetectionDialogType suffix in histograms.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LeakDialogType {
  // The user is asked to visit the Password Checkup.
  kCheckup = 0,
  // The user is asked to change the password for the current site.
  kChange = 1,
  // The user is asked to visit the Password Checkup and change the password for
  // the current site.
  kCheckupAndChange = 2,
  kMaxValue = kCheckupAndChange,
};

// Enum recording the dismissal reason of the data breach dialog which is shown
// in case a credential is reported as leaked. Needs to stay in sync with the
// PasswordLeakDetectionDialogDismissalReason enum in enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LeakDialogDismissalReason {
  kNoDirectInteraction = 0,
  kClickedClose = 1,
  kClickedCheckPasswords = 2,
  kClickedOk = 3,
  kMaxValue = kClickedOk,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FormDeserializationStatus {
  LOGIN_DATABASE_SUCCESS = 0,
  LOGIN_DATABASE_FAILURE = 1,
  LIBSECRET_SUCCESS = 2,
  LIBSECRET_FAILURE = 3,
  GNOME_SUCCESS = 4,
  GNOME_FAILURE = 5,
  NUM_DESERIALIZATION_STATUSES
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.PasswordSyncState"
enum PasswordSyncState {
  SYNCING_OK = 0,
  NOT_SYNCING_FAILED_READ = 1,
  NOT_SYNCING_DUPLICATE_TAGS = 2,
  NOT_SYNCING_SERVER_ERROR = 3,
  NOT_SYNCING_FAILED_CLEANUP = 4,
  NOT_SYNCING_FAILED_DECRYPTION = 5,
  NOT_SYNCING_FAILED_ADD = 6,
  NOT_SYNCING_FAILED_UPDATE = 7,
  NOT_SYNCING_FAILED_METADATA_PERSISTENCE = 8,
  NUM_SYNC_STATES
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.ApplySyncChangesState"
enum class ApplySyncChangesState {
  kApplyOK = 0,
  kApplyAddFailed = 1,
  kApplyUpdateFailed = 2,
  kApplyDeleteFailed = 3,
  kApplyMetadataChangesFailed = 4,

  kMaxValue = kApplyMetadataChangesFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordGeneration.SubmissionEvent"
enum PasswordSubmissionEvent {
  PASSWORD_SUBMITTED = 0,
  PASSWORD_SUBMISSION_FAILED = 1,
  PASSWORD_NOT_SUBMITTED = 2,
  PASSWORD_OVERRIDDEN = 3,
  PASSWORD_USED = 4,
  GENERATED_PASSWORD_FORCE_SAVED = 5,
  SUBMISSION_EVENT_ENUM_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum AutoSigninPromoUserAction {
  AUTO_SIGNIN_NO_ACTION = 0,
  AUTO_SIGNIN_TURN_OFF = 1,
  AUTO_SIGNIN_OK_GOT_IT = 2,
  AUTO_SIGNIN_PROMO_ACTION_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum AccountChooserUserAction {
  ACCOUNT_CHOOSER_DISMISSED = 0,
  ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN = 1,
  ACCOUNT_CHOOSER_SIGN_IN = 2,
  ACCOUNT_CHOOSER_ACTION_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.Mediation{Silent,Optional,Required}"
enum class CredentialManagerGetResult {
  // The promise is rejected.
  kRejected = 0,
  // Auto sign-in is not allowed in the current context.
  kNoneZeroClickOff = 1,
  // No matching credentials found.
  kNoneEmptyStore = 2,
  // User mediation required due to > 1 matching credentials.
  kNoneManyCredentials = 3,
  // User mediation required due to the signed out state.
  kNoneSignedOut = 4,
  // User mediation required due to pending first run experience dialog.
  kNoneFirstRun = 5,
  // Return empty credential for whatever reason.
  kNone = 6,
  // Return a credential from the account chooser.
  kAccountChooser = 7,
  // User is auto signed in.
  kAutoSignIn = 8,
  // No credentials are returned in incognito mode.
  kNoneIncognito = 9,
  // No credentials are returned while autofill_assistant is running.
  kNoneAutofillAssistant = 10,
  kMaxValue = kNoneAutofillAssistant,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum PasswordReusePasswordFieldDetected {
  NO_PASSWORD_FIELD = 0,
  HAS_PASSWORD_FIELD = 1,
  PASSWORD_REUSE_PASSWORD_FIELD_DETECTED_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SubmittedFormFrame {
  MAIN_FRAME = 0,
  IFRAME_WITH_SAME_URL_AS_MAIN_FRAME = 1,
  IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME = 2,
  IFRAME_WITH_DIFFERENT_SIGNON_REALM = 3,
  SUBMITTED_FORM_FRAME_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.AccessPasswordInSettings"
enum AccessPasswordInSettingsEvent {
  ACCESS_PASSWORD_VIEWED = 0,
  ACCESS_PASSWORD_COPIED = 1,
  ACCESS_PASSWORD_EDITED = 2,
  ACCESS_PASSWORD_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to stay in sync with
// "PasswordManager.ReauthResult" in enums.xml.
// Metrics: PasswordManager.ReauthToAccessPasswordInSettings
enum class ReauthResult {
  kSuccess = 0,
  kFailure = 1,
  kSkipped = 2,
  kMaxValue = kSkipped,
};

// Specifies the type of PasswordFormManagers and derived classes to distinguish
// the context in which a PasswordFormManager is being created and used.
enum class CredentialSourceType {
  kUnknown = 0,
  // This is used for form based credential management (PasswordFormManager).
  kPasswordManager = 1,
  // This is used for credential management API based credential management
  // (CredentialManagerPasswordFormManager).
  kCredentialManagementAPI = 2
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: PasswordManager.DeleteCorruptedPasswordsResult
// Metrics: PasswordManager.DeleteUndecryptableLoginsReturnValue
// A passwords is considered corrupted if it's stored locally using lost
// encryption key.
enum class DeleteCorruptedPasswordsResult {
  // No corrupted entries were deleted.
  kSuccessNoDeletions = 0,
  // There were corrupted entries that were successfully deleted.
  kSuccessPasswordsDeleted = 1,
  // There was at least one corrupted entry that failed to be removed (it's
  // possible that other corrupted entries were deleted).
  kItemFailure = 2,
  // Encryption is unavailable, it's impossible to determine which entries are
  // corrupted.
  kEncryptionUnavailable = 3,
  kMaxValue = kEncryptionUnavailable,
};

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
enum class GaiaPasswordHashChange {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Password hash saved event where the account is used to sign in to Chrome
  // (syncing).
  SAVED_ON_CHROME_SIGNIN = 0,
  // Password hash saved in content area.
  SAVED_IN_CONTENT_AREA = 1,
  // Clear password hash when the account is signed out of Chrome.
  CLEARED_ON_CHROME_SIGNOUT = 2,
  // Password hash changed event where the account is used to sign in to Chrome
  // (syncing).
  CHANGED_IN_CONTENT_AREA = 3,
  // Password hash changed event where the account is not syncing.
  NOT_SYNC_PASSWORD_CHANGE = 4,
  // Password hash change event for non-GAIA enterprise accounts.
  NON_GAIA_ENTERPRISE_PASSWORD_CHANGE = 5,
  SAVED_SYNC_PASSWORD_CHANGE_COUNT = 6,
  kMaxValue = SAVED_SYNC_PASSWORD_CHANGE_COUNT,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IsSyncPasswordHashSaved {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  NOT_SAVED = 0,
  SAVED_VIA_STRING_PREF = 1,
  SAVED_VIA_LIST_PREF = 2,
  IS_SYNC_PASSWORD_HASH_SAVED_COUNT = 3,
  kMaxValue = IS_SYNC_PASSWORD_HASH_SAVED_COUNT,
};
#endif

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.CertificateErrorsWhileSeeingForms"
enum class CertificateError {
  NONE = 0,
  OTHER = 1,
  AUTHORITY_INVALID = 2,
  DATE_INVALID = 3,
  COMMON_NAME_INVALID = 4,
  WEAK_SIGNATURE_ALGORITHM = 5,
  COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metric: "PasswordManager.ReusedPasswordType".
enum class PasswordType {
  // Passwords saved by password manager.
  SAVED_PASSWORD = 0,
  // Passwords used for Chrome sign-in and is closest ("blessed") to be set to
  // sync when signed into multiple profiles if user wants to set up sync.
  // The primary account is equivalent to the "sync account" if this profile has
  // enabled sync.
  PRIMARY_ACCOUNT_PASSWORD = 1,
  // Other Gaia passwords used in Chrome other than the sync password.
  OTHER_GAIA_PASSWORD = 2,
  // Passwords captured from enterprise login page.
  ENTERPRISE_PASSWORD = 3,
  // Unknown password type. Used by downstream code to indicate there was not a
  // password reuse.
  PASSWORD_TYPE_UNKNOWN = 4,
  PASSWORD_TYPE_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LinuxBackendMigrationStatus {
  // No migration was attempted (this value should not occur).
  kNotAttempted = 0,
  // The last attempt was not completed.
  kDeprecatedFailed = 1,
  // All the data is in the encrypted loginDB.
  kDeprecatedCopiedAll = 2,
  // The standard login database is encrypted.
  kLoginDBReplaced = 3,
  // The migration is about to be attempted.
  kStarted = 4,
  // No access to the native backend.
  kPostponed = 5,
  // Could not create or write into the temporary file. Deprecated and replaced
  // by more precise errors.
  kDeprecatedFailedCreatedEncrypted = 6,
  // Could not read from the native backend.
  kDeprecatedFailedAccessNative = 7,
  // Could not replace old database.
  kFailedReplace = 8,
  // Could not initialise the temporary encrypted database.
  kFailedInitEncrypted = 9,
  // Could not reset th temporary encrypted database.
  kDeprecatedFailedRecreateEncrypted = 10,
  // Could not add entries into the temporary encrypted database.
  kFailedWriteToEncrypted = 11,
  kMaxValue = kFailedWriteToEncrypted
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Type of the password drop-down shown on focus field.
enum class PasswordDropdownState {
  // The passwords are listed and maybe the "Show all" button.
  kStandard = 0,
  // The drop down shows passwords and "Generate password" item.
  kStandardGenerate = 1,
  kMaxValue = kStandardGenerate
};

// Type of the item the user selects in the password drop-down.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordDropdownSelectedOption {
  // User selected a credential to fill.
  kPassword = 0,
  // User decided to open the password list.
  kShowAll = 1,
  // User selected to generate a password.
  kGenerate = 2,
  // User unlocked the account-store to fill a password.
  kUnlockAccountStorePasswords = 3,
  // User unlocked the account-store to generate a password.
  kUnlockAccountStoreGeneration = 4,
  // Previoulsy opted-in user decided to log-in again to access their passwords.
  kResigninToUnlockAccountStore = 5,
  kMaxValue = kResigninToUnlockAccountStore
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metric: "KeyboardAccessory.GenerationDialogChoice.{Automatic, Manual}".
enum class GenerationDialogChoice {
  // The user accepted the generated password.
  kAccepted = 0,
  // The user rejected the generated password.
  kRejected = 1,
  kMaxValue = kRejected
};

// Represents the state of the user wrt. sign-in and account-scoped storage.
// Used for metrics. Always keep this enum in sync with the corresponding
// histogram_suffixes in histograms.xml!
enum class PasswordAccountStorageUserState {
  // Signed-out user (and no account storage opt-in exists).
  kSignedOutUser = 0,
  // Signed-out user, but an account storage opt-in exists.
  kSignedOutAccountStoreUser = 1,
  // Signed-in user, not opted in to the account storage (but will save
  // passwords to the account storage by default).
  kSignedInUser = 2,
  // Signed-in user, not opted in to the account storage, and has explicitly
  // chosen to save passwords only on the device.
  kSignedInUserSavingLocally = 3,
  // Signed-in user, opted in to the account storage, and saving passwords to
  // the account storage.
  kSignedInAccountStoreUser = 4,
  // Signed-in user and opted in to the account storage, but has chosen to save
  // passwords only on the device.
  kSignedInAccountStoreUserSavingLocally = 5,
  // Syncing user.
  kSyncUser = 6,
};

// Represents different user interactions related to password check.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Always keep this enum in sync with the
// corresponding PasswordCheckInteraction in enums.xml and
// password_manager_proxy.js.
enum class PasswordCheckInteraction {
  kAutomaticPasswordCheck = 0,
  kManualPasswordCheck = 1,
  kPasswordCheckStopped = 2,
  kChangePassword = 3,
  kEditPassword = 4,
  kRemovePassword = 5,
  kShowPassword = 6,
  // Must be last.
  kMaxValue = kShowPassword,
};

// Metrics: PasswordManager.MoveToAccountStoreTrigger.
// This must be kept in sync with the enum in password_move_to_account_dialog.js
// (in chrome/browser/resources/settings/autofill_page).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MoveToAccountStoreTrigger {
  // The user successfully logged in with a password from the profile store.
  kSuccessfulLoginWithProfileStorePassword = 0,
  // The user explicitly asked to move a password listed in Settings.
  kExplicitlyTriggeredInSettings = 1,
  kMaxValue = kExplicitlyTriggeredInSettings,
};

// Used to record metrics for the usage and timing of the GetChangePasswordUrl
// call. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class GetChangePasswordUrlMetric {
  // Used when GetChangePasswordUrl is called before the response
  // arrives.
  kNotFetchedYet = 0,
  // Used when a url was used, which corresponds to the requested site.
  kUrlOverrideUsed = 1,
  // Used when no override url was available.
  kNoUrlOverrideAvailable = 2,
  // Used when a url was used, which corresponds to a site from within same
  // FacetGroup.
  kGroupUrlOverrideUsed = 3,
  kMaxValue = kGroupUrlOverrideUsed,
};

// Used to record what exactly was updated during password editing flow.
// Entries should not be renumbered and numeric values should never be reused.
enum class PasswordEditUpdatedValues {
  // Nothing was updated.
  kNone = 0,
  // Only username was changed.
  kUsername = 1,
  // Only password was changed.
  kPassword = 2,
  // Both password and username were updated.
  kBoth = 3,
  kMaxValue = kBoth,
};

std::string GetPasswordAccountStorageUserStateHistogramSuffix(
    PasswordAccountStorageUserState user_state);

// The usage level of the account-scoped password storage. This is essentially
// a less-detailed version of PasswordAccountStorageUserState, for metrics that
// don't need the fully-detailed breakdown.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordAccountStorageUsageLevel {
  // The user is not using the account-scoped password storage. Either they're
  // not signed in, or they haven't opted in to the account storage.
  kNotUsingAccountStorage = 0,
  // The user is signed in and has opted in to the account storage.
  kUsingAccountStorage = 1,
  // The user has enabled Sync.
  kSyncing = 2,
};
std::string GetPasswordAccountStorageUsageLevelHistogramSuffix(
    PasswordAccountStorageUsageLevel usage_level);

// Log the |reason| a user dismissed the password manager UI except save/update
// bubbles.
void LogGeneralUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the save password bubble. If
// |user_state| is set, the |reason| is also logged to a separate
// user-state-specific histogram.
void LogSaveUIDismissalReason(
    UIDismissalReason reason,
    base::Optional<PasswordAccountStorageUserState> user_state);

// Log the |reason| a user dismissed the save password prompt after previously
// having unblacklisted the origin while on the page.
void LogSaveUIDismissalReasonAfterUnblacklisting(UIDismissalReason reason);

// Log the |reason| a user dismissed the update password bubble.
void LogUpdateUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the move password bubble.
void LogMoveUIDismissalReason(UIDismissalReason reason,
                              PasswordAccountStorageUserState user_state);

// Log the |type| of a leak dialog shown to the user and the |reason| why it was
// dismissed.
void LogLeakDialogTypeAndDismissalReason(LeakDialogType type,
                                         LeakDialogDismissalReason reason);

// Log the appropriate display disposition.
void LogUIDisplayDisposition(UIDisplayDisposition disposition);

// Log if a saved FormData was deserialized correctly.
void LogFormDataDeserializationStatus(FormDeserializationStatus status);

// When a credential was filled, log whether it came from an Android app.
void LogFilledCredentialIsFromAndroidApp(bool from_android);

// Log what's preventing passwords from syncing.
void LogPasswordSyncState(PasswordSyncState state);

// Log what's preventing passwords from applying sync changes.
void LogApplySyncChangesState(ApplySyncChangesState state);

// Log submission events related to generation.
void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event);

// Log when password generation is available for a particular form.
void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event);

// Log a user action on showing the autosignin first run experience.
void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action);

// Log a user action on showing the account chooser for one or many accounts.
void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action);
void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action);

// Log the result of navigator.credentials.get.
void LogCredentialManagerGetResult(CredentialManagerGetResult result,
                                   CredentialMediationRequirement mediation);

// Log the password reuse.
void LogPasswordReuse(int password_length,
                      int saved_passwords,
                      int number_matches,
                      bool password_field_detected,
                      PasswordType reused_password_type);

// Log the type of the password dropdown when it's shown.
void LogPasswordDropdownShown(PasswordDropdownState state, bool off_the_record);

// Log the type of the password dropdown suggestion when chosen.
void LogPasswordDropdownItemSelected(PasswordDropdownSelectedOption type,
                                     bool off_the_record);

// Log a password successful submission event.
void LogPasswordSuccessfulSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event);

// Log a password successful submission event for accepted by user password save
// or update.
void LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event);

// Log a frame of a submitted password form.
void LogSubmittedFormFrame(SubmittedFormFrame frame);

// Logs how many account-stored passwords are available right after unlock.
void LogPasswordsCountFromAccountStoreAfterUnlock(int account_store_passwords);

// Logs the result of a re-auth challenge in the password settings.
void LogPasswordSettingsReauthResult(ReauthResult result);

// Log a return value of LoginDatabase::DeleteUndecryptableLogins method.
void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result);

// Log whether a saved password was generated.
void LogNewlySavedPasswordIsGenerated(
    bool value,
    PasswordAccountStorageUsageLevel account_storage_usage_level);

// Log whether the generated password was accepted or rejected for generation of
// |type| (automatic or manual).
void LogGenerationDialogChoice(
    GenerationDialogChoice choice,
    autofill::password_generation::PasswordGenerationType type);

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
// Log a save gaia password change event.
void LogGaiaPasswordHashChange(GaiaPasswordHashChange event,
                               bool is_sync_password);

// Log whether a sync password hash saved.
void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state,
                                bool is_under_advanced_protection);

// Log the number of Gaia password hashes saved, and the number of enterprise
// password hashes saved. Currently only called on profile start up.
void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    size_t enterprise_hash_count,
                                    bool does_primary_account_exists,
                                    bool is_signed_in);

#endif

// Log the result of the password edit action.
void LogPasswordEditResult(IsUsernameChanged password_changed,
                           IsPasswordChanged username_changed);

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
