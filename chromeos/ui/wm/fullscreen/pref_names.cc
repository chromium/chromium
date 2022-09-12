// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/fullscreen/pref_names.h"

namespace chromeos::prefs {

// A list of URLs that are allowed to continue full screen mode after session
// unlock without a notification. To prevent fake login screens, the device
// normally exits full screen mode before locking a session.
const char kKeepFullscreenWithoutNotificationUrlAllowList[] =
    "keep_fullscreen_without_notification_url_allow_list";

}  // namespace chromeos::prefs
