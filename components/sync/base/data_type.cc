// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type.h"

#include <ostream>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

namespace {

static_assert(63 == syncer::GetNumDataTypes(),
              "When adding a new type, update enum SyncDataTypes in enums.xml "
              "and suffix SyncDataType in histograms.xml.");

static_assert(63 == syncer::GetNumDataTypes(),
              "When adding a new type, follow the integration checklist in "
              "https://www.chromium.org/developers/design-documents/sync/"
              "integration-checklist/");

// kSpecificsFieldNumberToDataTypeMap must have size syncer::GetNumDataTypes().
//
// NOTE: size here acts as a static assert on the constraint above.
using kSpecificsFieldNumberToDataTypeMap =
    base::fixed_flat_map<int, DataType, syncer::GetNumDataTypes()>;

constexpr kSpecificsFieldNumberToDataTypeMap specifics_field_number2data_type =
    base::MakeFixedFlatMap<int, DataType>({
        {-1, UNSPECIFIED},
        {sync_pb::EntitySpecifics::kBookmarkFieldNumber, BOOKMARKS},
        {sync_pb::EntitySpecifics::kPreferenceFieldNumber, PREFERENCES},
        {sync_pb::EntitySpecifics::kPasswordFieldNumber, PASSWORDS},
        {sync_pb::EntitySpecifics::kAutofillProfileFieldNumber,
         AUTOFILL_PROFILE},
        {sync_pb::EntitySpecifics::kAutofillFieldNumber, AUTOFILL},
        {sync_pb::EntitySpecifics::kAutofillWalletCredentialFieldNumber,
         AUTOFILL_WALLET_CREDENTIAL},
        {sync_pb::EntitySpecifics::kAutofillWalletFieldNumber,
         AUTOFILL_WALLET_DATA},
        {sync_pb::EntitySpecifics::kWalletMetadataFieldNumber,
         AUTOFILL_WALLET_METADATA},
        {sync_pb::EntitySpecifics::kAutofillOfferFieldNumber,
         AUTOFILL_WALLET_OFFER},
        {sync_pb::EntitySpecifics::kAutofillWalletUsageFieldNumber,
         AUTOFILL_WALLET_USAGE},
        {sync_pb::EntitySpecifics::kThemeFieldNumber, THEMES},
        {sync_pb::EntitySpecifics::kExtensionFieldNumber, EXTENSIONS},
        {sync_pb::EntitySpecifics::kSearchEngineFieldNumber, SEARCH_ENGINES},
        {sync_pb::EntitySpecifics::kSessionFieldNumber, SESSIONS},
        {sync_pb::EntitySpecifics::kAppFieldNumber, APPS},
        {sync_pb::EntitySpecifics::kAppSettingFieldNumber, APP_SETTINGS},
        {sync_pb::EntitySpecifics::kExtensionSettingFieldNumber,
         EXTENSION_SETTINGS},
        {sync_pb::EntitySpecifics::kHistoryDeleteDirectiveFieldNumber,
         HISTORY_DELETE_DIRECTIVES},
        {sync_pb::EntitySpecifics::kDictionaryFieldNumber, DICTIONARY},
        {sync_pb::EntitySpecifics::kDeviceInfoFieldNumber, DEVICE_INFO},
        {sync_pb::EntitySpecifics::kPriorityPreferenceFieldNumber,
         PRIORITY_PREFERENCES},
        {sync_pb::EntitySpecifics::kManagedUserSettingFieldNumber,
         SUPERVISED_USER_SETTINGS},
        {sync_pb::EntitySpecifics::kAppListFieldNumber, APP_LIST},
        {sync_pb::EntitySpecifics::kArcPackageFieldNumber, ARC_PACKAGE},
        {sync_pb::EntitySpecifics::kPrinterFieldNumber, PRINTERS},
        {sync_pb::EntitySpecifics::kReadingListFieldNumber, READING_LIST},
        {sync_pb::EntitySpecifics::kUserEventFieldNumber, USER_EVENTS},
        {sync_pb::EntitySpecifics::kUserConsentFieldNumber, USER_CONSENTS},
        {sync_pb::EntitySpecifics::kSendTabToSelfFieldNumber, SEND_TAB_TO_SELF},
        {sync_pb::EntitySpecifics::kSecurityEventFieldNumber, SECURITY_EVENTS},
        {sync_pb::EntitySpecifics::kWifiConfigurationFieldNumber,
         WIFI_CONFIGURATIONS},
        {sync_pb::EntitySpecifics::kWebAppFieldNumber, WEB_APPS},
        {sync_pb::EntitySpecifics::kWebApkFieldNumber, WEB_APKS},
        {sync_pb::EntitySpecifics::kOsPreferenceFieldNumber, OS_PREFERENCES},
        {sync_pb::EntitySpecifics::kOsPriorityPreferenceFieldNumber,
         OS_PRIORITY_PREFERENCES},
        {sync_pb::EntitySpecifics::kSharingMessageFieldNumber, SHARING_MESSAGE},
        {sync_pb::EntitySpecifics::kWorkspaceDeskFieldNumber, WORKSPACE_DESK},
        {sync_pb::EntitySpecifics::kHistoryFieldNumber, HISTORY},
        {sync_pb::EntitySpecifics::kPrintersAuthorizationServerFieldNumber,
         PRINTERS_AUTHORIZATION_SERVERS},
        {sync_pb::EntitySpecifics::kContactInfoFieldNumber, CONTACT_INFO},
        {sync_pb::EntitySpecifics::kSavedTabGroupFieldNumber, SAVED_TAB_GROUP},
        {sync_pb::EntitySpecifics::kWebauthnCredentialFieldNumber,
         WEBAUTHN_CREDENTIAL},
        {sync_pb::EntitySpecifics::
             kIncomingPasswordSharingInvitationFieldNumber,
         INCOMING_PASSWORD_SHARING_INVITATION},
        {sync_pb::EntitySpecifics::
             kOutgoingPasswordSharingInvitationFieldNumber,
         OUTGOING_PASSWORD_SHARING_INVITATION},
        {sync_pb::EntitySpecifics::kSharedTabGroupDataFieldNumber,
         SHARED_TAB_GROUP_DATA},
        {sync_pb::EntitySpecifics::kCollaborationGroupFieldNumber,
         COLLABORATION_GROUP},
        {sync_pb::EntitySpecifics::kPlusAddressFieldNumber, PLUS_ADDRESS},
        {sync_pb::EntitySpecifics::kProductComparisonFieldNumber,
         PRODUCT_COMPARISON},
        {sync_pb::EntitySpecifics::kCookieFieldNumber, COOKIES},
        {sync_pb::EntitySpecifics::kPlusAddressSettingFieldNumber,
         PLUS_ADDRESS_SETTING},
        {sync_pb::EntitySpecifics::kAutofillValuableFieldNumber,
         AUTOFILL_VALUABLE},
        {sync_pb::EntitySpecifics::kAutofillValuableMetadataFieldNumber,
         AUTOFILL_VALUABLE_METADATA},
        {sync_pb::EntitySpecifics::kSharedTabGroupAccountDataFieldNumber,
         SHARED_TAB_GROUP_ACCOUNT_DATA},
        {sync_pb::EntitySpecifics::kAccountSettingFieldNumber, ACCOUNT_SETTING},
        {sync_pb::EntitySpecifics::kSharedCommentFieldNumber, SHARED_COMMENT},
        {sync_pb::EntitySpecifics::kAiThreadFieldNumber, AI_THREAD},
        {sync_pb::EntitySpecifics::kContextualTaskFieldNumber, CONTEXTUAL_TASK},
        {sync_pb::EntitySpecifics::kSkillFieldNumber, SKILL},
        {sync_pb::EntitySpecifics::kGeminiThreadFieldNumber, GEMINI_THREAD},
        {sync_pb::EntitySpecifics::kThemeIosFieldNumber, THEMES_IOS},
        {sync_pb::EntitySpecifics::kAccessibilityAnnotationFieldNumber,
         ACCESSIBILITY_ANNOTATION},
        // ---- Control Types ----
        {sync_pb::EntitySpecifics::kNigoriFieldNumber, NIGORI},
    });

}  // namespace

