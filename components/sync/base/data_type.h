// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_DATA_TYPE_H_
#define COMPONENTS_SYNC_BASE_DATA_TYPE_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/enum_set.h"
#include "base/values.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
enum DataType {
  // Object type unknown. This may be used when:
  // a) The client received *valid* data from a data type which this version
  // is unaware of (only present in versions newer than this one, or present
  // in older versions but removed since).
  // b) The client received invalid data from the server due to some error.
  // c) A data object was just created, in which case this is a temporary state.
  UNSPECIFIED,

  // ------------------------------------ Start of "real" data types.
  // The data types declared before here are somewhat special, as they
  // they do not correspond to any browser data model.  The remaining types
  // are bona fide data types; all have a related browser data model and
  // can be represented in the protocol using a specific Message type in the
  // EntitySpecifics protocol buffer.
  //
  // A bookmark folder or a bookmark URL object.
  BOOKMARKS,
  FIRST_USER_DATA_TYPE = BOOKMARKS,  // Declared 2nd, for debugger prettiness.
  FIRST_REAL_DATA_TYPE = FIRST_USER_DATA_TYPE,

  // A preference object, a.k.a. "Settings".
  PREFERENCES,
  // A password object.
  PASSWORDS,
  // An autofill_profile object, i.e. an address.
  AUTOFILL_PROFILE,
  // An autofill object, i.e. an autocomplete entry keyed to an HTML form field.
  AUTOFILL,
  // Credentials related to an autofill wallet instrument; aka the CVC/CVV code.
  AUTOFILL_WALLET_CREDENTIAL,
  // Credit cards and customer data from the user's account. These are read-only
  // on the client.
  AUTOFILL_WALLET_DATA,
  // Usage counts and last use dates for Wallet cards. This data is both
  // readable and writable.
  AUTOFILL_WALLET_METADATA,
  // Offers and rewards from the user's account. These are read-only on the
  // client side.
  AUTOFILL_WALLET_OFFER,
  // Autofill usage data of a payment method related to a specific merchant.
  AUTOFILL_WALLET_USAGE,
  // A theme object.
  THEMES,
  // An extension object.
  EXTENSIONS,
  // An object representing a custom search engine.
  SEARCH_ENGINES,
  // An object representing a browser session, e.g. an open tab. This is used
  // for "Open Tabs".
  SESSIONS,
  // An app object.
  APPS,
  // An app setting from the extension settings API.
  APP_SETTINGS,
  // An extension setting from the extension settings API.
  EXTENSION_SETTINGS,
  // History delete directives, used to propagate history deletions (e.g. based
  // on a time range).
  HISTORY_DELETE_DIRECTIVES,
  // Custom spelling dictionary entries.
  DICTIONARY,
  // Client-specific metadata, synced before other user types.
  DEVICE_INFO,
  // These preferences are synced before other user types and are never
  // encrypted.
  PRIORITY_PREFERENCES,
  // Family Link supervised user settings. Cannot be encrypted.
  SUPERVISED_USER_SETTINGS,
  // App List items, used by the ChromeOS app launcher.
  APP_LIST,
  // ARC package items, i.e. Android apps on ChromeOS.
  ARC_PACKAGE,
  // Printer device information. ChromeOS only.
  PRINTERS,
  // Reading list items.
  READING_LIST,
  // Commit only user events.
  USER_EVENTS,
  // Commit only user consents.
  USER_CONSENTS,
  // Tabs sent between devices.
  SEND_TAB_TO_SELF,
  // Commit only security events.
  SECURITY_EVENTS,
  // Wi-Fi network configurations + credentials
  WIFI_CONFIGURATIONS,
  // A web app object.
  WEB_APPS,
  // A WebAPK object.
  WEB_APKS,
  // OS-specific preferences (a.k.a. "OS settings"). ChromeOS only.
  OS_PREFERENCES,
  // Synced before other user types. Never encrypted. ChromeOS only.
  OS_PRIORITY_PREFERENCES,
  // Commit only sharing message object.
  SHARING_MESSAGE,
  // A workspace desk saved by user. ChromeOS only.
  WORKSPACE_DESK,
  // Synced history. An entity roughly corresponds to a navigation.
  HISTORY,
  // Trusted Authorization Servers for printers. ChromeOS only.
  PRINTERS_AUTHORIZATION_SERVERS,
  // Contact information from the Google Address Storage.
  CONTACT_INFO,
  // A tab group saved by a user. Currently only supported on desktop platforms
  // (Linux, Mac, Windows, ChromeOS) and Android.
  SAVED_TAB_GROUP,

