// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type.h"

#include <array>
#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {

namespace {

// Determines when and if the additional Nigori encryption layer is used for a
// datatype.
enum class EncryptionPolicy {
  // Default: the type is encrypted using Nigori encryption only if the user
  // configured a custom passphrase (or the legacy frozen implicit passphrase).
  kEncryptedIfCustomPassphraseSet,
  // The type is always encrypted (e.g. PASSWORDS).
  kAlwaysEncrypted,
  // The type is never encrypted (e.g. DEVICE_INFO).
  kNeverEncrypted,
};

// Determines the priority of downloading or uploading changes for a datatype.
enum class DataTypePriority {
  kLow,
  kRegular,
  kHigh,
};

// Used to identify the communication direction of a data type.
enum class CommunicationDirection {
  kRegularTwoWay,
  kCommitOnly,
};

enum class UnsyncedDataCheckOnSignoutPolicy {
  kNone,
  kRequired,
};

enum class CrossUserSharingPolicy {
  kNone,
  kShared,
};

enum class ApplyUpdatesBatchPolicy {
  kStandard,
  kImmediately,
};

struct DataTypeInfo {
  DataType type;
  int specifics_field_number;
  std::string_view debug_string;
  std::string_view histogram_suffix;
  std::string_view stable_lowercase_string;
  EncryptionPolicy encryption_policy;
  DataTypePriority priority;
  CommunicationDirection communication_direction;
  ApplyUpdatesBatchPolicy apply_updates_batch_policy;
  UnsyncedDataCheckOnSignoutPolicy unsynced_data_check_on_signout_policy;
  CrossUserSharingPolicy cross_user_sharing_policy;
};

constexpr std::array<DataTypeInfo, syncer::GetNumDataTypes()>
    kDataTypeInfoTable = {{
        {
            .type = UNSPECIFIED,
            .specifics_field_number = -1,
            .debug_string = "Unspecified",
            .histogram_suffix = "",
            .stable_lowercase_string = "",
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = BOOKMARKS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kBookmarkFieldNumber,
            .debug_string = "Bookmarks",
            .histogram_suffix = "BOOKMARK",
            .stable_lowercase_string = "bookmarks",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PREFERENCES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kPreferenceFieldNumber,
            .debug_string = "Preferences",
            .histogram_suffix = "PREFERENCE",
            .stable_lowercase_string = "preferences",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PASSWORDS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kPasswordFieldNumber,
            .debug_string = "Passwords",
            .histogram_suffix = "PASSWORD",
            .stable_lowercase_string = "passwords",
            .encryption_policy = EncryptionPolicy::kAlwaysEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_PROFILE,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillProfileFieldNumber,
            .debug_string = "Autofill Profiles",
            .histogram_suffix = "AUTOFILL_PROFILE",
            .stable_lowercase_string = "autofill_profiles",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillFieldNumber,
            .debug_string = "Autofill",
            .histogram_suffix = "AUTOFILL",
            .stable_lowercase_string = "autofill",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_WALLET_CREDENTIAL,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillWalletCredentialFieldNumber,
            .debug_string = "Autofill Wallet Credential",
            .histogram_suffix = "AUTOFILL_WALLET_CREDENTIAL",
            .stable_lowercase_string = "autofill_wallet_credential",
            .encryption_policy = EncryptionPolicy::kAlwaysEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_WALLET_DATA,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillWalletFieldNumber,
            .debug_string = "Autofill Wallet",
            .histogram_suffix = "AUTOFILL_WALLET",
            .stable_lowercase_string = "autofill_wallet",
            // Wallet data is not encrypted since it actually originates on the
            // server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_WALLET_METADATA,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kWalletMetadataFieldNumber,
            .debug_string = "Autofill Wallet Metadata",
            .histogram_suffix = "WALLET_METADATA",
            .stable_lowercase_string = "autofill_wallet_metadata",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_WALLET_OFFER,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillOfferFieldNumber,
            .debug_string = "Autofill Wallet Offer",
            .histogram_suffix = "AUTOFILL_OFFER",
            .stable_lowercase_string = "autofill_wallet_offer",
            // Wallet data is not encrypted since it actually originates on the
            // server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_WALLET_USAGE,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillWalletUsageFieldNumber,
            .debug_string = "Autofill Wallet Usage",
            .histogram_suffix = "AUTOFILL_WALLET_USAGE",
            .stable_lowercase_string = "autofill_wallet_usage",
            // Wallet data is not encrypted since it actually originates on the
            // server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = THEMES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kThemeFieldNumber,
            .debug_string = "Themes",
            .histogram_suffix = "THEME",
            .stable_lowercase_string = "themes",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = EXTENSIONS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kExtensionFieldNumber,
            .debug_string = "Extensions",
            .histogram_suffix = "EXTENSION",
            .stable_lowercase_string = "extensions",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SEARCH_ENGINES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSearchEngineFieldNumber,
            .debug_string = "Search Engines",
            .histogram_suffix = "SEARCH_ENGINE",
            .stable_lowercase_string = "search_engines",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SESSIONS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSessionFieldNumber,
            .debug_string = "Sessions",
            .histogram_suffix = "SESSION",
            .stable_lowercase_string = "sessions",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = APPS,
            .specifics_field_number = sync_pb::EntitySpecifics::kAppFieldNumber,
            .debug_string = "Apps",
            .histogram_suffix = "APP",
            .stable_lowercase_string = "apps",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = APP_SETTINGS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAppSettingFieldNumber,
            .debug_string = "App settings",
            .histogram_suffix = "APP_SETTING",
            .stable_lowercase_string = "app_settings",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = EXTENSION_SETTINGS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kExtensionSettingFieldNumber,
            .debug_string = "Extension settings",
            .histogram_suffix = "EXTENSION_SETTING",
            .stable_lowercase_string = "extension_settings",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = HISTORY_DELETE_DIRECTIVES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kHistoryDeleteDirectiveFieldNumber,
            .debug_string = "History Delete Directives",
            .histogram_suffix = "HISTORY_DELETE_DIRECTIVE",
            .stable_lowercase_string = "history_delete_directives",
            // History Sync is disabled if encryption is enabled.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = DICTIONARY,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kDictionaryFieldNumber,
            .debug_string = "Dictionary",
            .histogram_suffix = "DICTIONARY",
            .stable_lowercase_string = "dictionary",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = DEVICE_INFO,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kDeviceInfoFieldNumber,
            .debug_string = "Device Info",
            .histogram_suffix = "DEVICE_INFO",
            .stable_lowercase_string = "device_info",
            // Never encrypted because consumed server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PRIORITY_PREFERENCES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kPriorityPreferenceFieldNumber,
            .debug_string = "Priority Preferences",
            .histogram_suffix = "PRIORITY_PREFERENCE",
            .stable_lowercase_string = "priority_preferences",
            // Never encrypted because also written server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SUPERVISED_USER_SETTINGS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kManagedUserSettingFieldNumber,
            .debug_string = "Managed User Settings",
            .histogram_suffix = "MANAGED_USER_SETTING",
            .stable_lowercase_string = "managed_user_settings",
            // Never encrypted because also written server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = APP_LIST,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAppListFieldNumber,
            .debug_string = "App List",
            .histogram_suffix = "APP_LIST",
            .stable_lowercase_string = "app_list",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = ARC_PACKAGE,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kArcPackageFieldNumber,
            .debug_string = "Arc Package",
            .histogram_suffix = "ARC_PACKAGE",
            .stable_lowercase_string = "arc_package",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PRINTERS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kPrinterFieldNumber,
            .debug_string = "Printers",
            .histogram_suffix = "PRINTER",
            .stable_lowercase_string = "printers",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = READING_LIST,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kReadingListFieldNumber,
            .debug_string = "Reading List",
            .histogram_suffix = "READING_LIST",
            .stable_lowercase_string = "reading_list",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = USER_EVENTS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kUserEventFieldNumber,
            .debug_string = "User Events",
            .histogram_suffix = "USER_EVENT",
            .stable_lowercase_string = "user_events",
            // Commit-only types are never encrypted since they are consumed
            // server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kLow,
            .communication_direction = CommunicationDirection::kCommitOnly,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = USER_CONSENTS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kUserConsentFieldNumber,
            .debug_string = "User Consents",
            .histogram_suffix = "USER_CONSENT",
            .stable_lowercase_string = "user_consent",
            // Commit-only types are never encrypted since they are consumed
            // server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kCommitOnly,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SEND_TAB_TO_SELF,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSendTabToSelfFieldNumber,
            .debug_string = "Send Tab To Self",
            .histogram_suffix = "SEND_TAB_TO_SELF",
            .stable_lowercase_string = "send_tab_to_self",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SECURITY_EVENTS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSecurityEventFieldNumber,
            .debug_string = "Security Events",
            .histogram_suffix = "SECURITY_EVENT",
            .stable_lowercase_string = "security_events",
            // Commit-only types are never encrypted since they are consumed
            // server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kCommitOnly,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = WIFI_CONFIGURATIONS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kWifiConfigurationFieldNumber,
            .debug_string = "Wifi Configurations",
            .histogram_suffix = "WIFI_CONFIGURATION",
            .stable_lowercase_string = "wifi_configurations",
            .encryption_policy = EncryptionPolicy::kAlwaysEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = WEB_APPS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kWebAppFieldNumber,
            .debug_string = "Web Apps",
            .histogram_suffix = "WEB_APP",
            .stable_lowercase_string = "web_apps",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = WEB_APKS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kWebApkFieldNumber,
            .debug_string = "Web Apks",
            .histogram_suffix = "WEB_APK",
            .stable_lowercase_string = "webapks",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = OS_PREFERENCES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kOsPreferenceFieldNumber,
            .debug_string = "OS Preferences",
            .histogram_suffix = "OS_PREFERENCE",
            .stable_lowercase_string = "os_preferences",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = OS_PRIORITY_PREFERENCES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kOsPriorityPreferenceFieldNumber,
            .debug_string = "OS Priority Preferences",
            .histogram_suffix = "OS_PRIORITY_PREFERENCE",
            .stable_lowercase_string = "os_priority_preferences",
            // Never encrypted because also written server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SHARING_MESSAGE,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSharingMessageFieldNumber,
            .debug_string = "Sharing Message",
            .histogram_suffix = "SHARING_MESSAGE",
            .stable_lowercase_string = "sharing_message",
            // Commit-only types are never encrypted since they are consumed
            // server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kCommitOnly,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = WORKSPACE_DESK,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kWorkspaceDeskFieldNumber,
            .debug_string = "Workspace Desk",
            .histogram_suffix = "WORKSPACE_DESK",
            .stable_lowercase_string = "workspace_desk",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = HISTORY,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kHistoryFieldNumber,
            .debug_string = "History",
            .histogram_suffix = "HISTORY",
            .stable_lowercase_string = "history",
            // History Sync is disabled if encryption is enabled.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kLow,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kImmediately,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PRINTERS_AUTHORIZATION_SERVERS,
            .specifics_field_number = sync_pb::EntitySpecifics::
                kPrintersAuthorizationServerFieldNumber,
            .debug_string = "Printers Authorization Servers",
            .histogram_suffix = "PRINTERS_AUTHORIZATION_SERVER",
            .stable_lowercase_string = "printers_authorization_servers",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = CONTACT_INFO,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kContactInfoFieldNumber,
            .debug_string = "Contact Info",
            .histogram_suffix = "CONTACT_INFO",
            .stable_lowercase_string = "contact_info",
            // Not encrypted since it originates on the server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SAVED_TAB_GROUP,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSavedTabGroupFieldNumber,
            .debug_string = "Saved Tab Group",
            .histogram_suffix = "SAVED_TAB_GROUP",
            .stable_lowercase_string = "saved_tab_group",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kRequired,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = WEBAUTHN_CREDENTIAL,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kWebauthnCredentialFieldNumber,
            .debug_string = "WebAuthn Credentials",
            .histogram_suffix = "WEBAUTHN_CREDENTIAL",
            .stable_lowercase_string = "webauthn_credential",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = INCOMING_PASSWORD_SHARING_INVITATION,
            .specifics_field_number = sync_pb::EntitySpecifics::
                kIncomingPasswordSharingInvitationFieldNumber,
            .debug_string = "Incoming Password Sharing Invitations",
            .histogram_suffix = "INCOMING_PASSWORD_SHARING_INVITATION",
            .stable_lowercase_string = "incoming_password_sharing_invitation",
            // Password sharing invitations have different encryption
            // implementation.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kLow,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = OUTGOING_PASSWORD_SHARING_INVITATION,
            .specifics_field_number = sync_pb::EntitySpecifics::
                kOutgoingPasswordSharingInvitationFieldNumber,
            .debug_string = "Outgoing Password Sharing Invitations",
            .histogram_suffix = "OUTGOING_PASSWORD_SHARING_INVITATION",
            .stable_lowercase_string = "outgoing_password_sharing_invitation",
            // Password sharing invitations have different encryption
            // implementation. Also commit-only types are never encrypted since
            // they are consumed server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kCommitOnly,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SHARED_TAB_GROUP_DATA,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSharedTabGroupDataFieldNumber,
            .debug_string = "Shared Tab Group Data",
            .histogram_suffix = "SHARED_TAB_GROUP_DATA",
            .stable_lowercase_string = "shared_tab_group_data",
            // Never encrypted because consumed server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kShared,
        },
        {
            .type = COLLABORATION_GROUP,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kCollaborationGroupFieldNumber,
            .debug_string = "Collaboration Group",
            .histogram_suffix = "COLLABORATION_GROUP",
            .stable_lowercase_string = "collaboration_group",
            // Not encrypted since it originates on the server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kHigh,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PLUS_ADDRESS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kPlusAddressFieldNumber,
            .debug_string = "Plus Address",
            .histogram_suffix = "PLUS_ADDRESS",
            .stable_lowercase_string = "plus_address",
            // Plus addresses and their settings are never encrypted because
            // they originate from outside Chrome.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PRODUCT_COMPARISON,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kProductComparisonFieldNumber,
            .debug_string = "Product Comparison",
            .histogram_suffix = "PRODUCT_COMPARISON",
            .stable_lowercase_string = "product_comparison",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = COOKIES,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kCookieFieldNumber,
            .debug_string = "Cookies",
            .histogram_suffix = "COOKIE",
            .stable_lowercase_string = "cookies",
            .encryption_policy = EncryptionPolicy::kAlwaysEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = PLUS_ADDRESS_SETTING,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kPlusAddressSettingFieldNumber,
            .debug_string = "Plus Address Setting",
            .histogram_suffix = "PLUS_ADDRESS_SETTING",
            .stable_lowercase_string = "plus_address_setting",
            // Plus addresses and their settings are never encrypted because
            // they originate from outside Chrome.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_VALUABLE,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillValuableFieldNumber,
            .debug_string = "Autofill Valuable",
            .histogram_suffix = "AUTOFILL_VALUABLE",
            .stable_lowercase_string = "autofill_valuable",
            // Valuables are never encrypted because they can be generated from
            // outside Chrome.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AUTOFILL_VALUABLE_METADATA,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAutofillValuableMetadataFieldNumber,
            .debug_string = "Autofill Valuable Metadata",
            .histogram_suffix = "AUTOFILL_VALUABLE_METADATA",
            .stable_lowercase_string = "autofill_valuable_metadata",
            // Valuable metadata is accessed on the server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SHARED_TAB_GROUP_ACCOUNT_DATA,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSharedTabGroupAccountDataFieldNumber,
            .debug_string = "Shared Tab Group Account Data",
            .histogram_suffix = "SHARED_TAB_GROUP_ACCOUNT_DATA",
            .stable_lowercase_string = "shared_tab_group_account_data",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SHARED_COMMENT,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSharedCommentFieldNumber,
            .debug_string = "SharedComment",
            .histogram_suffix = "SHARED_COMMENT",
            .stable_lowercase_string = "shared_comment",
            // Never encrypted because consumed server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = ACCOUNT_SETTING,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAccountSettingFieldNumber,
            .debug_string = "Account Setting",
            .histogram_suffix = "ACCOUNT_SETTING",
            .stable_lowercase_string = "account_setting",
            // Account settings are read-only and therefore never encrypted.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = AI_THREAD,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAiThreadFieldNumber,
            .debug_string = "AI Thread",
            .histogram_suffix = "AI_THREAD",
            .stable_lowercase_string = "ai_thread",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = CONTEXTUAL_TASK,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kContextualTaskFieldNumber,
            .debug_string = "Contextual Task",
            .histogram_suffix = "CONTEXTUAL_TASK",
            .stable_lowercase_string = "contextual_task",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = SKILL,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kSkillFieldNumber,
            .debug_string = "Skill",
            .histogram_suffix = "SKILL",
            .stable_lowercase_string = "skill",
            // Never encrypted because consumed server-side.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = GEMINI_THREAD,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kGeminiThreadFieldNumber,
            .debug_string = "Gemini Thread",
            .histogram_suffix = "GEMINI_THREAD",
            .stable_lowercase_string = "gemini_thread",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = THEMES_IOS,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kThemeIosFieldNumber,
            .debug_string = "Themes (iOS)",
            .histogram_suffix = "THEME_IOS",
            .stable_lowercase_string = "themes_ios",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = ACCESSIBILITY_ANNOTATION,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kAccessibilityAnnotationFieldNumber,
            .debug_string = "Accessibility Annotation",
            .histogram_suffix = "ACCESSIBILITY_ANNOTATION",
            .stable_lowercase_string = "accessibility_annotation",
            // Not encrypted since it originates from the server.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = THEMES_ANDROID,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kThemeAndroidFieldNumber,
            .debug_string = "Themes (Android)",
            .histogram_suffix = "THEMES_ANDROID",
            .stable_lowercase_string = "themes_android",
            .encryption_policy =
                EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
        {
            .type = NIGORI,
            .specifics_field_number =
                sync_pb::EntitySpecifics::kNigoriFieldNumber,
            .debug_string = "Encryption Keys",
            .histogram_suffix = "NIGORI",
            .stable_lowercase_string = "nigori",
            // Nigori has built-in encryption and powers the encryption of other
            // datatypes.
            .encryption_policy = EncryptionPolicy::kNeverEncrypted,
            .priority = DataTypePriority::kRegular,
            .communication_direction = CommunicationDirection::kRegularTwoWay,
            .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
            .unsynced_data_check_on_signout_policy =
                UnsyncedDataCheckOnSignoutPolicy::kNone,
            .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
        },
    }};

// LINT.IfChange(DataTypeHistogramSuffix)
static_assert(GetNumDataTypes() == 64,
              "When adding a new type, update kDataTypeInfoTable, update "
              "histograms.xml and follow the integration checklist in "
              "https://www.chromium.org/developers/design-documents/sync/"
              "integration-checklist/");
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/histograms.xml:DataTypeHistogramSuffix)

const DataTypeInfo& GetDataTypeInfo(DataType type) {
  static const base::NoDestructor<
      absl::flat_hash_map<DataType, const DataTypeInfo*>>
      type_to_info([] {
        absl::flat_hash_map<DataType, const DataTypeInfo*> map;
        for (const auto& info : kDataTypeInfoTable) {
          map.emplace(info.type, &info);
        }
        return map;
      }());

  auto it = type_to_info->find(type);
  CHECK(it != type_to_info->end())
      << "Unknown data type: " << static_cast<int>(type);
  return *it->second;
}

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
    case THEMES_ANDROID:
      specifics->mutable_theme_android();
      break;
  }
}

