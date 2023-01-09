// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/posix/app_server_posix.h"

#include "base/functional/callback.h"
#include "chrome/updater/registration_data.h"

namespace updater {

bool AppServerPosix::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // There is not a legacy update client for Linux.
  return true;
}

}  // namespace updater