void AddDefaultFieldValue(DataType type, sync_pb::EntitySpecifics* specifics) {
  switch (type) {
    case UNSPECIFIED:
      NOTREACHED() << "No default field value for "
                   << DataTypeToDebugString(type);
    case BOOKMARKS:
      specifics->mutable_bookmark();
      break;
    case PREFERENCES:
      specifics->mutable_preference();
      break;
    case PASSWORDS:
      specifics->mutable_password();
      break;
    case AUTOFILL_PROFILE:
      specifics->mutable_autofill_profile();
      break;
    case AUTOFILL:
      specifics->mutable_autofill();
      break;
    case AUTOFILL_WALLET_CREDENTIAL:
      specifics->mutable_autofill_wallet_credential();
      break;
    case AUTOFILL_WALLET_DATA:
      specifics->mutable_autofill_wallet();
      break;
    case AUTOFILL_WALLET_METADATA:
      specifics->mutable_wallet_metadata();
      break;
    case AUTOFILL_WALLET_OFFER:
      specifics->mutable_autofill_offer();
      break;
    case AUTOFILL_WALLET_USAGE:
      specifics->mutable_autofill_wallet_usage();
      break;
    case THEMES:
      specifics->mutable_theme();
      break;
    case THEMES_IOS:
      specifics->mutable_theme_ios();
      break;
    case EXTENSIONS:
      specifics->mutable_extension();
      break;
    case SEARCH_ENGINES:
      specifics->mutable_search_engine();
      break;
    case SESSIONS:
      specifics->mutable_session();
      break;
    case APPS:
      specifics->mutable_app();
      break;
    case APP_SETTINGS:
      specifics->mutable_app_setting();
      break;
    case EXTENSION_SETTINGS:
      specifics->mutable_extension_setting();
      break;
    case HISTORY_DELETE_DIRECTIVES:
      specifics->mutable_history_delete_directive();
      break;
    case DICTIONARY:
      specifics->mutable_dictionary();
      break;
    case DEVICE_INFO:
      specifics->mutable_device_info();
      break;
    case PRIORITY_PREFERENCES:
      specifics->mutable_priority_preference();
      break;
    case SUPERVISED_USER_SETTINGS:
      specifics->mutable_managed_user_setting();
      break;
    case APP_LIST:
      specifics->mutable_app_list();
      break;
    case ARC_PACKAGE:
      specifics->mutable_arc_package();
      break;
    case PRINTERS:
      specifics->mutable_printer();
      break;
    case PRINTERS_AUTHORIZATION_SERVERS:
      specifics->mutable_printers_authorization_server();
      break;
    case READING_LIST:
      specifics->mutable_reading_list();
      break;
    case USER_EVENTS:
      specifics->mutable_user_event();
      break;
    case SECURITY_EVENTS:
      specifics->mutable_security_event();
      break;
    case USER_CONSENTS:
      specifics->mutable_user_consent();
      break;
    case SEND_TAB_TO_SELF:
      specifics->mutable_send_tab_to_self();
      break;
    case NIGORI:
      specifics->mutable_nigori();
      break;
    case WEB_APPS:
      specifics->mutable_web_app();
      break;
    case WEB_APKS:
      specifics->mutable_web_apk();
      break;
    case WIFI_CONFIGURATIONS:
      specifics->mutable_wifi_configuration();
      break;
    case WORKSPACE_DESK:
      specifics->mutable_workspace_desk();
      break;
    case OS_PREFERENCES:
      specifics->mutable_os_preference();
      break;
    case OS_PRIORITY_PREFERENCES:
      specifics->mutable_os_priority_preference();
      break;
    case SHARING_MESSAGE:
      specifics->mutable_sharing_message();
      break;
    case HISTORY:
      specifics->mutable_history();
      break;
    case CONTACT_INFO:
      specifics->mutable_contact_info();
      break;
    case SAVED_TAB_GROUP:
      specifics->mutable_saved_tab_group();
      break;
    case WEBAUTHN_CREDENTIAL:
      specifics->mutable_webauthn_credential();
      break;
    case INCOMING_PASSWORD_SHARING_INVITATION:
      specifics->mutable_incoming_password_sharing_invitation();
      break;
    case OUTGOING_PASSWORD_SHARING_INVITATION:
      specifics->mutable_outgoing_password_sharing_invitation();
      break;
    case SHARED_TAB_GROUP_DATA:
      specifics->mutable_shared_tab_group_data();
      break;
    case COLLABORATION_GROUP:
      specifics->mutable_collaboration_group();
      break;
    case PLUS_ADDRESS:
      specifics->mutable_plus_address();
      break;
    case PRODUCT_COMPARISON:
      specifics->mutable_product_comparison();
      break;
    case COOKIES:
      specifics->mutable_cookie();
      break;
    case PLUS_ADDRESS_SETTING:
      specifics->mutable_plus_address_setting();
      break;
    case AUTOFILL_VALUABLE:
      specifics->mutable_autofill_valuable();
      break;
    case AUTOFILL_VALUABLE_METADATA:
      specifics->mutable_autofill_valuable_metadata();
      break;
    case ACCOUNT_SETTING:
      specifics->mutable_account_setting();
      break;
    case SHARED_TAB_GROUP_ACCOUNT_DATA:
      specifics->mutable_shared_tab_group_account_data();
      break;
    case SHARED_COMMENT:
      specifics->mutable_shared_comment();
      break;
    case AI_THREAD:
      specifics->mutable_ai_thread();
      break;
    case CONTEXTUAL_TASK:
      specifics->mutable_contextual_task();
      break;
    case SKILL:
      specifics->mutable_skill();
      break;
    case GEMINI_THREAD:
      specifics->mutable_gemini_thread();
      break;
    case ACCESSIBILITY_ANNOTATION:
      specifics->mutable_accessibility_annotation();
      break;
  }
}

