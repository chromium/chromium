// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_H_

#include "chrome/updater/updater_scope.h"

namespace updater {
// Start the update service in a platform-specific way. Returns false if the
// service could not be reached.
[[nodiscard]] bool DialUpdateService(UpdaterScope scope);
}  // namespace updater
#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_H_
