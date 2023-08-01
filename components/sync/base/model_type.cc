// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/model_type.h"

#include <stddef.h>

#include <ostream>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

namespace {

struct ModelTypeInfo {
  const ModelType model_type;
  // Used to identify the model type in the SyncModelType histogram_suffix in
  // histograms.xml. Must always be kept in sync.
  const char* const histogram_suffix;
  // Root tag for Model Type
  // This should be the same as the model type but all lowercase.
  const char* const lowercase_root_tag;
  // String value for Model Type
  // This should be the same as the model type but space separated and the
  // first letter of every word capitalized.
  const char* const model_type_debug_string;
  // Field number of the model type specifics in EntitySpecifics.
  const int specifics_field_number;
  // Model type value from SyncModelTypes enum in enums.xml. Must always be in
  // sync with the enum.
  const ModelTypeForHistograms model_type_histogram_val;
};

// Below struct entries are in the same order as their definition in the
// ModelType enum. When making changes to this list, don't forget to
//  - update kSpecificsFieldNumberToModelTypeMap below,
//  - update the ModelType enum,
//  - update the SyncModelTypes enum in enums.xml, and
//  - update the SyncModelType histogram suffix in histograms.xml.
// Struct field values should be unique across the entire map.
const ModelTypeInfo kModelTypeInfoMap[] = {
    {UNSPECIFIED, "", "", "Unspecified", -1,
     ModelTypeForHistograms::kUnspecified},
    {BOOKMARKS, "BOOKMARK", "bookmarks", "Bookmarks",
     sync_pb::EntitySpecifics::kBookmarkFieldNumber,
     ModelTypeForHistograms::kBookmarks},
    {PREFERENCES, "PREFERENCE", "preferences", "Preferences",
     sync_pb::EntitySpecifics::kPreferenceFieldNumber,
     ModelTypeForHistograms::kPreferences},
    {PASSWORDS, "PASSWORD", "passwords", "Passwords",
     sync_pb::EntitySpecifics::kPasswordFieldNumber,
     ModelTypeForHistograms::kPasswords},
    {AUTOFILL_PROFILE, "AUTOFILL_PROFILE", "autofill_profiles",
     "Autofill Profiles", sync_pb::EntitySpecifics::kAutofillProfileFieldNumber,
     ModelTypeForHistograms::kAutofillProfile},
    {AUTOFILL, "AUTOFILL", "autofill", "Autofill",
     sync_pb::EntitySpecifics::kAutofillFieldNumber,
     ModelTypeForHistograms::kAutofill},
    {AUTOFILL_WALLET_CREDENTIAL, "AUTOFILL_WALLET_CREDENTIAL",
     "autofill_wallet_credential", "Autofill Wallet Credential",
     sync_pb::EntitySpecifics::kAutofillWalletCredentialFieldNumber,
     ModelTypeForHistograms::kAutofillWalletCredential},
    {AUTOFILL_WALLET_DATA, "AUTOFILL_WALLET", "autofill_wallet",
     "Autofill Wallet", sync_pb::EntitySpecifics::kAutofillWalletFieldNumber,
     ModelTypeForHistograms::kAutofillWalletData},
    {AUTOFILL_WALLET_METADATA, "WALLET_METADATA", "autofill_wallet_metadata",
     "Autofill Wallet Metadata",
     sync_pb::EntitySpecifics::kWalletMetadataFieldNumber,
     ModelTypeForHistograms::kAutofillWalletMetadata},
    {AUTOFILL_WALLET_OFFER, "AUTOFILL_OFFER", "autofill_wallet_offer",
     "Autofill Wallet Offer",
     sync_pb::EntitySpecifics::kAutofillOfferFieldNumber,
     ModelTypeForHistograms::kAutofillWalletOffer},
    {AUTOFILL_WALLET_USAGE, "AUTOFILL_WALLET_USAGE", "autofill_wallet_usage",
     "Autofill Wallet Usage",
     sync_pb::EntitySpecifics::kAutofillWalletUsageFieldNumber,
     ModelTypeForHistograms::kAutofillWalletUsage},
    {THEMES, "THEME", "themes", "Themes",
     sync_pb::EntitySpecifics::kThemeFieldNumber,
     ModelTypeForHistograms::kThemes},
    {TYPED_URLS, "TYPED_URL", "typed_urls", "Typed URLs",
     sync_pb::EntitySpecifics::kTypedUrlFieldNumber,
     ModelTypeForHistograms::kTypedUrls},
    {EXTENSIONS, "EXTENSION", "extensions", "Extensions",
     sync_pb::EntitySpecifics::kExtensionFieldNumber,
     ModelTypeForHistograms::kExtensions},
    {SEARCH_ENGINES, "SEARCH_ENGINE", "search_engines", "Search Engines",
     sync_pb::EntitySpecifics::kSearchEngineFieldNumber,
     ModelTypeForHistograms::kSearchEngines},
    {SESSIONS, "SESSION", "sessions", "Sessions",
     sync_pb::EntitySpecifics::kSessionFieldNumber,
     ModelTypeForHistograms::kSessions},
    {APPS, "APP", "apps", "Apps", sync_pb::EntitySpecifics::kAppFieldNumber,
     ModelTypeForHistograms::kApps},
    {APP_SETTINGS, "APP_SETTING", "app_settings", "App settings",
     sync_pb::EntitySpecifics::kAppSettingFieldNumber,
     ModelTypeForHistograms::kAppSettings},
    {EXTENSION_SETTINGS, "EXTENSION_SETTING", "extension_settings",
     "Extension settings",
     sync_pb::EntitySpecifics::kExtensionSettingFieldNumber,
     ModelTypeForHistograms::kExtensionSettings},
    {HISTORY_DELETE_DIRECTIVES, "HISTORY_DELETE_DIRECTIVE",
     "history_delete_directives", "History Delete Directives",
     sync_pb::EntitySpecifics::kHistoryDeleteDirectiveFieldNumber,
     ModelTypeForHistograms::kHistoryDeleteDirectices},
    {DICTIONARY, "DICTIONARY", "dictionary", "Dictionary",
     sync_pb::EntitySpecifics::kDictionaryFieldNumber,
     ModelTypeForHistograms::kDictionary},
    {DEVICE_INFO, "DEVICE_INFO", "device_info", "Device Info",
     sync_pb::EntitySpecifics::kDeviceInfoFieldNumber,
     ModelTypeForHistograms::kDeviceInfo},
    {PRIORITY_PREFERENCES, "PRIORITY_PREFERENCE", "priority_preferences",
     "Priority Preferences",
     sync_pb::EntitySpecifics::kPriorityPreferenceFieldNumber,
     ModelTypeForHistograms::kPriorityPreferences},
    {SUPERVISED_USER_SETTINGS, "MANAGED_USER_SETTING", "managed_user_settings",
     "Managed User Settings",
     sync_pb::EntitySpecifics::kManagedUserSettingFieldNumber,
     ModelTypeForHistograms::kSupervisedUserSettings},
    {APP_LIST, "APP_LIST", "app_list", "App List",
     sync_pb::EntitySpecifics::kAppListFieldNumber,
     ModelTypeForHistograms::kAppList},
    {ARC_PACKAGE, "ARC_PACKAGE", "arc_package", "Arc Package",
     sync_pb::EntitySpecifics::kArcPackageFieldNumber,
     ModelTypeForHistograms::kArcPackage},
    {PRINTERS, "PRINTER", "printers", "Printers",
     sync_pb::EntitySpecifics::kPrinterFieldNumber,
     ModelTypeForHistograms::kPrinters},
    {READING_LIST, "READING_LIST", "reading_list", "Reading List",
     sync_pb::EntitySpecifics::kReadingListFieldNumber,
     ModelTypeForHistograms::kReadingList},
    {USER_EVENTS, "USER_EVENT", "user_events", "User Events",
     sync_pb::EntitySpecifics::kUserEventFieldNumber,
     ModelTypeForHistograms::kUserEvents},
    {USER_CONSENTS, "USER_CONSENT", "user_consent", "User Consents",
     sync_pb::EntitySpecifics::kUserConsentFieldNumber,
     ModelTypeForHistograms::kUserConsents},
    {SEGMENTATION, "SEGMENTATION", "segmentation", "Segmentation",
     sync_pb::EntitySpecifics::kSegmentationFieldNumber,
     ModelTypeForHistograms::kSegmentation},
    {SEND_TAB_TO_SELF, "SEND_TAB_TO_SELF", "send_tab_to_self",
     "Send Tab To Self", sync_pb::EntitySpecifics::kSendTabToSelfFieldNumber,
     ModelTypeForHistograms::kSendTabToSelf},
    {SECURITY_EVENTS, "SECURITY_EVENT", "security_events", "Security Events",
     sync_pb::EntitySpecifics::kSecurityEventFieldNumber,
     ModelTypeForHistograms::kSecurityEvents},
    {WIFI_CONFIGURATIONS, "WIFI_CONFIGURATION", "wifi_configurations",
     "Wifi Configurations",
     sync_pb::EntitySpecifics::kWifiConfigurationFieldNumber,
     ModelTypeForHistograms::kWifiConfigurations},
    {WEB_APPS, "WEB_APP", "web_apps", "Web Apps",
     sync_pb::EntitySpecifics::kWebAppFieldNumber,
     ModelTypeForHistograms::kWebApps},
    {OS_PREFERENCES, "OS_PREFERENCE", "os_preferences", "OS Preferences",
     sync_pb::EntitySpecifics::kOsPreferenceFieldNumber,
     ModelTypeForHistograms::kOsPreferences},
    {OS_PRIORITY_PREFERENCES, "OS_PRIORITY_PREFERENCE",
     "os_priority_preferences", "OS Priority Preferences",
     sync_pb::EntitySpecifics::kOsPriorityPreferenceFieldNumber,
     ModelTypeForHistograms::kOsPriorityPreferences},
    {SHARING_MESSAGE, "SHARING_MESSAGE", "sharing_message", "Sharing Message",
     sync_pb::EntitySpecifics::kSharingMessageFieldNumber,
     ModelTypeForHistograms::kSharingMessage},
    {WORKSPACE_DESK, "WORKSPACE_DESK", "workspace_desk", "Workspace Desk",
     sync_pb::EntitySpecifics::kWorkspaceDeskFieldNumber,
     ModelTypeForHistograms::kWorkspaceDesk},
    {HISTORY, "HISTORY", "history", "History",
     sync_pb::EntitySpecifics::kHistoryFieldNumber,
     ModelTypeForHistograms::kHistory},
    {PRINTERS_AUTHORIZATION_SERVERS, "PRINTERS_AUTHORIZATION_SERVER",
     "printers_authorization_servers", "Printers Authorization Servers",
     sync_pb::EntitySpecifics::kPrintersAuthorizationServerFieldNumber,
     ModelTypeForHistograms::kPrintersAuthorizationServers},
    {CONTACT_INFO, "CONTACT_INFO", "contact_info", "Contact Info",
     sync_pb::EntitySpecifics::kContactInfoFieldNumber,
     ModelTypeForHistograms::kContactInfo},
    {SAVED_TAB_GROUP, "SAVED_TAB_GROUP", "saved_tab_group", "Saved Tab Group",
     sync_pb::EntitySpecifics::kSavedTabGroupFieldNumber,
     ModelTypeForHistograms::kSavedTabGroups},
    {POWER_BOOKMARK, "POWER_BOOKMARK", "power_bookmark", "Power Bookmark",
     sync_pb::EntitySpecifics::kPowerBookmarkFieldNumber,
     ModelTypeForHistograms::kPowerBookmark},
    {WEBAUTHN_CREDENTIAL, "WEBAUTHN_CREDENTIAL", "webauthn_credential",
     "WebAuthn Credentials",
     sync_pb::EntitySpecifics::kWebauthnCredentialFieldNumber,
     ModelTypeForHistograms::kWebAuthnCredentials},
    {INCOMING_PASSWORD_SHARING_INVITATION,
     "INCOMING_PASSWORD_SHARING_INVITATION",
     "incoming_password_sharing_invitation",
     "Incoming Password Sharing Invitations",
     sync_pb::EntitySpecifics::kIncomingPasswordSharingInvitationFieldNumber,
     ModelTypeForHistograms::kIncomingPasswordSharingInvitations},
    {OUTGOING_PASSWORD_SHARING_INVITATION,
     "OUTGOING_PASSWORD_SHARING_INVITATION",
     "outgoing_password_sharing_invitation",
     "Outgoing Password Sharing Invitations",
     sync_pb::EntitySpecifics::kOutgoingPasswordSharingInvitationFieldNumber,
     ModelTypeForHistograms::kOutgoingPasswordSharingInvitations},
    // ---- Proxy types ----
    {PROXY_TABS, "", "", "Proxy tabs", -1, ModelTypeForHistograms::kProxyTabs},
    // ---- Control Types ----
    {NIGORI, "NIGORI", "nigori", "Encryption Keys",
     sync_pb::EntitySpecifics::kNigoriFieldNumber,
     ModelTypeForHistograms::kNigori},
};

static_assert(std::size(kModelTypeInfoMap) == GetNumModelTypes(),
              "kModelTypeInfoMap should have GetNumModelTypes() elements");

static_assert(49 == syncer::GetNumModelTypes(),
              "When adding a new type, update enum SyncModelTypes in enums.xml "
              "and suffix SyncModelType in histograms.xml.");

// kSpecificsFieldNumberToModelTypeMap must exactly match the kModelTypeInfoMap,
// but PROXY_TABS does not have a ModelType. So we skipped it and thus expect
// size to be (syncer::GetNumModelTypes()-1).
//
// NOTE: size here acts as a static assert on the constraint above.
using kSpecificsFieldNumberToModelTypeMap =
    base::fixed_flat_map<int, ModelType, syncer::GetNumModelTypes() - 1>;

constexpr kSpecificsFieldNumberToModelTypeMap
    specifics_field_number2model_type = base::MakeFixedFlatMap<int, ModelType>({
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
        {sync_pb::EntitySpecifics::kTypedUrlFieldNumber, TYPED_URLS},
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
        {sync_pb::EntitySpecifics::kSegmentationFieldNumber, SEGMENTATION},
        {sync_pb::EntitySpecifics::kSendTabToSelfFieldNumber, SEND_TAB_TO_SELF},
        {sync_pb::EntitySpecifics::kSecurityEventFieldNumber, SECURITY_EVENTS},
        {sync_pb::EntitySpecifics::kWifiConfigurationFieldNumber,
         WIFI_CONFIGURATIONS},
        {sync_pb::EntitySpecifics::kWebAppFieldNumber, WEB_APPS},
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
        // ---- Control Types ----
        {sync_pb::EntitySpecifics::kNigoriFieldNumber, NIGORI},
    });

}  // namespace

