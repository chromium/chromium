// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_INTERNALS_UTIL_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_INTERNALS_UTIL_H_

#include <memory>
#include <string>

#include "base/types/strong_alias.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace syncer {

class SyncService;

namespace sync_ui_util {

// These strings are used from logs to pull out specific data from sync; we
// don't want these to ever go out of sync between the logs and sync util.
inline constexpr char kIdentityTitle[] = "Identity";
inline constexpr char kDetailsKey[] = "details";

// Resource paths.
// Must match the resource file names.
inline constexpr char kAboutJS[] = "about.js";
inline constexpr char kChromeSyncJS[] = "chrome_sync.js";
inline constexpr char kDataJS[] = "data.js";
inline constexpr char kEventsJS[] = "events.js";
inline constexpr char kSearchJS[] = "search.js";
inline constexpr char kSyncIndexJS[] = "sync_index.js";
inline constexpr char kSyncLogJS[] = "sync_log.js";
inline constexpr char kSyncNodeBrowserJS[] = "sync_node_browser.js";
inline constexpr char kSyncSearchJS[] = "sync_search.js";
inline constexpr char kUserEventsJS[] = "user_events.js";
inline constexpr char kTrafficLogJS[] = "traffic_log.js";
inline constexpr char kInvalidationsJS[] = "invalidations.js";

// Message handlers.
// Must match the constants used in the resource files.
inline constexpr char kGetAllNodes[] = "getAllNodes";
inline constexpr char kRequestDataAndRegisterForUpdates[] =
    "requestDataAndRegisterForUpdates";
inline constexpr char kRequestIncludeSpecificsInitialState[] =
    "requestIncludeSpecificsInitialState";
inline constexpr char kRequestListOfTypes[] = "requestListOfTypes";
inline constexpr char kRequestStart[] = "requestStart";
inline constexpr char kSetIncludeSpecifics[] = "setIncludeSpecifics";
inline constexpr char kTriggerRefresh[] = "triggerRefresh";
inline constexpr char kWriteUserEvent[] = "writeUserEvent";

// Other strings.
// WARNING: Must match the property names used in the resource files.
inline constexpr char kEntityCounts[] = "entityCounts";
inline constexpr char kEntities[] = "entities";
inline constexpr char kNonTombstoneEntities[] = "nonTombstoneEntities";
inline constexpr char kIncludeSpecifics[] = "includeSpecifics";
inline constexpr char kDataType[] = "dataType";
inline constexpr char kOnAboutInfoUpdated[] = "onAboutInfoUpdated";
inline constexpr char kOnEntityCountsUpdated[] = "onEntityCountsUpdated";
inline constexpr char kOnProtocolEvent[] = "onProtocolEvent";
inline constexpr char kOnReceivedIncludeSpecificsInitialState[] =
    "onReceivedIncludeSpecificsInitialState";
inline constexpr char kOnReceivedListOfTypes[] = "onReceivedListOfTypes";
inline constexpr char kTypes[] = "types";
inline constexpr char kOnInvalidationReceived[] = "onInvalidationReceived";

using IncludeSensitiveData =
    base::StrongAlias<class IncludeSensitiveDataTag, bool>;
// This function returns a base::Value::Dict which contains all the information
// required to populate the 'About' tab of chrome://sync-internals.
// Note that |service| may be null.
// If |include_sensitive_data| is false, Personally Identifiable Information
// won't be included in the return value.
base::Value::Dict ConstructAboutInformation(
    IncludeSensitiveData include_sensitive_data,
    SyncService* service,
    const std::string& channel);

}  // namespace sync_ui_util

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_INTERNALS_UTIL_H_
