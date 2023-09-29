// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
#define COMPONENTS_SYNC_BASE_MODEL_TYPE_H_

#include <iosfwd>
#include <map>
#include <memory>
#include <string>

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
//
// |kModelTypeInfoMap| struct entries are in the same order as their definition
// in ModelType enum. When you make changes in ModelType enum, don't forget to
// update the |kModelTypeInfoMap| struct in model_type.cc and also the
// SyncModelType histogram suffix in histograms.xml
enum ModelType {
  // Object type unknown. This may be used when:
  // a) The client received *valid* data from a data type which this version
  // is unaware of (only present in versions newer than this one, or present
  // in older versions but removed since).
  // b) The client received invalid data from the server due to some error.
  // c) A data object was just created, in which case this is a temporary state.
  UNSPECIFIED,

  // ------------------------------------ Start of "real" model types.
  // The model types declared before here are somewhat special, as they
  // they do not correspond to any browser data model.  The remaining types
  // are bona fide model types; all have a related browser data model and
  // can be represented in the protocol using a specific Message type in the
  // EntitySpecifics protocol buffer.
  //
  // A bookmark folder or a bookmark URL object.
  BOOKMARKS,
  FIRST_USER_MODEL_TYPE = BOOKMARKS,  // Declared 2nd, for debugger prettiness.
  FIRST_REAL_MODEL_TYPE = FIRST_USER_MODEL_TYPE,

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
  // Credit cards and addresses from the user's account. These are read-only on
  // the client.
  AUTOFILL_WALLET_DATA,
  // Usage counts and last use dates for Wallet cards and addresses. This data
  // is both readable and writable.
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
  // for "Tabs" (depending on PROXY_TABS).
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
  // Supervised user settings. Cannot be encrypted.
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
  // Segmentation data.
  SEGMENTATION,
  // Tabs sent between devices.
  SEND_TAB_TO_SELF,
  // Commit only security events.
  SECURITY_EVENTS,
  // Wi-Fi network configurations + credentials
  WIFI_CONFIGURATIONS,
  // A web app object.
  WEB_APPS,
  // OS-specific preferences (a.k.a. "OS settings"). Chrome OS only.
  OS_PREFERENCES,
  // Synced before other user types. Never encrypted. Chrome OS only.
  OS_PRIORITY_PREFERENCES,
  // Commit only sharing message object.
  SHARING_MESSAGE,
  // A workspace desk saved by user. Chrome OS only.
  WORKSPACE_DESK,
  // Synced history. An entity roughly corresponds to a navigation.
  HISTORY,
  // Trusted Authorization Servers for printers. ChromeOS only.
  PRINTERS_AUTHORIZATION_SERVERS,
  // Contact information from the Google Address Storage.
  CONTACT_INFO,
  // A tab group saved by a user. Currently only supported on desktop platforms
  // (Linux, Mac, Windows, ChromeOS).
  SAVED_TAB_GROUP,

  // Power bookmarks are features associated with bookmarks(i.e. notes, price
  // tracking). Their life cycle are synced with bookmarks.
  POWER_BOOKMARK,

  // WebAuthn credentials, more commonly known as passkeys.
  WEBAUTHN_CREDENTIAL,

  // Invitations for sending passwords. Outgoing invitation from one user will
  // become an incoming one for another.
  INCOMING_PASSWORD_SHARING_INVITATION,
  OUTGOING_PASSWORD_SHARING_INVITATION,

  // Proxy types are excluded from the sync protocol, but are still considered
  // real user types. By convention, we prefix them with 'PROXY_' to distinguish
  // them from normal protocol types.
  //
  // Tab sync. This is a placeholder type, which implicitly enables Sessions
  // for tabs sync.
  // TODO(crbug.com/1365291): Now that TYPED_URLS is gone, it should be possible
  // to remove this type, and the whole concept of "proxy types".
  PROXY_TABS,
  LAST_USER_MODEL_TYPE = PROXY_TABS,

  // ---- Control Types ----
  // An object representing a set of Nigori keys.
  NIGORI,
  LAST_REAL_MODEL_TYPE = NIGORI,

  // NEW ENTRIES MUST BE ADDED ABOVE THIS.
  LAST_ENTRY = LAST_REAL_MODEL_TYPE,
};

using ModelTypeSet =
    base::EnumSet<ModelType, FIRST_REAL_MODEL_TYPE, LAST_REAL_MODEL_TYPE>;

constexpr int GetNumModelTypes() {
  return static_cast<int>(ModelType::LAST_ENTRY) + 1;
}