void AddDefaultFieldValue(ModelType type, sync_pb::EntitySpecifics* specifics) {
  switch (type) {
    case UNSPECIFIED:
      NOTREACHED() << "No default field value for "
                   << ModelTypeToDebugString(type);
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
    case TYPED_URLS:
      specifics->mutable_typed_url();
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
    case PROXY_TABS:
      NOTREACHED() << "No default field value for "
                   << ModelTypeToDebugString(type);
      break;
    case NIGORI:
      specifics->mutable_nigori();
      break;
    case WEB_APPS:
      specifics->mutable_web_app();
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
    case SEGMENTATION:
      specifics->mutable_segmentation();
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
  }
}

ModelType GetModelTypeFromSpecificsFieldNumber(int field_number) {
  kSpecificsFieldNumberToModelTypeMap::const_iterator it =
      specifics_field_number2model_type.find(field_number);
  return (it == specifics_field_number2model_type.end() ? UNSPECIFIED
                                                        : it->second);
}

int GetSpecificsFieldNumberFromModelType(ModelType model_type) {
  DCHECK(ProtocolTypes().Has(model_type))
      << "Only protocol types have field values.";
  return kModelTypeInfoMap[model_type].specifics_field_number;
}

void internal::GetModelTypeSetFromSpecificsFieldNumberListHelper(
    ModelTypeSet& model_types,
    int field_number) {
  ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
  if (IsRealDataType(model_type)) {
    model_types.Put(model_type);
  } else {
    DLOG(WARNING) << "Unknown field number " << field_number;
  }
}

ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics) {
  static_assert(49 == syncer::GetNumModelTypes(),
                "When adding new protocol types, the following type lookup "
                "logic must be updated.");
  if (specifics.has_bookmark())
    return BOOKMARKS;
  if (specifics.has_preference())
    return PREFERENCES;
  if (specifics.has_password())
    return PASSWORDS;
  if (specifics.has_autofill_profile())
    return AUTOFILL_PROFILE;
  if (specifics.has_autofill())
    return AUTOFILL;
  if (specifics.has_autofill_wallet())
    return AUTOFILL_WALLET_DATA;
  if (specifics.has_wallet_metadata())
    return AUTOFILL_WALLET_METADATA;
  if (specifics.has_theme())
    return THEMES;
  if (specifics.has_typed_url())
    return TYPED_URLS;
  if (specifics.has_extension())
    return EXTENSIONS;
  if (specifics.has_search_engine())
    return SEARCH_ENGINES;
  if (specifics.has_session())
    return SESSIONS;
  if (specifics.has_app())
    return APPS;
  if (specifics.has_app_setting())
    return APP_SETTINGS;
  if (specifics.has_extension_setting())
    return EXTENSION_SETTINGS;
  if (specifics.has_history_delete_directive())
    return HISTORY_DELETE_DIRECTIVES;
  if (specifics.has_dictionary())
    return DICTIONARY;
  if (specifics.has_device_info())
    return DEVICE_INFO;
  if (specifics.has_priority_preference())
    return PRIORITY_PREFERENCES;
  if (specifics.has_managed_user_setting())
    return SUPERVISED_USER_SETTINGS;
  if (specifics.has_app_list())
    return APP_LIST;
  if (specifics.has_arc_package())
    return ARC_PACKAGE;
  if (specifics.has_printer())
    return PRINTERS;
  if (specifics.has_reading_list())
    return READING_LIST;
  if (specifics.has_user_event())
    return USER_EVENTS;
  if (specifics.has_user_consent())
    return USER_CONSENTS;
  if (specifics.has_nigori())
    return NIGORI;
  if (specifics.has_send_tab_to_self())
    return SEND_TAB_TO_SELF;
  if (specifics.has_security_event())
    return SECURITY_EVENTS;
  if (specifics.has_web_app())
    return WEB_APPS;
  if (specifics.has_wifi_configuration())
    return WIFI_CONFIGURATIONS;
  if (specifics.has_os_preference())
    return OS_PREFERENCES;
  if (specifics.has_os_priority_preference())
    return OS_PRIORITY_PREFERENCES;
  if (specifics.has_sharing_message())
    return SHARING_MESSAGE;
  if (specifics.has_autofill_offer())
    return AUTOFILL_WALLET_OFFER;
  if (specifics.has_workspace_desk())
    return WORKSPACE_DESK;
  if (specifics.has_history())
    return HISTORY;
  if (specifics.has_printers_authorization_server())
    return PRINTERS_AUTHORIZATION_SERVERS;
  if (specifics.has_contact_info())
    return CONTACT_INFO;
  if (specifics.has_autofill_wallet_usage())
    return AUTOFILL_WALLET_USAGE;
  if (specifics.has_segmentation())
    return SEGMENTATION;
  if (specifics.has_saved_tab_group())
    return SAVED_TAB_GROUP;
  if (specifics.has_power_bookmark())
    return POWER_BOOKMARK;
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

  // This client version doesn't understand |specifics|.
  DVLOG(1) << "Unknown datatype in sync proto.";
  return UNSPECIFIED;
}

