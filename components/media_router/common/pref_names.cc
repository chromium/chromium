// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/pref_names.h"

namespace media_router {
namespace prefs {

// Whether the enterprise policy allows Cast devices on all IPs.
const char kMediaRouterCastAllowAllIPs[] = "media_router.cast_allow_all_ips";
// Whether or not the user has enabled Media Remoting. Defaults to true.
const char kMediaRouterMediaRemotingEnabled[] =
    "media_router.media_remoting.enabled";
// The per-profile randomly generated token to include with the hash when
// externalizing MediaSink IDs.
const char kMediaRouterReceiverIdHashToken[] =
    "media_router.receiver_id_hash_token";
// Whether or not the user has enabled to show Cast sessions started by
// other devices on the same network. This change only affects the Zenith
// dialog. Defaults to true.
const char kMediaRouterShowCastSessionsStartedByOtherDevices[] =
    "media_router.show_cast_sessions_started_by_other_devices.enabled";
// A list of website origins on which the user has chosen to use tab mirroring.
const char kMediaRouterTabMirroringSources[] =
    "media_router.tab_mirroring_sources";

}  // namespace prefs
}  // namespace media_router