  // WebAuthn credentials, more commonly known as passkeys.
  WEBAUTHN_CREDENTIAL,

  // Invitations for sending passwords. Outgoing invitation from one user will
  // become an incoming one for another.
  INCOMING_PASSWORD_SHARING_INVITATION,
  OUTGOING_PASSWORD_SHARING_INVITATION,

  // Data related to tab group sharing.
  SHARED_TAB_GROUP_DATA,

  // Special datatype to notify client about People Group changes. Read-only on
  // the client.
  COLLABORATION_GROUP,

  // Origin-specific email addresses forwarded from the user's account.
  // Read-only on the client.
  PLUS_ADDRESS,

  // Product comparison groups.
  PRODUCT_COMPARISON,

  // Browser cookies, ChromeOS only.
  COOKIES,

  // Settings for PLUS_ADDRESS forwarded from the user's account. Since the
  // settings originate from the user's account, this is not reusing any of the
  // standard syncable prefs.
  PLUS_ADDRESS_SETTING,

  // Valuables stored in the Google Wallet.
  // Read-only on the client.
  AUTOFILL_VALUABLE,

  // Account-local metadata for shared tab groups.
  SHARED_TAB_GROUP_ACCOUNT_DATA,

  // Comments for shared contexts.
  SHARED_COMMENT,

  // ACCOUNT_SETTING(s) forwarded from the user's account. Since the
  // settings originate from the user account, this is not reusing any of the
  // standard syncable prefs.
  // Read-only on the client.
  ACCOUNT_SETTING,

  // A user thread when interacting with AI features.
  AI_THREAD,

  // Information about a contextual task.
  CONTEXTUAL_TASK,

  // Usage metadata for `AUTOFILL_VALUABLE`.
  AUTOFILL_VALUABLE_METADATA,

  // A skill that the user has saved.
  SKILL,

  // A gemini thread.
  GEMINI_THREAD,

  // A theme object specifically for iOS devices.
  THEMES_IOS,

  // An accessibility annotation.
  ACCESSIBILITY_ANNOTATION,

  // A theme object specifically for Android devices.
  THEMES_ANDROID,

  LAST_USER_DATA_TYPE = THEMES_ANDROID,

  // ---- Control Types ----
  // An object representing a set of Nigori keys.
  NIGORI,
  LAST_REAL_DATA_TYPE = NIGORI,

  // NEW ENTRIES MUST BE ADDED ABOVE THIS.
  LAST_ENTRY = LAST_REAL_DATA_TYPE,
};

using DataTypeSet =
    base::EnumSet<DataType, FIRST_REAL_DATA_TYPE, LAST_REAL_DATA_TYPE>;

constexpr int GetNumDataTypes() {
  return static_cast<int>(DataType::LAST_ENTRY) + 1;
}