ModelTypeSet EncryptableUserTypes() {
  static_assert(49 == syncer::GetNumModelTypes(),
                "If adding an unencryptable type, remove from "
                "encryptable_user_types below.");
  ModelTypeSet encryptable_user_types = UserTypes();
  // Wallet data is not encrypted since it actually originates on the server.
  encryptable_user_types.Remove(AUTOFILL_WALLET_DATA);
  encryptable_user_types.Remove(AUTOFILL_WALLET_OFFER);
  encryptable_user_types.Remove(AUTOFILL_WALLET_USAGE);
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
  // Proxy types have no sync representation and are therefore not encrypted.
  // Note however that proxy types map to one or more protocol types, which
  // may or may not be encrypted themselves.
  encryptable_user_types.RetainAll(ProtocolTypes());
  return encryptable_user_types;
}

const char* ModelTypeToDebugString(ModelType model_type) {
  // This is used for displaying debug information.
  return kModelTypeInfoMap[model_type].model_type_debug_string;
}

const char* ModelTypeToHistogramSuffix(ModelType model_type) {
  return kModelTypeInfoMap[model_type].histogram_suffix;
}

ModelTypeForHistograms ModelTypeHistogramValue(ModelType model_type) {
  return kModelTypeInfoMap[model_type].model_type_histogram_val;
}

