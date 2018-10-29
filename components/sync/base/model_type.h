// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
#define COMPONENTS_SYNC_BASE_MODEL_TYPE_H_

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>

#include "base/logging.h"
#include "components/reading_list/features/reading_list_buildflags.h"
#include "components/sync/base/enum_set.h"

namespace base {
class ListValue;
class Value;
}

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
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
// SyncModelType and SyncModelTypeByMacro histogram suffixes in histograms.xml
enum ModelType {
  // Object type unknown.  Objects may transition through
  // the unknown state during their initial creation, before
  // their properties are set.  After deletion, object types
  // are generally preserved.
  UNSPECIFIED,
  // A permanent folder whose children may be of mixed
  // datatypes (e.g. the "Google Chrome" folder).
  TOP_LEVEL_FOLDER,

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

  // A preference object.
  PREFERENCES,
  // A password object.
  PASSWORDS,
  // An AutofillProfile Object
  AUTOFILL_PROFILE,
  // An autofill object.
  AUTOFILL,
  // Credit cards and addresses synced from the user's account. These are
  // read-only on the client.
  AUTOFILL_WALLET_DATA,
  // Usage counts and last use dates for Wallet cards and addresses. This data
  // is both readable and writable.
  AUTOFILL_WALLET_METADATA,
  // A themes object.
  THEMES,
  // A typed_url object.
  TYPED_URLS,
  // An extension object.
  EXTENSIONS,
  // An object representing a custom search engine.
  SEARCH_ENGINES,
  // An object representing a browser session.
  SESSIONS,
  // An app object.
  APPS,
  // An app setting from the extension settings API.
  APP_SETTINGS,
  // An extension setting from the extension settings API.
  EXTENSION_SETTINGS,
  // App notifications. Deprecated.
  APP_NOTIFICATIONS,
  // History delete directives.
  HISTORY_DELETE_DIRECTIVES,
  // Synced push notifications. Deprecated.
  SYNCED_NOTIFICATIONS,
  // Synced Notification app info. Deprecated.
  SYNCED_NOTIFICATION_APP_INFO,
  // Custom spelling dictionary.
  DICTIONARY,
  // Favicon images.
  FAVICON_IMAGES,
  // Favicon tracking information.
  FAVICON_TRACKING,
  // Client-specific metadata, synced before other user types.
  DEVICE_INFO,
  // These preferences are synced before other user types and are never
  // encrypted.
  PRIORITY_PREFERENCES,
  // Supervised user settings. Cannot be encrypted.
  SUPERVISED_USER_SETTINGS,
  // Deprecated supervised user types that are not used anymore.
  DEPRECATED_SUPERVISED_USERS,
  DEPRECATED_SUPERVISED_USER_SHARED_SETTINGS,
  // Distilled articles.
  DEPRECATED_ARTICLES,
  // App List items
  APP_LIST,
  // WiFi credentials. Each item contains the information for connecting to one
  // WiFi network. This includes, e.g., network name and password.
  WIFI_CREDENTIALS,
  // Supervised user whitelists. Each item contains a CRX ID (like an extension
  // ID) and a name.
  SUPERVISED_USER_WHITELISTS,
  // ARC Package items.
  ARC_PACKAGE,
  // Printer device information.
  PRINTERS,
  // Reading list items.
  READING_LIST,
  // Commit only user events.
  USER_EVENTS,
  // Shares in project Mountain.
  MOUNTAIN_SHARES,
  // Commit only user consents.
  USER_CONSENTS,

  // ---- Proxy types ----
  // Proxy types are excluded from the sync protocol, but are still considered
  // real user types. By convention, we prefix them with 'PROXY_' to distinguish
  // them from normal protocol types.

  // Tab sync. This is a placeholder type, so that Sessions can be implicitly
  // enabled for history sync and tabs sync.
  PROXY_TABS,
  FIRST_PROXY_TYPE = PROXY_TABS,
  LAST_PROXY_TYPE = PROXY_TABS,
  LAST_USER_MODEL_TYPE = PROXY_TABS,

  // ---- Control Types ----
  // An object representing a set of Nigori keys.
  NIGORI,
  FIRST_CONTROL_MODEL_TYPE = NIGORI,
  // Flags to enable experimental features.
  EXPERIMENTS,
  LAST_CONTROL_MODEL_TYPE = EXPERIMENTS,
  LAST_REAL_MODEL_TYPE = LAST_CONTROL_MODEL_TYPE,

  MODEL_TYPE_COUNT,
};

using ModelTypeSet =
    EnumSet<ModelType, FIRST_REAL_MODEL_TYPE, LAST_REAL_MODEL_TYPE>;
using FullModelTypeSet = EnumSet<ModelType, UNSPECIFIED, LAST_REAL_MODEL_TYPE>;
using ModelTypeNameMap = std::map<ModelType, const char*>;

inline ModelType ModelTypeFromInt(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, MODEL_TYPE_COUNT);
  return static_cast<ModelType>(i);
}

