// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/model_type.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"

namespace syncer {

struct ModelTypeInfo {
  const ModelType model_type;
  // Model Type notification string.
  // This needs to match the corresponding proto message name in sync.proto. It
  // is also used to identify the model type in the SyncModelType
  // histogram_suffix in histograms.xml. Must always be kept in sync.
  const char* const notification_type;
  // Root tag for Model Type
  // This should be the same as the model type but all lowercase.
  const char* const root_tag;
  // String value for Model Type
  // This should be the same as the model type but space separated and the
  // first letter of every word capitalized.
  const char* const model_type_string;
  // Field number of the model type specifics in EntitySpecifics.
  const int specifics_field_number;
  // Model type value from SyncModelTypes enum in enums.xml. Must always be in
  // sync with the enum.
  const ModelTypeForHistograms model_type_histogram_val;
};

// Below struct entries are in the same order as their definition in the
// ModelType enum. When making changes to this list, don't forget to
//  - update the ModelType enum,
//  - update the SyncModelTypes enum in enums.xml, and
//  - update the SyncModelType histogram suffix in histograms.xml.
// Struct field values should be unique across the entire map.
const ModelTypeInfo kModelTypeInfoMap[] = {
    {UNSPECIFIED, "", "", "Unspecified", -1,
     ModelTypeForHistograms::kUnspecified},
    {TOP_LEVEL_FOLDER, "", "", "Top Level Folder", -1,
     ModelTypeForHistograms::kTopLevelFolder},
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
    {AUTOFILL_WALLET_DATA, "AUTOFILL_WALLET", "autofill_wallet",
     "Autofill Wallet", sync_pb::EntitySpecifics::kAutofillWalletFieldNumber,
     ModelTypeForHistograms::kAutofillWalletData},
    {AUTOFILL_WALLET_METADATA, "WALLET_METADATA", "autofill_wallet_metadata",
     "Autofill Wallet Metadata",
     sync_pb::EntitySpecifics::kWalletMetadataFieldNumber,
     ModelTypeForHistograms::kAutofillWalletMetadata},
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
    {FAVICON_IMAGES, "FAVICON_IMAGE", "favicon_images", "Favicon Images",
     sync_pb::EntitySpecifics::kFaviconImageFieldNumber,
     ModelTypeForHistograms::kFaviconImages},
    {FAVICON_TRACKING, "FAVICON_TRACKING", "favicon_tracking",
     "Favicon Tracking", sync_pb::EntitySpecifics::kFaviconTrackingFieldNumber,
     ModelTypeForHistograms::kFaviconTracking},
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
    {SUPERVISED_USER_WHITELISTS, "MANAGED_USER_WHITELIST",
     "managed_user_whitelists", "Managed User Whitelists",
     sync_pb::EntitySpecifics::kManagedUserWhitelistFieldNumber,
     ModelTypeForHistograms::kSupervisedUserWhitelists},
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
    // ---- Proxy types ----
    {PROXY_TABS, "", "", "Tabs", -1, ModelTypeForHistograms::kProxyTabs},
    // ---- Control Types ----
    {NIGORI, "NIGORI", "nigori", "Encryption Keys",
     sync_pb::EntitySpecifics::kNigoriFieldNumber,
     ModelTypeForHistograms::kNigori},
    {DEPRECATED_EXPERIMENTS, "EXPERIMENTS", "experiments", "Experiments",
     sync_pb::EntitySpecifics::kExperimentsFieldNumber,
     ModelTypeForHistograms::kDeprecatedExperiments},
};

static_assert(base::size(kModelTypeInfoMap) == ModelType::NUM_ENTRIES,
              "kModelTypeInfoMap should have ModelType::NUM_ENTRIES elements");

static_assert(40 == syncer::ModelType::NUM_ENTRIES,
              "When adding a new type, update enum SyncModelTypes in enums.xml "
              "and suffix SyncModelType in histograms.xml.");

static_assert(40 == syncer::ModelType::NUM_ENTRIES,
              "When adding a new type, update kAllocatorDumpNameWhitelist in "
              "base/trace_event/memory_infra_background_whitelist.cc.");

void AddDefaultFieldValue(ModelType type, sync_pb::EntitySpecifics* specifics) {
  switch (type) {
    case UNSPECIFIED:
    case TOP_LEVEL_FOLDER:
      NOTREACHED() << "No default field value for " << ModelTypeToString(type);
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
    case AUTOFILL_WALLET_DATA:
      specifics->mutable_autofill_wallet();
      break;
    case AUTOFILL_WALLET_METADATA:
      specifics->mutable_wallet_metadata();
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
    case FAVICON_IMAGES:
      specifics->mutable_favicon_image();
      break;
    case FAVICON_TRACKING:
      specifics->mutable_favicon_tracking();
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
    case SUPERVISED_USER_WHITELISTS:
      specifics->mutable_managed_user_whitelist();
      break;
    case ARC_PACKAGE:
      specifics->mutable_arc_package();
      break;
    case PRINTERS:
      specifics->mutable_printer();
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
      NOTREACHED() << "No default field value for " << ModelTypeToString(type);
      break;
    case NIGORI:
      specifics->mutable_nigori();
      break;
    case DEPRECATED_EXPERIMENTS:
      specifics->mutable_experiments();
      break;
    case WEB_APPS:
      specifics->mutable_web_app();
      break;
    case WIFI_CONFIGURATIONS:
      specifics->mutable_wifi_configuration();
      break;
    case OS_PREFERENCES:
      specifics->mutable_os_preference();
      break;
    case OS_PRIORITY_PREFERENCES:
      specifics->mutable_os_priority_preference();
      break;
    case ModelType::NUM_ENTRIES:
      NOTREACHED() << "No default field value for " << ModelTypeToString(type);
      break;
  }
}

ModelType GetModelTypeFromSpecificsFieldNumber(int field_number) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    if (GetSpecificsFieldNumberFromModelType(type) == field_number)
      return type;
  }
  return UNSPECIFIED;
}

