// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_PREF_NAMES_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_PREF_NAMES_H_

namespace network_time::prefs {

// Stores a pair of local time and corresponding network time to bootstrap
// network time tracker when browser starts.
inline constexpr char kNetworkTimeMapping[] =
    "network_time.network_time_mapping";

// Stores a boolean indicating whether network time queries should be enabled.
inline constexpr char kNetworkTimeQueriesEnabled[] =
    "network_time.network_time_queries_enabled";

}  // namespace network_time::prefs

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_PREF_NAMES_H_