DataType GetDataTypeFromSpecificsFieldNumber(int field_number) {
  kSpecificsFieldNumberToDataTypeMap::const_iterator it =
      specifics_field_number2data_type.find(field_number);
  return (it == specifics_field_number2data_type.end() ? UNSPECIFIED
                                                       : it->second);
}

int GetSpecificsFieldNumberFromDataType(DataType data_type) {
  DCHECK(ProtocolTypes().Has(data_type))
      << "Only protocol types have field values.";
  switch (data_type) {
    case UNSPECIFIED:
      return -1;
    case BOOKMARKS:
      return sync_pb::EntitySpecifics::kBookmarkFieldNumber;
    case PREFERENCES:
      return sync_pb::EntitySpecifics::kPreferenceFieldNumber;
    case PASSWORDS:
      return sync_pb::EntitySpecifics::kPasswordFieldNumber;
    case AUTOFILL_PROFILE:
      return sync_pb::EntitySpecifics::kAutofillProfileFieldNumber;
    case AUTOFILL:
      return sync_pb::EntitySpecifics::kAutofillFieldNumber;
    case AUTOFILL_WALLET_CREDENTIAL:
      return sync_pb::EntitySpecifics::kAutofillWalletCredentialFieldNumber;
    case AUTOFILL_WALLET_DATA:
      return sync_pb::EntitySpecifics::kAutofillWalletFieldNumber;
    case AUTOFILL_WALLET_METADATA:
      return sync_pb::EntitySpecifics::kWalletMetadataFieldNumber;
    case AUTOFILL_WALLET_OFFER:
      return sync_pb::EntitySpecifics::kAutofillOfferFieldNumber;
    case AUTOFILL_WALLET_USAGE:
      return sync_pb::EntitySpecifics::kAutofillWalletUsageFieldNumber;
    case THEMES:
      return sync_pb::EntitySpecifics::kThemeFieldNumber;
    case THEMES_IOS:
      return sync_pb::EntitySpecifics::kThemeIosFieldNumber;
    case EXTENSIONS:
      return sync_pb::EntitySpecifics::kExtensionFieldNumber;
    case SEARCH_ENGINES:
      return sync_pb::EntitySpecifics::kSearchEngineFieldNumber;
    case SESSIONS:
      return sync_pb::EntitySpecifics::kSessionFieldNumber;
    case APPS:
      return sync_pb::EntitySpecifics::kAppFieldNumber;
    case APP_SETTINGS:
      return sync_pb::EntitySpecifics::kAppSettingFieldNumber;
    case EXTENSION_SETTINGS:
      return sync_pb::EntitySpecifics::kExtensionSettingFieldNumber;
    case HISTORY_DELETE_DIRECTIVES:
      return sync_pb::EntitySpecifics::kHistoryDeleteDirectiveFieldNumber;
    case DICTIONARY:
      return sync_pb::EntitySpecifics::kDictionaryFieldNumber;
    case DEVICE_INFO:
      return sync_pb::EntitySpecifics::kDeviceInfoFieldNumber;
    case PRIORITY_PREFERENCES:
      return sync_pb::EntitySpecifics::kPriorityPreferenceFieldNumber;
    case SUPERVISED_USER_SETTINGS:
      return sync_pb::EntitySpecifics::kManagedUserSettingFieldNumber;
    case APP_LIST:
      return sync_pb::EntitySpecifics::kAppListFieldNumber;
    case ARC_PACKAGE:
      return sync_pb::EntitySpecifics::kArcPackageFieldNumber;
    case PRINTERS:
      return sync_pb::EntitySpecifics::kPrinterFieldNumber;
    case READING_LIST:
      return sync_pb::EntitySpecifics::kReadingListFieldNumber;
    case USER_EVENTS:
      return sync_pb::EntitySpecifics::kUserEventFieldNumber;
    case USER_CONSENTS:
      return sync_pb::EntitySpecifics::kUserConsentFieldNumber;
    case SEND_TAB_TO_SELF:
      return sync_pb::EntitySpecifics::kSendTabToSelfFieldNumber;
    case SECURITY_EVENTS:
      return sync_pb::EntitySpecifics::kSecurityEventFieldNumber;
    case WIFI_CONFIGURATIONS:
      return sync_pb::EntitySpecifics::kWifiConfigurationFieldNumber;
    case WEB_APPS:
      return sync_pb::EntitySpecifics::kWebAppFieldNumber;
    case WEB_APKS:
      return sync_pb::EntitySpecifics::kWebApkFieldNumber;
    case OS_PREFERENCES:
      return sync_pb::EntitySpecifics::kOsPreferenceFieldNumber;
    case OS_PRIORITY_PREFERENCES:
      return sync_pb::EntitySpecifics::kOsPriorityPreferenceFieldNumber;
    case SHARING_MESSAGE:
      return sync_pb::EntitySpecifics::kSharingMessageFieldNumber;
    case WORKSPACE_DESK:
      return sync_pb::EntitySpecifics::kWorkspaceDeskFieldNumber;
    case HISTORY:
      return sync_pb::EntitySpecifics::kHistoryFieldNumber;
    case PRINTERS_AUTHORIZATION_SERVERS:
      return sync_pb::EntitySpecifics::kPrintersAuthorizationServerFieldNumber;
    case CONTACT_INFO:
      return sync_pb::EntitySpecifics::kContactInfoFieldNumber;
    case SAVED_TAB_GROUP:
      return sync_pb::EntitySpecifics::kSavedTabGroupFieldNumber;
    case WEBAUTHN_CREDENTIAL:
      return sync_pb::EntitySpecifics::kWebauthnCredentialFieldNumber;
    case INCOMING_PASSWORD_SHARING_INVITATION:
      return sync_pb::EntitySpecifics::
          kIncomingPasswordSharingInvitationFieldNumber;
    case OUTGOING_PASSWORD_SHARING_INVITATION:
      return sync_pb::EntitySpecifics::
          kOutgoingPasswordSharingInvitationFieldNumber;
    case SHARED_TAB_GROUP_DATA:
      return sync_pb::EntitySpecifics::kSharedTabGroupDataFieldNumber;
    case COLLABORATION_GROUP:
      return sync_pb::EntitySpecifics::kCollaborationGroupFieldNumber;
    case PLUS_ADDRESS:
      return sync_pb::EntitySpecifics::kPlusAddressFieldNumber;
    case PRODUCT_COMPARISON:
      return sync_pb::EntitySpecifics::kProductComparisonFieldNumber;
    case COOKIES:
      return sync_pb::EntitySpecifics::kCookieFieldNumber;
    case PLUS_ADDRESS_SETTING:
      return sync_pb::EntitySpecifics::kPlusAddressSettingFieldNumber;
    case AUTOFILL_VALUABLE:
      return sync_pb::EntitySpecifics::kAutofillValuableFieldNumber;
    case AUTOFILL_VALUABLE_METADATA:
      return sync_pb::EntitySpecifics::kAutofillValuableMetadataFieldNumber;
    case ACCOUNT_SETTING:
      return sync_pb::EntitySpecifics::kAccountSettingFieldNumber;
    case SHARED_TAB_GROUP_ACCOUNT_DATA:
      return sync_pb::EntitySpecifics::kSharedTabGroupAccountDataFieldNumber;
    case SHARED_COMMENT:
      return sync_pb::EntitySpecifics::kSharedCommentFieldNumber;
    case AI_THREAD:
      return sync_pb::EntitySpecifics::kAiThreadFieldNumber;
    case CONTEXTUAL_TASK:
      return sync_pb::EntitySpecifics::kContextualTaskFieldNumber;
    case NIGORI:
      return sync_pb::EntitySpecifics::kNigoriFieldNumber;
    case SKILL:
      return sync_pb::EntitySpecifics::kSkillFieldNumber;
    case GEMINI_THREAD:
      return sync_pb::EntitySpecifics::kGeminiThreadFieldNumber;
    case ACCESSIBILITY_ANNOTATION:
      return sync_pb::EntitySpecifics::kAccessibilityAnnotationFieldNumber;
  }
  NOTREACHED();
}