int ModelTypeToStableIdentifier(ModelType model_type) {
  // Make sure the value is stable and positive.
  return static_cast<int>(ModelTypeHistogramValue(model_type)) + 1;
}

base::Value ModelTypeToValue(ModelType model_type) {
  return base::Value(ModelTypeToDebugString(model_type));
}

std::string ModelTypeSetToDebugString(ModelTypeSet model_types) {
  std::string result;
  for (ModelType type : model_types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += ModelTypeToDebugString(type);
  }
  return result;
}

std::ostream& operator<<(std::ostream& out, ModelTypeSet model_type_set) {
  return out << ModelTypeSetToDebugString(model_type_set);
}

base::Value::List ModelTypeSetToValue(ModelTypeSet model_types) {
  base::Value::List value;
  for (ModelType type : model_types) {
    value.Append(ModelTypeToDebugString(type));
  }
  return value;
}

std::string ModelTypeToProtocolRootTag(ModelType model_type) {
  DCHECK(ProtocolTypes().Has(model_type));
  DCHECK(IsRealDataType(model_type));
  const std::string root_tag =
      std::string(kModelTypeInfoMap[model_type].lowercase_root_tag);
  DCHECK(!root_tag.empty());
  return "google_chrome_" + root_tag;
}

const char* GetModelTypeLowerCaseRootTag(ModelType model_type) {
  return kModelTypeInfoMap[model_type].lowercase_root_tag;
}

bool IsRealDataType(ModelType model_type) {
  return model_type >= FIRST_REAL_MODEL_TYPE &&
         model_type <= LAST_REAL_MODEL_TYPE;
}

bool IsActOnceDataType(ModelType model_type) {
  return model_type == HISTORY_DELETE_DIRECTIVES;
}

bool IsTypeWithServerGeneratedRoot(ModelType model_type) {
  return model_type == BOOKMARKS || model_type == NIGORI;
}

bool IsTypeWithClientGeneratedRoot(ModelType model_type) {
  return IsRealDataType(model_type) &&
         !IsTypeWithServerGeneratedRoot(model_type);
}

}  // namespace syncer