// Used to mark the type of EntitySpecifics that has no actual data.
void AddDefaultFieldValue(ModelType type, sync_pb::EntitySpecifics* specifics);

// Extract the model type of a SyncEntity protocol buffer.  ModelType is a
// local concept: the enum is not in the protocol.  The SyncEntity's ModelType
// is inferred from the presence of particular datatype field in the
// entity specifics.
ModelType GetModelType(const sync_pb::SyncEntity& sync_entity);

// Extract the model type from an EntitySpecifics field.  Note that there
// are some ModelTypes (like TOP_LEVEL_FOLDER) that can't be inferred this way;
// prefer using GetModelType where possible.
ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics);

// Notes:
// 1) This list must contain exactly the same elements as the set returned by
//    UserSelectableTypes().
// 2) This list must be in the same order as the respective values in the
//    ModelType enum.
constexpr const char* kUserSelectableDataTypeNames[] = {
    "bookmarks",   "preferences", "passwords",  "autofill",
    "themes",      "typedUrls",   "extensions", "apps",
#if BUILDFLAG(ENABLE_READING_LIST)
    "readingList",
#endif
    "userEvents",  "tabs"};

// Protocol types are those types that have actual protocol buffer
// representations. This distinguishes them from Proxy types, which have no
// protocol representation and are never sent to the server.
constexpr ModelTypeSet ProtocolTypes() {
  return ModelTypeSet(
      BOOKMARKS, PREFERENCES, PASSWORDS, AUTOFILL_PROFILE, AUTOFILL,
      AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_METADATA, THEMES, TYPED_URLS,
      EXTENSIONS, SEARCH_ENGINES, SESSIONS, APPS, APP_SETTINGS,
      EXTENSION_SETTINGS, APP_NOTIFICATIONS, HISTORY_DELETE_DIRECTIVES,
      SYNCED_NOTIFICATIONS, SYNCED_NOTIFICATION_APP_INFO, DICTIONARY,
      FAVICON_IMAGES, FAVICON_TRACKING, DEVICE_INFO, PRIORITY_PREFERENCES,
      SUPERVISED_USER_SETTINGS, DEPRECATED_SUPERVISED_USERS,
      DEPRECATED_SUPERVISED_USER_SHARED_SETTINGS, DEPRECATED_ARTICLES, APP_LIST,
      WIFI_CREDENTIALS, SUPERVISED_USER_WHITELISTS, ARC_PACKAGE, PRINTERS,
      READING_LIST, USER_EVENTS, NIGORI, EXPERIMENTS, MOUNTAIN_SHARES,
      USER_CONSENTS);
}

// These are the normal user-controlled types. This is to distinguish from
// ControlTypes which are always enabled.  Note that some of these share a
// preference flag, so not all of them are individually user-selectable.
constexpr ModelTypeSet UserTypes() {
  return ModelTypeSet::FromRange(FIRST_USER_MODEL_TYPE, LAST_USER_MODEL_TYPE);
}

// User types, which are not user-controlled.
constexpr ModelTypeSet AlwaysPreferredUserTypes() {
  return ModelTypeSet(DEVICE_INFO, USER_CONSENTS);
}

// These are the user-selectable data types.
constexpr ModelTypeSet UserSelectableTypes() {
  return ModelTypeSet(BOOKMARKS, PREFERENCES, PASSWORDS, AUTOFILL, THEMES,
                      TYPED_URLS, EXTENSIONS, APPS,
#if BUILDFLAG(ENABLE_READING_LIST)
                      READING_LIST,
#endif
                      USER_EVENTS, PROXY_TABS);
}

constexpr bool IsUserSelectableType(ModelType model_type) {
  return UserSelectableTypes().Has(model_type);
}

// This is the subset of UserTypes() that have priority over other types.  These
// types are synced before other user types and are never encrypted.
constexpr ModelTypeSet PriorityUserTypes() {
  return ModelTypeSet(DEVICE_INFO, PRIORITY_PREFERENCES);
}

