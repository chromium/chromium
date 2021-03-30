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
extern const char kIdentityTitle[];
extern const char kDetailsKey[];

// Resource paths.
// Must match the resource file names.
extern const char kAboutJS[];
extern const char kChromeSyncJS[];
extern const char kDataJS[];
extern const char kEventsJS[];
extern const char kSearchJS[];
extern const char kSyncIndexJS[];
extern const char kSyncLogJS[];
extern const char kSyncNodeBrowserJS[];
extern const char kSyncSearchJS[];
extern const char kUserEventsJS[];
extern const char kTrafficLogJS[];
extern const char kInvalidationsJS[];

// Message handlers.
// Must match the constants used in the resource files.
extern const char kGetAllNodes[];
extern const char kRequestDataAndRegisterForUpdates[];
extern const char kRequestIncludeSpecificsInitialState[];
extern const char kRequestListOfTypes[];
extern const char kRequestStart[];
extern const char kRequestStopKeepData[];
extern const char kRequestStopClearData[];
extern const char kSetIncludeSpecifics[];
extern const char kTriggerRefresh[];
extern const char kWriteUserEvent[];

// Other strings.
// WARNING: Must match the property names used in the resource files.
extern const char kEntityCounts[];
extern const char kEntities[];
extern const char kNonTombstoneEntities[];
extern const char kIncludeSpecifics[];
extern const char kModelType[];
extern const char kOnAboutInfoUpdated[];
extern const char kOnEntityCountsUpdated[];
extern const char kOnProtocolEvent[];
extern const char kOnReceivedIncludeSpecificsInitialState[];
extern const char kOnReceivedListOfTypes[];
extern const char kTypes[];
extern const char kOnInvalidationReceived[];

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
