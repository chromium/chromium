// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_BLOCK_CHECK_H_
#define CHROME_UPDATER_UPDATE_BLOCK_CHECK_H_

#include "base/functional/callback_forward.h"
#include "chrome/updater/update_service.h"

namespace updater {

// Checks if the update should be blocked because the device is using a metered
// network. This only blocks background updates. Foreground updates are still
// allowed to continue. This prevents the updater from spending the network
// bandwidth of a metered network without the user knowing, and also respects
// Windows compliance with regards to metered networks. This currently only
// applies to Win10 devices.
// `priority` indicates if the update is foreground or background.
// `callback` will be called once the function is done and will receive a bool
// that indicates if the update should be blocked.
void ShouldBlockUpdateForMeteredNetwork(
    UpdateService::Priority priority,
    base::OnceCallback<void(bool)> callback);

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_BLOCK_CHECK_H_
