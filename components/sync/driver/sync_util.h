// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_UTIL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_UTIL_H_

#include <string>

#include "components/version_info/version_info.h"

class GURL;

namespace base {
class CommandLine;
}

namespace syncer {
namespace internal {

// Default sync server URL. Visible for testing.
extern const char* const kSyncServerUrl;

// Sync server URL for dev channel users. Visible for testing.
extern const char* const kSyncDevServerUrl;

// Formats user agent string from system string and channel. Visible for
// testing.
std::string FormatUserAgentForSync(const std::string& system,
                                   version_info::Channel channel);

}  // namespace internal

GURL GetSyncServiceURL(const base::CommandLine& command_line,
                       version_info::Channel channel);

// Helper to construct a user agent string (ASCII) suitable for use by
// the syncapi for any HTTP communication. This string is used by the sync
// backend for classifying client types when calculating statistics.
std::string MakeUserAgentForSync(version_info::Channel channel);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_UTIL_H_