// A version of the ModelType enum for use in histograms. ModelType does not
// have stable values (e.g. new ones may be inserted in the middle), so it can't
// be recorded directly.
// Instead of using entries from this enum directly, you'll usually want to get
// them via ModelTypeHistogramValue(model_type).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When you add a new entry or when you
// deprecate an existing one, also update SyncModelTypes in enums.xml and
// SyncModelType suffix in histograms.xml.
enum class ModelTypeForHistograms {
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
  // kFaviconImages = 23,
  // kFaviconTracking = 24,
  kProxyTabs = 25,
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
  kSegmentation = 55,
  kSavedTabGroups = 56,
  kPowerBookmark = 57,
  kWebAuthnCredentials = 58,
  kIncomingPasswordSharingInvitations = 59,
  kOutgoingPasswordSharingInvitations = 60,
  kAutofillWalletCredential = 61,
  kMaxValue = kAutofillWalletCredential,
};

// Used to mark the type of EntitySpecifics that has no actual data.
void AddDefaultFieldValue(ModelType type, sync_pb::EntitySpecifics* specifics);

// Extract the model type from an EntitySpecifics field. ModelType is a
// local concept: the enum is not in the protocol.
ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics);

// Protocol types are those types that have actual protocol buffer
// representations. This distinguishes them from Proxy types, which have no
// protocol representation and are never sent to the server.
constexpr ModelTypeSet ProtocolTypes() {
  return {BOOKMARKS,
          PREFERENCES,
          PASSWORDS,
          AUTOFILL_PROFILE,
          AUTOFILL,
          AUTOFILL_WALLET_CREDENTIAL,
          AUTOFILL_WALLET_DATA,
          AUTOFILL_WALLET_METADATA,
          AUTOFILL_WALLET_OFFER,
          AUTOFILL_WALLET_USAGE,
          THEMES,
          EXTENSIONS,
          SEARCH_ENGINES,
          SESSIONS,
          APPS,
          APP_SETTINGS,
          EXTENSION_SETTINGS,
          HISTORY_DELETE_DIRECTIVES,
          DICTIONARY,
          DEVICE_INFO,
          PRIORITY_PREFERENCES,
          SUPERVISED_USER_SETTINGS,
          APP_LIST,
          ARC_PACKAGE,
          PRINTERS,
          READING_LIST,
          USER_EVENTS,
          NIGORI,
          USER_CONSENTS,
          SEND_TAB_TO_SELF,
          SECURITY_EVENTS,
          WEB_APPS,
          WIFI_CONFIGURATIONS,
          OS_PREFERENCES,
          OS_PRIORITY_PREFERENCES,
          SHARING_MESSAGE,
          WORKSPACE_DESK,
          HISTORY,
          PRINTERS_AUTHORIZATION_SERVERS,
          CONTACT_INFO,
          SAVED_TAB_GROUP,
          POWER_BOOKMARK,
          WEBAUTHN_CREDENTIAL,
          INCOMING_PASSWORD_SHARING_INVITATION,
          OUTGOING_PASSWORD_SHARING_INVITATION};
}

// These are the normal user-controlled types. This is to distinguish from
// ControlTypes which are always enabled.  Note that some of these share a
// preference flag, so not all of them are individually user-selectable.
constexpr ModelTypeSet UserTypes() {
  return ModelTypeSet::FromRange(FIRST_USER_MODEL_TYPE, LAST_USER_MODEL_TYPE);
}

// User types which are not user-controlled.
constexpr ModelTypeSet AlwaysPreferredUserTypes() {
  return {DEVICE_INFO,
          USER_CONSENTS,
          SECURITY_EVENTS,
          SEND_TAB_TO_SELF,
          SUPERVISED_USER_SETTINGS,
          SHARING_MESSAGE};
}

// User types which are always encrypted.
constexpr ModelTypeSet AlwaysEncryptedUserTypes() {
  // If you add a new model type here that is conceptually different from a
  // password, make sure you audit UI code that refers to these types as
  // passwords, e.g. consumers of IsEncryptEverythingEnabled().
  return {AUTOFILL_WALLET_CREDENTIAL, PASSWORDS, WIFI_CONFIGURATIONS};
}

// This is the subset of UserTypes() that have priority over other types. These
// types are synced before other user types (both for get_updates and commits).
// This mostly matters during initial sync, since priority types can become
// active before all the data for non-prio types has been downloaded (which may
// be a lot of data).
constexpr ModelTypeSet HighPriorityUserTypes() {
  return {
      // The "Send to Your Devices" feature needs fast updating of the list of
      // your devices and also fast sending of the actual messages.
      DEVICE_INFO, SHARING_MESSAGE,
      // For supervised users, it is important to quickly deliver changes in
      // settings and in allowed sites to the supervised user.
      SUPERVISED_USER_SETTINGS,
      // These are by definition preferences for which it is important that the
      // client picks them up quickly (also because these can get changed
      // server-side). For example, such a pref could control whether a
      // non-priority type gets enabled (Wallet has such a pref).
      PRIORITY_PREFERENCES, OS_PRIORITY_PREFERENCES,
      // Speed matters for the user experience when sync gets enabled directly
      // in the creation flow for a new profile. If the user has no theme in
      // their sync data, the browser offers a theme customization bubble which
      // should appear soon after opening the browser.
      THEMES};
}

