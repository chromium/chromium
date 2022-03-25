// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/pref_names.h"

namespace media_router {
namespace prefs {

// Whether or not the user has enabled Media Remoting. Defaults to true.
const char kMediaRouterMediaRemotingEnabled[] =
    "media_router.media_remoting.enabled";
// A list of website origins on which the user has chosen to use tab mirroring.
const char kMediaRouterTabMirroringSources[] =
    "media_router.tab_mirroring_sources";
// Whether or not the user has enabled to show Cast sessions started by
// other devices on the same network. This change only affects the Zenith
// dialog. Defaults to true.
const char kMediaRouterShowCastSessionsStartedByOtherDevices[] =
    "media_router.show_cast_sessions_started_by_other_devices.enabled";

}  // namespace prefs
}  // namespace media_router
