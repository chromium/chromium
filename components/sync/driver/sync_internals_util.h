// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_INTERNALS_UTIL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_INTERNALS_UTIL_H_

#include <memory>
#include <string>

#include "base/types/strong_alias.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace base {
class DictionaryValue;
}

namespace syncer {

class SyncService;

namespace sync_ui_util {

// These strings are used from logs to pull out specific data from sync; we
// don't want these to ever go out of sync between the logs and sync util.
constexpr inline char kIdentityTitle[] = "Identity";
constexpr inline char kDetailsKey[] = "details";

// Resource paths.
// Must match the resource file names.
constexpr inline char kAboutJS[] = "about.js";
constexpr inline char kChromeSyncJS[] = "chrome_sync.js";
constexpr inline char kDataJS[] = "data.js";
constexpr inline char kEventsJS[] = "events.js";
constexpr inline char kSearchJS[] = "search.js";
constexpr inline char kSyncIndexJS[] = "sync_index.js";
constexpr inline char kSyncLogJS[] = "sync_log.js";
constexpr inline char kSyncNodeBrowserJS[] = "sync_node_browser.js";
constexpr inline char kSyncSearchJS[] = "sync_search.js";
constexpr inline char kUserEventsJS[] = "user_events.js";
constexpr inline char kTrafficLogJS[] = "traffic_log.js";
constexpr inline char kInvalidationsJS[] = "invalidations.js";

// Message handlers.
// Must match the constants used in the resource files.
constexpr inline char kGetAllNodes[] = "getAllNodes";
constexpr inline char kRequestDataAndRegisterForUpdates[] =
    "requestDataAndRegisterForUpdates";
constexpr inline char kRequestIncludeSpecificsInitialState[] =
    "requestIncludeSpecificsInitialState";
constexpr inline char kRequestListOfTypes[] = "requestListOfTypes";
constexpr inline char kRequestStart[] = "requestStart";
constexpr inline char kRequestStopKeepData[] = "requestStopKeepData";
constexpr inline char kRequestStopClearData[] = "requestStopClearData";
constexpr inline char kSetIncludeSpecifics[] = "setIncludeSpecifics";
constexpr inline char kTriggerRefresh[] = "triggerRefresh";
constexpr inline char kWriteUserEvent[] = "writeUserEvent";

// Other strings.
// WARNING: Must match the property names used in the resource files.
constexpr inline char kEntityCounts[] = "entityCounts";
constexpr inline char kEntities[] = "entities";
constexpr inline char kNonTombstoneEntities[] = "nonTombstoneEntities";
constexpr inline char kIncludeSpecifics[] = "includeSpecifics";
constexpr inline char kModelType[] = "modelType";
constexpr inline char kOnAboutInfoUpdated[] = "onAboutInfoUpdated";
constexpr inline char kOnEntityCountsUpdated[] = "onEntityCountsUpdated";
constexpr inline char kOnProtocolEvent[] = "onProtocolEvent";
constexpr inline char kOnReceivedIncludeSpecificsInitialState[] =
    "onReceivedIncludeSpecificsInitialState";
constexpr inline char kOnReceivedListOfTypes[] = "onReceivedListOfTypes";
constexpr inline char kTypes[] = "types";
constexpr inline char kOnInvalidationReceived[] = "onInvalidationReceived";

using IncludeSensitiveData =
    base::StrongAlias<class IncludeSensitiveDataTag, bool>;
// This function returns a DictionaryValue which contains all the information
// required to populate the 'About' tab of chrome://sync-internals.
// Note that |service| may be null.
// If |include_sensitive_data| is false, Personally Identifiable Information
// won't be included in the return value.
std::unique_ptr<base::DictionaryValue> ConstructAboutInformation(
    IncludeSensitiveData include_sensitive_data,
    SyncService* service,
    const std::string& channel);

}  // namespace sync_ui_util

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_INTERNALS_UTIL_H_
