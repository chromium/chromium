// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PREF_NAMES_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PREF_NAMES_H_

namespace media_router {
namespace prefs {

// Local State
extern const char kMediaRouterCastAllowAllIPs[];
extern const char kSuppressLocalDiscoveryPermissionError[];

// Profile Prefs
extern const char kMediaRouterMediaRemotingEnabled[];
extern const char kMediaRouterReceiverIdHashToken[];
extern const char kMediaRouterShowCastSessionsStartedByOtherDevices[];

}  // namespace prefs
}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PREF_NAMES_H_