// A version of the DataType enum for use in histograms. DataType does not
// have stable values (e.g. new ones may be inserted in the middle), so it can't
// be recorded directly.
// Instead of using entries from this enum directly, you'll usually want to get
// them via DataTypeHistogramValue(data_type).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When you add a new entry or when you
// deprecate an existing one, also update SyncDataTypes in enums.xml and
// SyncDataType suffix in histograms.xml.
// LINT.IfChange(SyncDataTypes)
enum class DataTypeForHistograms {
  kUnspecified = 0,
  // kTopLevelFolder = 1,
  kBookmarks = 2,
  kPreferences = 3,
  kPasswords = 4,
  kAutofillProfile = 5,
  kAutofill = 6,
  kThemes = 7,
  // kDeprecatedTypedUrls = 8,
  kExtensions = 9,
  kSearchEngines = 10,
  kSessions = 11,
  kApps = 12,
  kAppSettings = 13,
  kExtensionSettings = 14,
  // kDeprecatedAppNotifications = 15,
  kHistoryDeleteDirectices = 16,
  kNigori = 17,
  kDeviceInfo = 18,
  // kDeprecatedExperiments = 19,
  // kDeprecatedSyncedNotifications = 20,
  kPriorityPreferences = 21,
  kDictionary = 22,
  // kDeprecatedFaviconImages = 23,
  // kDeprecatedFaviconTracking = 24,
  // kDeprecatedProxyTabs = 25,
  kSupervisedUserSettings = 26,
  // kDeprecatedSupervisedUsers = 27,
  // kDeprecatedArticles = 28,
  kAppList = 29,
  // kDeprecatedSupervisedUserSharedSettings = 30,
  // kDeprecatedSyncedNotificationAppInfo = 31,
  // kDeprecatedWifiCredentials = 32,
  kDeprecatedSupervisedUserAllowlists = 33,
  kAutofillWalletData = 34,
  kAutofillWalletMetadata = 35,
  kArcPackage = 36,
  kPrinters = 37,
  kReadingList = 38,
  kUserEvents = 39,
  // kDeprecatedMountainShares = 40,
  kUserConsents = 41,
  kSendTabToSelf = 42,
  kSecurityEvents = 43,
  kWifiConfigurations = 44,
  kWebApps = 45,
  kOsPreferences = 46,
  kOsPriorityPreferences = 47,
  kSharingMessage = 48,
  kAutofillWalletOffer = 49,
  kWorkspaceDesk = 50,
  kHistory = 51,
  kPrintersAuthorizationServers = 52,
  kContactInfo = 53,
  kAutofillWalletUsage = 54,
  // kDeprecatedSegmentation = 55,
  kSavedTabGroups = 56,
  // kDeprecatedPowerBookmark = 57,
  kWebAuthnCredentials = 58,
  kIncomingPasswordSharingInvitations = 59,
  kOutgoingPasswordSharingInvitations = 60,
  kAutofillWalletCredential = 61,
  kWebApks = 62,
  kSharedTabGroupData = 63,
  kCollaborationGroup = 64,
  kPlusAddresses = 65,
  kProductComparison = 66,
  kCookies = 67,
  kPlusAddressSettings = 68,
  kAutofillValuable = 69,
  kSharedTabGroupAccountData = 70,
  kSharedComment = 71,
  kAccountSetting = 72,
  kAIThread = 73,
  kContextualTask = 74,
  kAutofillValuableMetadata = 75,
  kSkill = 76,
  kGeminiThread = 77,
  kThemesIos = 78,
  kAccessibilityAnnotation = 79,
  kThemesAndroid = 80,
  kMaxValue = kThemesAndroid,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncDataTypes)

// Used to mark the type of EntitySpecifics that has no actual data.
void AddDefaultFieldValue(DataType type, sync_pb::EntitySpecifics* specifics);

// Extract the data type from an EntitySpecifics field. DataType is a
// local concept: the enum is not in the protocol.
DataType GetDataTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics);

// Protocol types are those types that have actual protocol buffer
// representations. This is the same as the "real" data types, i.e. all types
// except UNSPECIFIED.
DataTypeSet ProtocolTypes();

// These are the normal user-controlled types. This is to distinguish from
// ControlTypes which are always enabled.  Note that some of these share a
// preference flag, so not all of them are individually user-selectable.
DataTypeSet UserTypes();

// User types which are not user-controlled.
DataTypeSet AlwaysPreferredUserTypes();

// User types which are always encrypted.
DataTypeSet AlwaysEncryptedUserTypes();

// This is the subset of UserTypes() that have priority over other types. These
// types are synced before other user types (both for get_updates and commits).
// This mostly matters during initial sync, since priority types can become
// active before all the data for non-prio types has been downloaded (which may
// be a lot of data).
DataTypeSet HighPriorityUserTypes();

// This is the subset of UserTypes() that have a *lower* priority than other
// types. These types are synced only after all other user types (both for
// get_updates and commits). This mostly matters during initial sync, since
// high-priority and regular types can become active before all the data for
// low-priority types has been downloaded (which may be a lot of data).
DataTypeSet LowPriorityUserTypes();

// Returns a list of all control types.
//
// The control types are intended to contain metadata nodes that are essential
// for the normal operation of the syncer.  As such, they have the following
// special properties:
// - They are downloaded early during SyncBackend initialization.
// - They are always enabled.  Users may not disable these types.
// - Their contents are not encrypted automatically.
// - They support custom update application and conflict resolution logic.
// - All change processing occurs on the sync thread.
DataTypeSet ControlTypes();