void internal::GetDataTypeSetFromSpecificsFieldNumberListHelper(
    DataTypeSet& data_types,
    int field_number) {
  DataType data_type = GetDataTypeFromSpecificsFieldNumber(field_number);
  if (IsRealDataType(data_type)) {
    data_types.Put(data_type);
  } else {
    DLOG(WARNING) << "Unknown field number " << field_number;
  }
}

DataType GetDataTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics) {
  // SpecificsVariantCase is a generated enum that lists all possible
  // variants of the `specifics` oneof and has values corresponding to the
  // field numbers of the oneof.
  if (specifics.specifics_variant_case() ==
      sync_pb::EntitySpecifics::SPECIFICS_VARIANT_NOT_SET) {
    // This client version doesn't understand `specifics`.
    DVLOG(1) << "Unknown datatype in sync proto.";
    return UNSPECIFIED;
  }

  return GetDataTypeFromSpecificsFieldNumber(
      specifics.specifics_variant_case());
}

DataTypeSet AlwaysPreferredUserTypes() {
  // TODO(crbug.com/477624427): add SKILL to a corresponding UserSelectableType
  // or another toggle.
  DataTypeSet types = {ACCOUNT_SETTING,
                       DEVICE_INFO,
                       USER_CONSENTS,
                       PLUS_ADDRESS,
                       PLUS_ADDRESS_SETTING,
                       PRIORITY_PREFERENCES,
                       SECURITY_EVENTS,
                       SEND_TAB_TO_SELF,
                       SUPERVISED_USER_SETTINGS,
                       SHARING_MESSAGE,
                       SKILL};
  // TODO(crbug.com/412602018): Mark AlwaysPreferredUserTypes() method as
  // constexpr when removing the feature flag.
  if (!base::FeatureList::IsEnabled(
          kSyncSupportAlwaysSyncingPriorityPreferences)) {
    types.Remove(PRIORITY_PREFERENCES);
  }
  // TODO(crbug.com/486856790): add ACCESSIBILITY_ANNOTATION to a corresponding
  // UserSelectableType or another toggle once feature is finalized.
  if (base::FeatureList::IsEnabled(kSyncAccessibilityAnnotation)) {
    types.Put(ACCESSIBILITY_ANNOTATION);
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncAIThread)) {
    types.Put(AI_THREAD);
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncGeminiThread)) {
    types.Put(GEMINI_THREAD);
  }
  return types;
}

