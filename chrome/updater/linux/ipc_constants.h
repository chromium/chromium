// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_
#define CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_

namespace updater {

// The name of the platform channel used to broker a Mojo connection between the
// client and server.
extern const char kUpdateServerChannelName[];

// The name of the the pipe attached to the Mojo invitation for transmitting an
// UpdateService or UpdateServiceInternal PendingReceiver.
extern const char kUpdateServerChannelPipeName[];

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_IPC_CONSTANTS_H_