// This is the subset of UserTypes() that have a *lower* priority than other
// types. These types are synced only after all other user types (both for
// get_updates and commits). This mostly matters during initial sync, since
// high-priority and regular types can become active before all the data for
// low-priority types has been downloaded (which may be a lot of data).
constexpr ModelTypeSet LowPriorityUserTypes() {
  return {
      // Downloading History may take a while, but should not block the download
      // of other data types.
      HISTORY,
      // User Events should not block or delay commits for other data types.
      USER_EVENTS,
      // Incoming password sharing invitations must be processed after
      // Passwords data type to prevent storing incoming passwords locally first
      // and overwriting the remote password during conflict resolution.
      INCOMING_PASSWORD_SHARING_INVITATION};
}

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
constexpr ModelTypeSet ControlTypes() {
  return {NIGORI};
}

// Types that may commit data, but should never be included in a GetUpdates.
// These are never encrypted.
constexpr ModelTypeSet CommitOnlyTypes() {
  return {USER_EVENTS, USER_CONSENTS, SECURITY_EVENTS, SHARING_MESSAGE,
          OUTGOING_PASSWORD_SHARING_INVITATION};
}

// Types for which downloaded updates are applied immediately, before all
// updates are downloaded and the Sync cycle finishes.
// For these types, ModelTypeSyncBridge::MergeFullSyncData() will never be
// called (since without downloading all the data, no initial merge is
// possible).
constexpr ModelTypeSet ApplyUpdatesImmediatelyTypes() {
  return {HISTORY};
}

// User types that can be encrypted, which is a subset of UserTypes() and a
// superset of AlwaysEncryptedUserTypes();
ModelTypeSet EncryptableUserTypes();

// Determine a model type from the field number of its associated
// EntitySpecifics field.  Returns UNSPECIFIED if the field number is
// not recognized.
ModelType GetModelTypeFromSpecificsFieldNumber(int field_number);

namespace internal {
// Obtain model type from field_number and add to model_types if valid.
void GetModelTypeSetFromSpecificsFieldNumberListHelper(
    ModelTypeSet& model_types,
    int field_number);
}  // namespace internal

// Build a ModelTypeSet from a list of field numbers. Any unknown field numbers
// are ignored.
template <typename ContainerT>
ModelTypeSet GetModelTypeSetFromSpecificsFieldNumberList(
    const ContainerT& field_numbers) {
  ModelTypeSet model_types;
  for (int field_number : field_numbers) {
    internal::GetModelTypeSetFromSpecificsFieldNumberListHelper(model_types,
                                                                field_number);
  }
  return model_types;
}

// Return the field number of the EntitySpecifics field associated with
// a model type.
int GetSpecificsFieldNumberFromModelType(ModelType model_type);

// Returns a string with application lifetime that represents the name of
// |model_type|.
const char* ModelTypeToDebugString(ModelType model_type);

// Returns a string with application lifetime that is used as the histogram
// suffix for |model_type|.
const char* ModelTypeToHistogramSuffix(ModelType model_type);

// Some histograms take an integer parameter that represents a model type.
// The mapping from ModelType to integer is defined here. It defines a
// completely different order than the ModelType enum itself. The mapping should
// match the SyncModelTypes mapping from integer to labels defined in enums.xml.
ModelTypeForHistograms ModelTypeHistogramValue(ModelType model_type);

// Returns for every model_type a positive unique integer that is stable over
// time and thus can be used when persisting data.
int ModelTypeToStableIdentifier(ModelType model_type);

// Returns the comma-separated string representation of |model_types|.
std::string ModelTypeSetToDebugString(ModelTypeSet model_types);

// Necessary for compatibility with EXPECT_EQ and the like.
std::ostream& operator<<(std::ostream& out, ModelTypeSet model_type_set);

// Returns a string corresponding to the root tag as exposed in the sync
// protocol as the root entity's ID, which makes the root entity trivially
// distinguishable from regular entities. Note that the existence of a root
// entity in the sync protocol is a legacy artifact, and modern clients ignore
// it except for bookmarks and Nigori. For this reason, the server may or may
// not return the root entity.
std::string ModelTypeToProtocolRootTag(ModelType model_type);

// As opposed to ModelTypeToProtocolRootTag(), this returns a string that isn't
// exposed in the sync protocol, but that is still stable and thus can be used
// for local persistence. It is guaranteed to be lowercase.
const char* GetModelTypeLowerCaseRootTag(ModelType model_type);

// Returns true if |model_type| is a real datatype
bool IsRealDataType(ModelType model_type);

// Returns true if |model_type| is an act-once type. Act once types drop
// entities after applying them. Drops are deletes that are not synced to other
// clients.
bool IsActOnceDataType(ModelType model_type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