DataTypeSet EncryptableUserTypes() {
  static_assert(63 == syncer::GetNumDataTypes(),
                "If adding an unencryptable type, remove from "
                "encryptable_user_types below.");
  DataTypeSet encryptable_user_types = UserTypes();
  // Accessibility annotations are not encrypted since they originate from the
  // server.
  encryptable_user_types.Remove(ACCESSIBILITY_ANNOTATION);
  // Account settings are read-only and therefore never encrypted.
  encryptable_user_types.Remove(ACCOUNT_SETTING);
  if (base::FeatureList::IsEnabled(kSyncMakeAutofillValuableNonEncryptable)) {
    // Valuables are never encrypted because they can be generated from outside
    // of Chrome.
    encryptable_user_types.Remove(AUTOFILL_VALUABLE);
  }
  // Wallet data is not encrypted since it actually originates on the server.
  encryptable_user_types.Remove(AUTOFILL_WALLET_DATA);
  encryptable_user_types.Remove(AUTOFILL_WALLET_OFFER);
  encryptable_user_types.Remove(AUTOFILL_WALLET_USAGE);
  // Similarly, collaboration group is not encrypted since it originates on the
  // server.
  encryptable_user_types.Remove(COLLABORATION_GROUP);
  // Similarly, contact info is not encrypted since it originates on the server.
  encryptable_user_types.Remove(CONTACT_INFO);
  // Commit-only types are never encrypted since they are consumed server-side.
  encryptable_user_types.RemoveAll(CommitOnlyTypes());
  // History Sync is disabled if encryption is enabled.
  encryptable_user_types.Remove(HISTORY);
  encryptable_user_types.Remove(HISTORY_DELETE_DIRECTIVES);
  // Never encrypted because consumed server-side.
  encryptable_user_types.Remove(DEVICE_INFO);
  // Never encrypted because also written server-side.
  encryptable_user_types.Remove(PRIORITY_PREFERENCES);
  encryptable_user_types.Remove(OS_PRIORITY_PREFERENCES);
  encryptable_user_types.Remove(SUPERVISED_USER_SETTINGS);
  encryptable_user_types.Remove(SKILL);
  // Password sharing invitations have different encryption implementation.
  encryptable_user_types.Remove(INCOMING_PASSWORD_SHARING_INVITATION);
  encryptable_user_types.Remove(OUTGOING_PASSWORD_SHARING_INVITATION);
  // Never encrypted because consumed server-side.
  encryptable_user_types.Remove(SHARED_COMMENT);
  encryptable_user_types.Remove(SHARED_TAB_GROUP_DATA);
  // Plus addresses and their settings are never encrypted because they
  // originate from outside Chrome.
  encryptable_user_types.Remove(PLUS_ADDRESS);
  encryptable_user_types.Remove(PLUS_ADDRESS_SETTING);
  // Valuable metadata is accessed on the server.
  encryptable_user_types.Remove(AUTOFILL_VALUABLE_METADATA);

  return encryptable_user_types;
}

const char* DataTypeToDebugString(DataType data_type) {
  // This is used for displaying debug information.
  switch (data_type) {
    case UNSPECIFIED:
      return "Unspecified";
    case BOOKMARKS:
      return "Bookmarks";
    case PREFERENCES:
      return "Preferences";
    case PASSWORDS:
      return "Passwords";
    case AUTOFILL_PROFILE:
      return "Autofill Profiles";
    case AUTOFILL:
      return "Autofill";
    case AUTOFILL_WALLET_CREDENTIAL:
      return "Autofill Wallet Credential";
    case AUTOFILL_WALLET_DATA:
      return "Autofill Wallet";
    case AUTOFILL_WALLET_METADATA:
      return "Autofill Wallet Metadata";
    case AUTOFILL_WALLET_OFFER:
      return "Autofill Wallet Offer";
    case AUTOFILL_WALLET_USAGE:
      return "Autofill Wallet Usage";
    case THEMES:
      return "Themes";
    case THEMES_IOS:
      return "Themes (iOS)";
    case EXTENSIONS:
      return "Extensions";
    case SEARCH_ENGINES:
      return "Search Engines";
    case SESSIONS:
      return "Sessions";
    case APPS:
      return "Apps";
    case APP_SETTINGS:
      return "App settings";
    case EXTENSION_SETTINGS:
      return "Extension settings";
    case HISTORY_DELETE_DIRECTIVES:
      return "History Delete Directives";
    case DICTIONARY:
      return "Dictionary";
    case DEVICE_INFO:
      return "Device Info";
    case PRIORITY_PREFERENCES:
      return "Priority Preferences";
    case SUPERVISED_USER_SETTINGS:
      return "Managed User Settings";
    case APP_LIST:
      return "App List";
    case ARC_PACKAGE:
      return "Arc Package";
    case PRINTERS:
      return "Printers";
    case READING_LIST:
      return "Reading List";
    case USER_EVENTS:
      return "User Events";
    case USER_CONSENTS:
      return "User Consents";
    case SEND_TAB_TO_SELF:
      return "Send Tab To Self";
    case SECURITY_EVENTS:
      return "Security Events";
    case WIFI_CONFIGURATIONS:
      return "Wifi Configurations";
    case WEB_APPS:
      return "Web Apps";
    case WEB_APKS:
      return "Web Apks";
    case OS_PREFERENCES:
      return "OS Preferences";
    case OS_PRIORITY_PREFERENCES:
      return "OS Priority Preferences";
    case SHARING_MESSAGE:
      return "Sharing Message";
    case WORKSPACE_DESK:
      return "Workspace Desk";
    case HISTORY:
      return "History";
    case PRINTERS_AUTHORIZATION_SERVERS:
      return "Printers Authorization Servers";
    case CONTACT_INFO:
      return "Contact Info";
    case SAVED_TAB_GROUP:
      return "Saved Tab Group";
    case WEBAUTHN_CREDENTIAL:
      return "WebAuthn Credentials";
    case INCOMING_PASSWORD_SHARING_INVITATION:
      return "Incoming Password Sharing Invitations";
    case OUTGOING_PASSWORD_SHARING_INVITATION:
      return "Outgoing Password Sharing Invitations";
    case SHARED_TAB_GROUP_DATA:
      return "Shared Tab Group Data";
    case COLLABORATION_GROUP:
      return "Collaboration Group";
    case PLUS_ADDRESS:
      return "Plus Address";
    case PRODUCT_COMPARISON:
      return "Product Comparison";
    case COOKIES:
      return "Cookies";
    case PLUS_ADDRESS_SETTING:
      return "Plus Address Setting";
    case AUTOFILL_VALUABLE:
      return "Autofill Valuable";
    case AUTOFILL_VALUABLE_METADATA:
      return "Autofill Valuable Metadata";
    case ACCOUNT_SETTING:
      return "Account Setting";
    case SHARED_TAB_GROUP_ACCOUNT_DATA:
      return "Shared Tab Group Account Data";
    case SHARED_COMMENT:
      return "SharedComment";
    case AI_THREAD:
      return "AI Thread";
    case CONTEXTUAL_TASK:
      return "Contextual Task";
    case NIGORI:
      return "Encryption Keys";
    case SKILL:
      return "Skill";
    case GEMINI_THREAD:
      return "Gemini Thread";
    case ACCESSIBILITY_ANNOTATION:
      return "Accessibility Annotation";
  }
  NOTREACHED();
}

