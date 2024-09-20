// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type.h"

#include <ostream>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

namespace {

static_assert(53 == syncer::GetNumDataTypes(),
              "When adding a new type, update enum SyncDataTypes in enums.xml "
              "and suffix SyncDataType in histograms.xml.");

static_assert(53 == syncer::GetNumDataTypes(),
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
        {sync_pb::EntitySpecifics::kPowerBookmarkFieldNumber, POWER_BOOKMARK},
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
        // ---- Control Types ----
        {sync_pb::EntitySpecifics::kNigoriFieldNumber, NIGORI},
    });

}  // namespace

void AddDefaultFieldValue(DataType type, sync_pb::EntitySpecifics* specifics) {
  switch (type) {
    case UNSPECIFIED:
      NOTREACHED_IN_MIGRATION()
          << "No default field value for " << DataTypeToDebugString(type);
      break;
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
    case POWER_BOOKMARK:
      specifics->mutable_power_bookmark();
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
    case POWER_BOOKMARK:
      return sync_pb::EntitySpecifics::kPowerBookmarkFieldNumber;
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
    case NIGORI:
      return sync_pb::EntitySpecifics::kNigoriFieldNumber;
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
  static_assert(53 == syncer::GetNumDataTypes(),
                "When adding new protocol types, the following type lookup "
                "logic must be updated.");
  if (specifics.has_bookmark()) {
    return BOOKMARKS;
  }
  if (specifics.has_preference()) {
    return PREFERENCES;
  }
  if (specifics.has_password()) {
    return PASSWORDS;
  }
  if (specifics.has_autofill_profile()) {
    return AUTOFILL_PROFILE;
  }
  if (specifics.has_autofill()) {
    return AUTOFILL;
  }
  if (specifics.has_autofill_wallet()) {
    return AUTOFILL_WALLET_DATA;
  }
  if (specifics.has_wallet_metadata()) {
    return AUTOFILL_WALLET_METADATA;
  }
  if (specifics.has_theme()) {
    return THEMES;
  }
  if (specifics.has_extension()) {
    return EXTENSIONS;
  }
  if (specifics.has_search_engine()) {
    return SEARCH_ENGINES;
  }
  if (specifics.has_session()) {
    return SESSIONS;
  }
  if (specifics.has_app()) {
    return APPS;
  }
  if (specifics.has_app_setting()) {
    return APP_SETTINGS;
  }
  if (specifics.has_extension_setting()) {
    return EXTENSION_SETTINGS;
  }
  if (specifics.has_history_delete_directive()) {
    return HISTORY_DELETE_DIRECTIVES;
  }
  if (specifics.has_dictionary()) {
    return DICTIONARY;
  }
  if (specifics.has_device_info()) {
    return DEVICE_INFO;
  }
  if (specifics.has_priority_preference()) {
    return PRIORITY_PREFERENCES;
  }
  if (specifics.has_managed_user_setting()) {
    return SUPERVISED_USER_SETTINGS;
  }
  if (specifics.has_app_list()) {
    return APP_LIST;
  }
  if (specifics.has_arc_package()) {
    return ARC_PACKAGE;
  }
  if (specifics.has_printer()) {
    return PRINTERS;
  }
  if (specifics.has_reading_list()) {
    return READING_LIST;
  }
  if (specifics.has_user_event()) {
    return USER_EVENTS;
  }
  if (specifics.has_user_consent()) {
    return USER_CONSENTS;
  }
  if (specifics.has_nigori()) {
    return NIGORI;
  }
  if (specifics.has_send_tab_to_self()) {
    return SEND_TAB_TO_SELF;
  }
  if (specifics.has_security_event()) {
    return SECURITY_EVENTS;
  }
  if (specifics.has_web_app()) {
    return WEB_APPS;
  }
  if (specifics.has_web_apk()) {
    return WEB_APKS;
  }
  if (specifics.has_wifi_configuration()) {
    return WIFI_CONFIGURATIONS;
  }
  if (specifics.has_os_preference()) {
    return OS_PREFERENCES;
  }
  if (specifics.has_os_priority_preference()) {
    return OS_PRIORITY_PREFERENCES;
  }
  if (specifics.has_sharing_message()) {
    return SHARING_MESSAGE;
  }
  if (specifics.has_autofill_offer()) {
    return AUTOFILL_WALLET_OFFER;
  }
  if (specifics.has_workspace_desk()) {
    return WORKSPACE_DESK;
  }
  if (specifics.has_history()) {
    return HISTORY;
  }
  if (specifics.has_printers_authorization_server()) {
    return PRINTERS_AUTHORIZATION_SERVERS;
  }
  if (specifics.has_contact_info()) {
    return CONTACT_INFO;
  }
  if (specifics.has_autofill_wallet_usage()) {
    return AUTOFILL_WALLET_USAGE;
  }
  if (specifics.has_saved_tab_group()) {
    return SAVED_TAB_GROUP;
  }
  if (specifics.has_power_bookmark()) {
    return POWER_BOOKMARK;
  }
  if (specifics.has_webauthn_credential()) {
    return WEBAUTHN_CREDENTIAL;
  }
  if (specifics.has_incoming_password_sharing_invitation()) {
    return INCOMING_PASSWORD_SHARING_INVITATION;
  }
  if (specifics.has_outgoing_password_sharing_invitation()) {
    return OUTGOING_PASSWORD_SHARING_INVITATION;
  }
  if (specifics.has_autofill_wallet_credential()) {
    return AUTOFILL_WALLET_CREDENTIAL;
  }
  if (specifics.has_shared_tab_group_data()) {
    return SHARED_TAB_GROUP_DATA;
  }
  if (specifics.has_collaboration_group()) {
    return COLLABORATION_GROUP;
  }
  if (specifics.has_plus_address()) {
    return PLUS_ADDRESS;
  }
  if (specifics.has_product_comparison()) {
    return PRODUCT_COMPARISON;
  }
  if (specifics.has_cookie()) {
    return COOKIES;
  }
  if (specifics.has_plus_address_setting()) {
    return PLUS_ADDRESS_SETTING;
  }

  // This client version doesn't understand |specifics|.
  DVLOG(1) << "Unknown datatype in sync proto.";
  return UNSPECIFIED;
}

DataTypeSet EncryptableUserTypes() {
  static_assert(53 == syncer::GetNumDataTypes(),
                "If adding an unencryptable type, remove from "
                "encryptable_user_types below.");
  DataTypeSet encryptable_user_types = UserTypes();
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
  // Password sharing invitations have different encryption implementation.
  encryptable_user_types.Remove(INCOMING_PASSWORD_SHARING_INVITATION);
  encryptable_user_types.Remove(OUTGOING_PASSWORD_SHARING_INVITATION);
  // Never encrypted because consumed server-side.
  encryptable_user_types.Remove(SHARED_TAB_GROUP_DATA);
  // Plus addresses and their settings are never encrypted because they
  // originate from outside Chrome.
  encryptable_user_types.Remove(PLUS_ADDRESS);
  encryptable_user_types.Remove(PLUS_ADDRESS_SETTING);

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
    case POWER_BOOKMARK:
      return "Power Bookmark";
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
    case NIGORI:
      return "Encryption Keys";
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
    case POWER_BOOKMARK:
      return "POWER_BOOKMARK";
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
    case NIGORI:
      return "NIGORI";
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
    case POWER_BOOKMARK:
      return DataTypeForHistograms::kPowerBookmark;
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
    case NIGORI:
      return DataTypeForHistograms::kNigori;
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
    case POWER_BOOKMARK:
      return "power_bookmark";
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
    case NIGORI:
      return "nigori";
  }
  // WARNING: existing strings must not be changed without migration, they
  // are persisted!
  NOTREACHED();
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