int GetSpecificsFieldNumberFromModelType(ModelType model_type) {
  DCHECK(ProtocolTypes().Has(model_type))
      << "Only protocol types have field values.";
  return kModelTypeInfoMap[model_type].specifics_field_number;
}

// Note: keep this consistent with GetModelType in entry.cc!
ModelType GetModelType(const sync_pb::SyncEntity& sync_entity) {
  ModelType specifics_type = GetModelTypeFromSpecifics(sync_entity.specifics());
  if (specifics_type != UNSPECIFIED)
    return specifics_type;

  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!sync_entity.server_defined_unique_tag().empty() &&
      sync_entity.folder()) {
    return TOP_LEVEL_FOLDER;
  }

  // This is an item of a datatype we can't understand. Maybe it's
  // from the future?  Either we mis-encoded the object, or the
  // server sent us entries it shouldn't have.
  DVLOG(1) << "Unknown datatype in sync proto.";
  return UNSPECIFIED;
}

ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics) {
  static_assert(40 == ModelType::NUM_ENTRIES,
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
  if (specifics.has_favicon_image())
    return FAVICON_IMAGES;
  if (specifics.has_favicon_tracking())
    return FAVICON_TRACKING;
  if (specifics.has_device_info())
    return DEVICE_INFO;
  if (specifics.has_priority_preference())
    return PRIORITY_PREFERENCES;
  if (specifics.has_managed_user_setting())
    return SUPERVISED_USER_SETTINGS;
  if (specifics.has_app_list())
    return APP_LIST;
  if (specifics.has_managed_user_whitelist())
    return SUPERVISED_USER_WHITELISTS;
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
  if (specifics.has_experiments())
    return DEPRECATED_EXPERIMENTS;
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

  return UNSPECIFIED;
}

ModelTypeSet EncryptableUserTypes() {
  static_assert(40 == ModelType::NUM_ENTRIES,
                "If adding an unencryptable type, remove from "
                "encryptable_user_types below.");
  ModelTypeSet encryptable_user_types = UserTypes();
  // Wallet data is not encrypted since it actually originates on the server.
  encryptable_user_types.Remove(AUTOFILL_WALLET_DATA);
  // We never encrypt history delete directives.
  encryptable_user_types.Remove(HISTORY_DELETE_DIRECTIVES);
  // Device info data is not encrypted because it might be synced before
  // encryption is ready.
  encryptable_user_types.Remove(DEVICE_INFO);
  // Priority preferences are not encrypted because they might be synced before
  // encryption is ready.
  encryptable_user_types.Remove(PRIORITY_PREFERENCES);
  // OS priority preferences are not encrypted because they might be synced
  // before encryption is ready.
  encryptable_user_types.Remove(OS_PRIORITY_PREFERENCES);
  // Supervised user settings are not encrypted since they are set server-side.
  encryptable_user_types.Remove(SUPERVISED_USER_SETTINGS);
  // Supervised user whitelists are not encrypted since they are managed
  // server-side.
  encryptable_user_types.Remove(SUPERVISED_USER_WHITELISTS);
  // User events and consents are not encrypted since they are consumed
  // server-side.
  encryptable_user_types.Remove(USER_EVENTS);
  encryptable_user_types.Remove(USER_CONSENTS);
  encryptable_user_types.Remove(SECURITY_EVENTS);
  // Proxy types have no sync representation and are therefore not encrypted.
  // Note however that proxy types map to one or more protocol types, which
  // may or may not be encrypted themselves.
  encryptable_user_types.RemoveAll(ProxyTypes());
  return encryptable_user_types;
}

const char* ModelTypeToString(ModelType model_type) {
  // This is used in serialization routines as well as for displaying debug
  // information.  Do not attempt to change these string values unless you know
  // what you're doing.
  if (model_type >= UNSPECIFIED && model_type < ModelType::NUM_ENTRIES)
    return kModelTypeInfoMap[model_type].model_type_string;
  NOTREACHED() << "No known extension for model type.";
  return "Invalid";
}

const char* ModelTypeToHistogramSuffix(ModelType model_type) {
  DCHECK_GE(model_type, UNSPECIFIED);
  DCHECK_LT(model_type, ModelType::NUM_ENTRIES);

  // We use the same string that is used for notification types because they
  // satisfy all we need (being stable and explanatory).
  return kModelTypeInfoMap[model_type].notification_type;
}

ModelTypeForHistograms ModelTypeHistogramValue(ModelType model_type) {
  DCHECK_GE(model_type, UNSPECIFIED);
  DCHECK_LT(model_type, ModelType::NUM_ENTRIES);
  return kModelTypeInfoMap[model_type].model_type_histogram_val;
}

int ModelTypeToStableIdentifier(ModelType model_type) {
  DCHECK_GE(model_type, UNSPECIFIED);
  DCHECK_LT(model_type, ModelType::NUM_ENTRIES);
  // Make sure the value is stable and positive.
  return static_cast<int>(ModelTypeHistogramValue(model_type)) + 1;
}

std::unique_ptr<base::Value> ModelTypeToValue(ModelType model_type) {
  if (model_type >= FIRST_REAL_MODEL_TYPE) {
    return std::make_unique<base::Value>(ModelTypeToString(model_type));
  }
  if (model_type == TOP_LEVEL_FOLDER) {
    return std::make_unique<base::Value>("Top-level folder");
  }
  DCHECK_EQ(model_type, UNSPECIFIED);
  return std::make_unique<base::Value>("Unspecified");
}

ModelType ModelTypeFromString(const std::string& model_type_string) {
  if (model_type_string != "Unspecified" &&
      model_type_string != "Top Level Folder") {
    for (size_t i = 0; i < base::size(kModelTypeInfoMap); ++i) {
      if (kModelTypeInfoMap[i].model_type_string == model_type_string)
        return kModelTypeInfoMap[i].model_type;
    }
  }
  return UNSPECIFIED;
}

std::string ModelTypeSetToString(ModelTypeSet model_types) {
  std::string result;
  for (ModelType type : model_types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += ModelTypeToString(type);
  }
  return result;
}

std::ostream& operator<<(std::ostream& out, ModelTypeSet model_type_set) {
  return out << ModelTypeSetToString(model_type_set);
}

ModelTypeSet ModelTypeSetFromString(const std::string& model_types_string) {
  std::string working_copy = model_types_string;
  ModelTypeSet model_types;
  while (!working_copy.empty()) {
    // Remove any leading spaces.
    working_copy = working_copy.substr(working_copy.find_first_not_of(' '));
    if (working_copy.empty())
      break;
    std::string type_str;
    size_t end = working_copy.find(',');
    if (end == std::string::npos) {
      end = working_copy.length() - 1;
      type_str = working_copy;
    } else {
      type_str = working_copy.substr(0, end);
    }
    ModelType type = ModelTypeFromString(type_str);
    if (IsRealDataType(type))
      model_types.Put(type);
    working_copy = working_copy.substr(end + 1);
  }
  return model_types;
}

std::unique_ptr<base::ListValue> ModelTypeSetToValue(ModelTypeSet model_types) {
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  for (ModelType type : model_types) {
    value->AppendString(ModelTypeToString(type));
  }
  return value;
}

// TODO(zea): remove all hardcoded tags in model associators and have them use
// this instead.
std::string ModelTypeToRootTag(ModelType type) {
  if (IsProxyType(type))
    return std::string();
  DCHECK(IsRealDataType(type));
  return "google_chrome_" + std::string(kModelTypeInfoMap[type].root_tag);
}

const char* GetModelTypeRootTag(ModelType model_type) {
  return kModelTypeInfoMap[model_type].root_tag;
}

bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type) {
  if (ProtocolTypes().Has(model_type)) {
    *notification_type = kModelTypeInfoMap[model_type].notification_type;
    return true;
  }
  notification_type->clear();
  return false;
}

bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type) {
  if (notification_type.empty()) {
    *model_type = UNSPECIFIED;
    return false;
  }
  for (size_t i = 0; i < base::size(kModelTypeInfoMap); ++i) {
    if (kModelTypeInfoMap[i].notification_type == notification_type) {
      *model_type = kModelTypeInfoMap[i].model_type;
      return true;
    }
  }
  *model_type = UNSPECIFIED;
  return false;
}

bool IsRealDataType(ModelType model_type) {
  return model_type >= FIRST_REAL_MODEL_TYPE &&
         model_type < ModelType::NUM_ENTRIES;
}

bool IsProxyType(ModelType model_type) {
  return model_type >= FIRST_PROXY_TYPE && model_type <= LAST_PROXY_TYPE;
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

bool TypeSupportsHierarchy(ModelType model_type) {
  // TODO(stanisc): crbug/438313: Should this also include TOP_LEVEL_FOLDER?
  return model_type == BOOKMARKS;
}

bool TypeSupportsOrdering(ModelType model_type) {
  return model_type == BOOKMARKS;
}

}  // namespace syncer