const char* DataTypeToHistogramSuffix(DataType data_type) {
  // LINT.IfChange(DataTypeHistogramSuffix)
  switch (data_type) {
    case UNSPECIFIED:
      return "";
    case BOOKMARKS:
      return "BOOKMARK";
    case PREFERENCES:
      return "PREFERENCE";
    case PASSWORDS:
      return "PASSWORD";
    case AUTOFILL_PROFILE:
      return "AUTOFILL_PROFILE";
    case AUTOFILL:
      return "AUTOFILL";
    case AUTOFILL_WALLET_CREDENTIAL:
      return "AUTOFILL_WALLET_CREDENTIAL";
    case AUTOFILL_WALLET_DATA:
      return "AUTOFILL_WALLET";
    case AUTOFILL_WALLET_METADATA:
      return "WALLET_METADATA";
    case AUTOFILL_WALLET_OFFER:
      return "AUTOFILL_OFFER";
    case AUTOFILL_WALLET_USAGE:
      return "AUTOFILL_WALLET_USAGE";
    case THEMES:
      return "THEME";
    case THEMES_IOS:
      return "THEME_IOS";
    case EXTENSIONS:
      return "EXTENSION";
    case SEARCH_ENGINES:
      return "SEARCH_ENGINE";
    case SESSIONS:
      return "SESSION";
    case APPS:
      return "APP";
    case APP_SETTINGS:
      return "APP_SETTING";
    case EXTENSION_SETTINGS:
      return "EXTENSION_SETTING";
    case HISTORY_DELETE_DIRECTIVES:
      return "HISTORY_DELETE_DIRECTIVE";
    case DICTIONARY:
      return "DICTIONARY";
    case DEVICE_INFO:
      return "DEVICE_INFO";
    case PRIORITY_PREFERENCES:
      return "PRIORITY_PREFERENCE";
    case SUPERVISED_USER_SETTINGS:
      return "MANAGED_USER_SETTING";
    case APP_LIST:
      return "APP_LIST";
    case ARC_PACKAGE:
      return "ARC_PACKAGE";
    case PRINTERS:
      return "PRINTER";
    case READING_LIST:
      return "READING_LIST";
    case USER_EVENTS:
      return "USER_EVENT";
    case USER_CONSENTS:
      return "USER_CONSENT";
    case SEND_TAB_TO_SELF:
      return "SEND_TAB_TO_SELF";
    case SECURITY_EVENTS:
      return "SECURITY_EVENT";
    case WIFI_CONFIGURATIONS:
      return "WIFI_CONFIGURATION";
    case WEB_APPS:
      return "WEB_APP";
    case WEB_APKS:
      return "WEB_APK";
    case OS_PREFERENCES:
      return "OS_PREFERENCE";
    case OS_PRIORITY_PREFERENCES:
      return "OS_PRIORITY_PREFERENCE";
    case SHARING_MESSAGE:
      return "SHARING_MESSAGE";
    case WORKSPACE_DESK:
      return "WORKSPACE_DESK";
    case HISTORY:
      return "HISTORY";
    case PRINTERS_AUTHORIZATION_SERVERS:
      return "PRINTERS_AUTHORIZATION_SERVER";
    case CONTACT_INFO:
      return "CONTACT_INFO";
    case SAVED_TAB_GROUP:
      return "SAVED_TAB_GROUP";
    case WEBAUTHN_CREDENTIAL:
      return "WEBAUTHN_CREDENTIAL";
    case INCOMING_PASSWORD_SHARING_INVITATION:
      return "INCOMING_PASSWORD_SHARING_INVITATION";
    case OUTGOING_PASSWORD_SHARING_INVITATION:
      return "OUTGOING_PASSWORD_SHARING_INVITATION";
    case SHARED_TAB_GROUP_DATA:
      return "SHARED_TAB_GROUP_DATA";
    case COLLABORATION_GROUP:
      return "COLLABORATION_GROUP";
    case PLUS_ADDRESS:
      return "PLUS_ADDRESS";
    case PRODUCT_COMPARISON:
      return "PRODUCT_COMPARISON";
    case COOKIES:
      return "COOKIE";
    case PLUS_ADDRESS_SETTING:
      return "PLUS_ADDRESS_SETTING";
    case AUTOFILL_VALUABLE:
      return "AUTOFILL_VALUABLE";
    case AUTOFILL_VALUABLE_METADATA:
      return "AUTOFILL_VALUABLE_METADATA";
    case SHARED_TAB_GROUP_ACCOUNT_DATA:
      return "SHARED_TAB_GROUP_ACCOUNT_DATA";
    case SHARED_COMMENT:
      return "SHARED_COMMENT";
    case AI_THREAD:
      return "AI_THREAD";
    case CONTEXTUAL_TASK:
      return "CONTEXTUAL_TASK";
    case NIGORI:
      return "NIGORI";
    case ACCOUNT_SETTING:
      return "ACCOUNT_SETTING";
    case SKILL:
      return "SKILL";
    case GEMINI_THREAD:
      return "GEMINI_THREAD";
    case ACCESSIBILITY_ANNOTATION:
      return "ACCESSIBILITY_ANNOTATION";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/histograms.xml:DataTypeHistogramSuffix)
  NOTREACHED();
}