DataType GetDataTypeFromSpecificsFieldNumber(int field_number) {
  static const base::NoDestructor<absl::flat_hash_map<int, DataType>>
      field_number_to_type([] {
        absl::flat_hash_map<int, DataType> map;
        for (const auto& info : kDataTypeInfoTable) {
          if (info.specifics_field_number != -1) {
            map.emplace(info.specifics_field_number, info.type);
          }
        }
        return map;
      }());

  auto it = field_number_to_type->find(field_number);
  return (it != field_number_to_type->end()) ? it->second : UNSPECIFIED;
}

int GetSpecificsFieldNumberFromDataType(DataType data_type) {
  CHECK(ProtocolTypes().Has(data_type))
      << "Only protocol types have field values: "
      << DataTypeToDebugString(data_type);
  return GetDataTypeInfo(data_type).specifics_field_number;
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

DataTypeSet ProtocolTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.type != UNSPECIFIED) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet UserTypes() {
  static const DataTypeSet types =
      DataTypeSet::FromRange(FIRST_USER_DATA_TYPE, LAST_USER_DATA_TYPE);
  return types;
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
                       SKILL,
                       AI_THREAD,
                       GEMINI_THREAD};
  // TODO(crbug.com/412602018): Mark AlwaysPreferredUserTypes() method as
  // constexpr when removing the feature flag.
  if (!base::FeatureList::IsEnabled(
          kSyncSupportAlwaysSyncingPriorityPreferences)) {
    types.Remove(PRIORITY_PREFERENCES);
  }

  if (base::FeatureList::IsEnabled(kSyncAccessibilityAnnotation)) {
    types.Put(ACCESSIBILITY_ANNOTATION);
  }

  return types;
}

DataTypeSet AlwaysEncryptedUserTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      switch (info.encryption_policy) {
        case EncryptionPolicy::kAlwaysEncrypted:
          types.Put(info.type);
          break;
        case EncryptionPolicy::kEncryptedIfCustomPassphraseSet:
        case EncryptionPolicy::kNeverEncrypted:
          break;
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet HighPriorityUserTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.priority == DataTypePriority::kHigh) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet LowPriorityUserTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.priority == DataTypePriority::kLow) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet ControlTypes() {
  return {NIGORI};
}

DataTypeSet CommitOnlyTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.communication_direction == CommunicationDirection::kCommitOnly) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet ApplyUpdatesImmediatelyTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.apply_updates_batch_policy ==
          ApplyUpdatesBatchPolicy::kImmediately) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet SharedTypes() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.cross_user_sharing_policy == CrossUserSharingPolicy::kShared) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet TypesRequiringUnsyncedDataCheckOnSignout() {
  static const DataTypeSet types = [] {
    DataTypeSet types;
    for (const auto& info : kDataTypeInfoTable) {
      if (info.unsynced_data_check_on_signout_policy ==
          UnsyncedDataCheckOnSignoutPolicy::kRequired) {
        types.Put(info.type);
      }
    }
    return types;
  }();
  return types;
}