// Types that may commit data, but should never be included in a GetUpdates.
// These are never encrypted.
DataTypeSet CommitOnlyTypes();

// Types for which downloaded updates are applied immediately, before all
// updates are downloaded and the Sync cycle finishes.
// For these types, DataTypeSyncBridge::MergeFullSyncData() will never be
// called (since without downloading all the data, no initial merge is
// possible).
DataTypeSet ApplyUpdatesImmediatelyTypes();

// Types for which `collaboration_id` field in SyncEntity should be provided.
// These types also support `gc_directive` for collaborations to track active
// collaboratons.
DataTypeSet SharedTypes();

// Types triggering a warning when the user signs out and the types have
// unsynced data. The warning offers the user to proceed with sign-out deleting
// any pending account data or abort, depending on the platform.
DataTypeSet TypesRequiringUnsyncedDataCheckOnSignout();

// User types that can be encrypted, which is a subset of UserTypes() and a
// superset of AlwaysEncryptedUserTypes();
DataTypeSet EncryptableUserTypes();

// Determine a data type from the field number of its associated
// EntitySpecifics field.  Returns UNSPECIFIED if the field number is
// not recognized.
DataType GetDataTypeFromSpecificsFieldNumber(int field_number);

namespace internal {
// Obtain data type from field_number and add to data_types if valid.
void GetDataTypeSetFromSpecificsFieldNumberListHelper(DataTypeSet& data_types,
                                                      int field_number);
}  // namespace internal

// Build a DataTypeSet from a list of field numbers. Any unknown field numbers
// are ignored.
template <typename ContainerT>
DataTypeSet GetDataTypeSetFromSpecificsFieldNumberList(
    const ContainerT& field_numbers) {
  DataTypeSet data_types;
  for (int field_number : field_numbers) {
    internal::GetDataTypeSetFromSpecificsFieldNumberListHelper(data_types,
                                                               field_number);
  }
  return data_types;
}

// Return the field number of the EntitySpecifics field associated with
// a data type.
int GetSpecificsFieldNumberFromDataType(DataType data_type);

// Returns a string with application lifetime that represents the name of
// `data_type`.
std::string_view DataTypeToDebugString(DataType data_type);

// Returns a string with application lifetime that is used as the histogram
// suffix for `data_type`.
std::string_view DataTypeToHistogramSuffix(DataType data_type);

// Some histograms take an integer parameter that represents a data type.
// The mapping from DataType to integer is defined here. It defines a
// completely different order than the DataType enum itself. The mapping should
// match the SyncDataTypes mapping from integer to labels defined in enums.xml.
DataTypeForHistograms DataTypeHistogramValue(DataType data_type);

// Returns for every data_type a positive unique integer that is stable over
// time and thus can be used when persisting data.
int DataTypeToStableIdentifier(DataType data_type);

// This returns a string that is stable over time and thus can be used for local
// persistence. It is guaranteed to be lowercase.
std::string_view DataTypeToStableLowerCaseString(DataType data_type);

// Returns the comma-separated string representation of `data_types`.
std::string DataTypeSetToDebugString(DataTypeSet data_types);

// Necessary for compatibility with EXPECT_EQ and the like.
std::ostream& operator<<(std::ostream& out, DataType data_type);

// Necessary for compatibility with EXPECT_EQ and the like.
std::ostream& operator<<(std::ostream& out, DataTypeSet data_type_set);

// Returns a string corresponding to the root tag as exposed in the sync
// protocol as the root entity's ID, which makes the root entity trivially
// distinguishable from regular entities. Note that the existence of a root
// entity in the sync protocol is a legacy artifact, and modern clients ignore
// it except for bookmarks and Nigori. For this reason, the server may or may
// not return the root entity.
std::string DataTypeToProtocolRootTag(DataType data_type);

// Returns true if `data_type` is a real datatype
bool IsRealDataType(DataType data_type);

// Returns true if `data_type` is an act-once type. Act once types drop
// entities after applying them. Drops are deletes that are not synced to other
// clients.
bool IsActOnceDataType(DataType data_type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_DATA_TYPE_H_