DataTypeForHistograms DataTypeHistogramValue(DataType data_type) {
  switch (data_type) {
    case UNSPECIFIED:
      return DataTypeForHistograms::kUnspecified;
    case BOOKMARKS:
      return DataTypeForHistograms::kBookmarks;
    case PREFERENCES:
      return DataTypeForHistograms::kPreferences;
    case PASSWORDS:
      return DataTypeForHistograms::kPasswords;
    case AUTOFILL_PROFILE:
      return DataTypeForHistograms::kAutofillProfile;
    case AUTOFILL:
      return DataTypeForHistograms::kAutofill;
    case AUTOFILL_WALLET_CREDENTIAL:
      return DataTypeForHistograms::kAutofillWalletCredential;
    case AUTOFILL_WALLET_DATA:
      return DataTypeForHistograms::kAutofillWalletData;
    case AUTOFILL_WALLET_METADATA:
      return DataTypeForHistograms::kAutofillWalletMetadata;
    case AUTOFILL_WALLET_OFFER:
      return DataTypeForHistograms::kAutofillWalletOffer;
    case AUTOFILL_WALLET_USAGE:
      return DataTypeForHistograms::kAutofillWalletUsage;
    case THEMES:
      return DataTypeForHistograms::kThemes;
    case THEMES_IOS:
      return DataTypeForHistograms::kThemesIos;
    case EXTENSIONS:
      return DataTypeForHistograms::kExtensions;
    case SEARCH_ENGINES:
      return DataTypeForHistograms::kSearchEngines;
    case SESSIONS:
      return DataTypeForHistograms::kSessions;
    case APPS:
      return DataTypeForHistograms::kApps;
    case APP_SETTINGS:
      return DataTypeForHistograms::kAppSettings;
    case EXTENSION_SETTINGS:
      return DataTypeForHistograms::kExtensionSettings;
    case HISTORY_DELETE_DIRECTIVES:
      return DataTypeForHistograms::kHistoryDeleteDirectices;
    case DICTIONARY:
      return DataTypeForHistograms::kDictionary;
    case DEVICE_INFO:
      return DataTypeForHistograms::kDeviceInfo;
    case PRIORITY_PREFERENCES:
      return DataTypeForHistograms::kPriorityPreferences;
    case SUPERVISED_USER_SETTINGS:
      return DataTypeForHistograms::kSupervisedUserSettings;
    case APP_LIST:
      return DataTypeForHistograms::kAppList;
    case ARC_PACKAGE:
      return DataTypeForHistograms::kArcPackage;
    case PRINTERS:
      return DataTypeForHistograms::kPrinters;
    case READING_LIST:
      return DataTypeForHistograms::kReadingList;
    case USER_EVENTS:
      return DataTypeForHistograms::kUserEvents;
    case USER_CONSENTS:
      return DataTypeForHistograms::kUserConsents;
    case SEND_TAB_TO_SELF:
      return DataTypeForHistograms::kSendTabToSelf;
    case SECURITY_EVENTS:
      return DataTypeForHistograms::kSecurityEvents;
    case WIFI_CONFIGURATIONS:
      return DataTypeForHistograms::kWifiConfigurations;
    case WEB_APPS:
      return DataTypeForHistograms::kWebApps;
    case WEB_APKS:
      return DataTypeForHistograms::kWebApks;
    case OS_PREFERENCES:
      return DataTypeForHistograms::kOsPreferences;
    case OS_PRIORITY_PREFERENCES:
      return DataTypeForHistograms::kOsPriorityPreferences;
    case SHARING_MESSAGE:
      return DataTypeForHistograms::kSharingMessage;
    case WORKSPACE_DESK:
      return DataTypeForHistograms::kWorkspaceDesk;
    case HISTORY:
      return DataTypeForHistograms::kHistory;
    case PRINTERS_AUTHORIZATION_SERVERS:
      return DataTypeForHistograms::kPrintersAuthorizationServers;
    case CONTACT_INFO:
      return DataTypeForHistograms::kContactInfo;
    case SAVED_TAB_GROUP:
      return DataTypeForHistograms::kSavedTabGroups;
    case WEBAUTHN_CREDENTIAL:
      return DataTypeForHistograms::kWebAuthnCredentials;
    case INCOMING_PASSWORD_SHARING_INVITATION:
      return DataTypeForHistograms::kIncomingPasswordSharingInvitations;
    case OUTGOING_PASSWORD_SHARING_INVITATION:
      return DataTypeForHistograms::kOutgoingPasswordSharingInvitations;
    case SHARED_TAB_GROUP_DATA:
      return DataTypeForHistograms::kSharedTabGroupData;
    case COLLABORATION_GROUP:
      return DataTypeForHistograms::kCollaborationGroup;
    case PLUS_ADDRESS:
      return DataTypeForHistograms::kPlusAddresses;
    case PRODUCT_COMPARISON:
      return DataTypeForHistograms::kProductComparison;
    case COOKIES:
      return DataTypeForHistograms::kCookies;
    case PLUS_ADDRESS_SETTING:
      return DataTypeForHistograms::kPlusAddressSettings;
    case AUTOFILL_VALUABLE:
      return DataTypeForHistograms::kAutofillValuable;
    case AUTOFILL_VALUABLE_METADATA:
      return DataTypeForHistograms::kAutofillValuableMetadata;
    case ACCOUNT_SETTING:
      return DataTypeForHistograms::kAccountSetting;
    case SHARED_TAB_GROUP_ACCOUNT_DATA:
      return DataTypeForHistograms::kSharedTabGroupAccountData;
    case SHARED_COMMENT:
      return DataTypeForHistograms::kSharedComment;
    case AI_THREAD:
      return DataTypeForHistograms::kAIThread;
    case CONTEXTUAL_TASK:
      return DataTypeForHistograms::kContextualTask;
    case NIGORI:
      return DataTypeForHistograms::kNigori;
    case SKILL:
      return DataTypeForHistograms::kSkill;
    case GEMINI_THREAD:
      return DataTypeForHistograms::kGeminiThread;
    case ACCESSIBILITY_ANNOTATION:
      return DataTypeForHistograms::kAccessibilityAnnotation;
  }
  NOTREACHED();
}