// Proxy types are placeholder types for handling implicitly enabling real
// types. They do not exist at the server, and are simply used for
// UI/Configuration logic.
constexpr ModelTypeSet ProxyTypes() {
  return ModelTypeSet::FromRange(FIRST_PROXY_TYPE, LAST_PROXY_TYPE);
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
// - All change processing occurs on the sync thread (GROUP_PASSIVE).
constexpr ModelTypeSet ControlTypes() {
  return ModelTypeSet::FromRange(FIRST_CONTROL_MODEL_TYPE,
                                 LAST_CONTROL_MODEL_TYPE);
}

// Returns true if this is a control type.
//
// See comment above for more information on what makes these types special.
constexpr bool IsControlType(ModelType model_type) {
  return ControlTypes().Has(model_type);
}

// Core types are those data types used by sync's core functionality (i.e. not
// user data types). These types are always enabled, and include ControlTypes().
//
// The set of all core types.
constexpr ModelTypeSet CoreTypes() {
  return ModelTypeSet(NIGORI, EXPERIMENTS, SUPERVISED_USER_SETTINGS,
                      SYNCED_NOTIFICATIONS, SYNCED_NOTIFICATION_APP_INFO,
                      SUPERVISED_USER_WHITELISTS);
}
// Those core types that have high priority (includes ControlTypes()).
constexpr ModelTypeSet PriorityCoreTypes() {
  return ModelTypeSet(NIGORI, EXPERIMENTS, SUPERVISED_USER_SETTINGS);
}

// Types that may commit data, but should never be included in a GetUpdates.
constexpr ModelTypeSet CommitOnlyTypes() {
  return ModelTypeSet(USER_EVENTS, USER_CONSENTS);
}

ModelTypeNameMap GetUserSelectableTypeNameMap();

// This is the subset of UserTypes() that can be encrypted.
ModelTypeSet EncryptableUserTypes();

// Determine a model type from the field number of its associated
// EntitySpecifics field.  Returns UNSPECIFIED if the field number is
// not recognized.
//
// If you're putting the result in a ModelTypeSet, you should use the
// following pattern:
//
//   ModelTypeSet model_types;
//   // Say we're looping through a list of items, each of which has a
//   // field number.
//   for (...) {
//     int field_number = ...;
//     ModelType model_type =
//         GetModelTypeFromSpecificsFieldNumber(field_number);
//     if (!IsRealDataType(model_type)) {
//       DLOG(WARNING) << "Unknown field number " << field_number;
//       continue;
//     }
//     model_types.Put(model_type);
//   }
ModelType GetModelTypeFromSpecificsFieldNumber(int field_number);

// Return the field number of the EntitySpecifics field associated with
// a model type.
int GetSpecificsFieldNumberFromModelType(ModelType model_type);

FullModelTypeSet ToFullModelTypeSet(ModelTypeSet in);

// TODO(sync): The functions below badly need some cleanup.

// Returns a string with application lifetime that represents the name of
// |model_type|.
const char* ModelTypeToString(ModelType model_type);

// Returns a string with application lifetime that is used as the histogram
// suffix for |model_type|.
const char* ModelTypeToHistogramSuffix(ModelType model_type);

// Some histograms take an integer parameter that represents a model type.
// The mapping from ModelType to integer is defined here. It defines a
// completely different order than the ModelType enum itself. The mapping should
// match the SyncModelTypes mapping from integer to labels defined in enums.xml.
int ModelTypeToHistogramInt(ModelType model_type);

// Returns for every model_type a positive unique integer that is stable over
// time and thus can be used when persisting data.
int ModelTypeToStableIdentifier(ModelType model_type);

// Handles all model types, and not just real ones.
std::unique_ptr<base::Value> ModelTypeToValue(ModelType model_type);

// Returns the ModelType corresponding to the name |model_type_string|.
ModelType ModelTypeFromString(const std::string& model_type_string);

// Returns the comma-separated string representation of |model_types|.
std::string ModelTypeSetToString(ModelTypeSet model_types);

// Necessary for compatibility with EXPECT_EQ and the like.
std::ostream& operator<<(std::ostream& out, ModelTypeSet model_type_set);

// Returns the set of comma-separated model types from |model_type_string|.
ModelTypeSet ModelTypeSetFromString(const std::string& model_type_string);

std::unique_ptr<base::ListValue> ModelTypeSetToValue(ModelTypeSet model_types);

// Returns a string corresponding to the syncable tag for this datatype.
std::string ModelTypeToRootTag(ModelType type);

// Returns root_tag for |model_type| in ModelTypeInfo.
// Difference with ModelTypeToRootTag(), this just simply returns root_tag in
// ModelTypeInfo.
const char* GetModelTypeRootTag(ModelType model_type);

// Convert a real model type to a notification type (used for
// subscribing to server-issued notifications).  Returns true iff
// |model_type| was a real model type and |notification_type| was
// filled in.
bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type);

// Converts a notification type to a real model type.  Returns true
// iff |notification_type| was the notification type of a real model
// type and |model_type| was filled in.
bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type);

// Returns true if |model_type| is a real datatype
bool IsRealDataType(ModelType model_type);

// Returns true if |model_type| is a proxy type
bool IsProxyType(ModelType model_type);

// Returns true if |model_type| is an act-once type. Act once types drop
// entities after applying them. Drops are deletes that are not synced to other
// clients.
// TODO(haitaol): Make entries of act-once data types immutable.
bool IsActOnceDataType(ModelType model_type);

// Returns true if |model_type| requires its root folder to be explicitly
// created on the server during initial sync.
bool IsTypeWithServerGeneratedRoot(ModelType model_type);

// Returns true if root folder for |model_type| is created on the client when
// that type is initially synced.
bool IsTypeWithClientGeneratedRoot(ModelType model_type);

// Returns true if |model_type| supports parent-child hierarchy or entries.
bool TypeSupportsHierarchy(ModelType model_type);

// Returns true if |model_type| supports ordering of sibling entries.
bool TypeSupportsOrdering(ModelType model_type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
