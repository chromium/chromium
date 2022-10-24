// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/ipc_constants.h"

#include "chrome/updater/updater_branding.h"

namespace updater {

// The name of the platform channel used to broker a Mojo connection between the
// client and server.
const char kUpdateServerChannelName[] = PRODUCT_FULLNAME_STRING "ServerChannel";

// The name of the the pipe attached to the Mojo invitation for transmitting an
// UpdateService or UpdateServiceInternal PendingReceiver.
const char kUpdateServerChannelPipeName[] = "UpdateServiceReceiverPipe";

}  // namespace updater