int DataTypeToStableIdentifier(DataType data_type) {
  // Make sure the value is stable and positive.
  return static_cast<int>(DataTypeHistogramValue(data_type)) + 1;
}

std::string DataTypeSetToDebugString(DataTypeSet data_types) {
  std::string result;
  for (DataType type : data_types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += DataTypeToDebugString(type);
  }
  return result;
}

const char* DataTypeToStableLowerCaseString(DataType data_type) {
  // WARNING: existing strings must not be changed without migration, they are
  // persisted!
  switch (data_type) {
    case UNSPECIFIED:
      return "";
    case BOOKMARKS:
      return "bookmarks";
    case PREFERENCES:
      return "preferences";
    case PASSWORDS:
      return "passwords";
    case AUTOFILL_PROFILE:
      return "autofill_profiles";
    case AUTOFILL:
      return "autofill";
    case AUTOFILL_WALLET_CREDENTIAL:
      return "autofill_wallet_credential";
    case AUTOFILL_WALLET_DATA:
      return "autofill_wallet";
    case AUTOFILL_WALLET_METADATA:
      return "autofill_wallet_metadata";
    case AUTOFILL_WALLET_OFFER:
      return "autofill_wallet_offer";
    case AUTOFILL_WALLET_USAGE:
      return "autofill_wallet_usage";
    case THEMES:
      return "themes";
    case THEMES_IOS:
      return "themes_ios";
    case EXTENSIONS:
      return "extensions";
    case SEARCH_ENGINES:
      return "search_engines";
    case SESSIONS:
      return "sessions";
    case APPS:
      return "apps";
    case APP_SETTINGS:
      return "app_settings";
    case EXTENSION_SETTINGS:
      return "extension_settings";
    case HISTORY_DELETE_DIRECTIVES:
      return "history_delete_directives";
    case DICTIONARY:
      return "dictionary";
    case DEVICE_INFO:
      return "device_info";
    case PRIORITY_PREFERENCES:
      return "priority_preferences";
    case SUPERVISED_USER_SETTINGS:
      return "managed_user_settings";
    case APP_LIST:
      return "app_list";
    case ARC_PACKAGE:
      return "arc_package";
    case PRINTERS:
      return "printers";
    case READING_LIST:
      return "reading_list";
    case USER_EVENTS:
      return "user_events";
    case USER_CONSENTS:
      return "user_consent";
    case SEND_TAB_TO_SELF:
      return "send_tab_to_self";
    case SECURITY_EVENTS:
      return "security_events";
    case WIFI_CONFIGURATIONS:
      return "wifi_configurations";
    case WEB_APPS:
      return "web_apps";
    case WEB_APKS:
      return "webapks";
    case OS_PREFERENCES:
      return "os_preferences";
    case OS_PRIORITY_PREFERENCES:
      return "os_priority_preferences";
    case SHARING_MESSAGE:
      return "sharing_message";
    case WORKSPACE_DESK:
      return "workspace_desk";
    case HISTORY:
      return "history";
    case PRINTERS_AUTHORIZATION_SERVERS:
      return "printers_authorization_servers";
    case CONTACT_INFO:
      return "contact_info";
    case SAVED_TAB_GROUP:
      return "saved_tab_group";
    case WEBAUTHN_CREDENTIAL:
      return "webauthn_credential";
    case INCOMING_PASSWORD_SHARING_INVITATION:
      return "incoming_password_sharing_invitation";
    case OUTGOING_PASSWORD_SHARING_INVITATION:
      return "outgoing_password_sharing_invitation";
    case SHARED_TAB_GROUP_DATA:
      return "shared_tab_group_data";
    case COLLABORATION_GROUP:
      return "collaboration_group";
    case PLUS_ADDRESS:
      return "plus_address";
    case PRODUCT_COMPARISON:
      return "product_comparison";
    case COOKIES:
      return "cookies";
    case PLUS_ADDRESS_SETTING:
      return "plus_address_setting";
    case AUTOFILL_VALUABLE:
      return "autofill_valuable";
    case AUTOFILL_VALUABLE_METADATA:
      return "autofill_valuable_metadata";
    case ACCOUNT_SETTING:
      return "account_setting";
    case SHARED_TAB_GROUP_ACCOUNT_DATA:
      return "shared_tab_group_account_data";
    case SHARED_COMMENT:
      return "shared_comment";
    case AI_THREAD:
      return "ai_thread";
    case CONTEXTUAL_TASK:
      return "contextual_task";
    case NIGORI:
      return "nigori";
    case SKILL:
      return "skill";
    case GEMINI_THREAD:
      return "gemini_thread";
    case ACCESSIBILITY_ANNOTATION:
      return "accessibility_annotation";
  }
  // WARNING: existing strings must not be changed without migration, they
  // are persisted!
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& out, DataType data_type) {
  return out << DataTypeToDebugString(data_type);
}

std::ostream& operator<<(std::ostream& out, DataTypeSet data_type_set) {
  return out << DataTypeSetToDebugString(data_type_set);
}

std::string DataTypeToProtocolRootTag(DataType data_type) {
  DCHECK(ProtocolTypes().Has(data_type));
  DCHECK(IsRealDataType(data_type));
  const std::string root_tag = DataTypeToStableLowerCaseString(data_type);
  DCHECK(!root_tag.empty());
  return "google_chrome_" + root_tag;
}

bool IsRealDataType(DataType data_type) {
  return data_type >= FIRST_REAL_DATA_TYPE && data_type <= LAST_REAL_DATA_TYPE;
}

bool IsActOnceDataType(DataType data_type) {
  return data_type == HISTORY_DELETE_DIRECTIVES;
}

}  // namespace syncer