DataTypeSet EncryptableUserTypes() {
  DataTypeSet encryptable_user_types;
  for (const auto& info : kDataTypeInfoTable) {
    if (UserTypes().Has(info.type) &&
        info.encryption_policy != EncryptionPolicy::kNeverEncrypted) {
      encryptable_user_types.Put(info.type);
    }
  }
  return encryptable_user_types;
}

std::string_view DataTypeToDebugString(DataType data_type) {
  return GetDataTypeInfo(data_type).debug_string;
}

std::string_view DataTypeToHistogramSuffix(DataType data_type) {
  return GetDataTypeInfo(data_type).histogram_suffix;
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
    case THEMES_ANDROID:
      return DataTypeForHistograms::kThemesAndroid;
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

std::string_view DataTypeToStableLowerCaseString(DataType data_type) {
  return GetDataTypeInfo(data_type).stable_lowercase_string;
}

std::ostream& operator<<(std::ostream& out, DataType data_type) {
  return out << DataTypeToDebugString(data_type);
}

std::ostream& operator<<(std::ostream& out, DataTypeSet data_type_set) {
  return out << DataTypeSetToDebugString(data_type_set);
}

std::string DataTypeToProtocolRootTag(DataType data_type) {
  CHECK(ProtocolTypes().Has(data_type));
  CHECK(IsRealDataType(data_type));
  const std::string root_tag(DataTypeToStableLowerCaseString(data_type));
  CHECK(!root_tag.empty());
  return "google_chrome_" + root_tag;
}

bool IsRealDataType(DataType data_type) {
  return data_type >= FIRST_REAL_DATA_TYPE && data_type <= LAST_REAL_DATA_TYPE;
}

bool IsActOnceDataType(DataType data_type) {
  return data_type == HISTORY_DELETE_DIRECTIVES;
}

}  // namespace syncer
