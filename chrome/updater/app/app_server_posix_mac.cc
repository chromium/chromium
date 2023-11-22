// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/app_server_posix.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/mac/setup/keystone.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/util/mac_util.h"

namespace updater {

bool AppServerPosix::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // This is potentially a race condition because the Keystone might be
  // modifying its data when the new updater is trying to read and migrate.
  // See crbug.com/1453460.
  return MigrateKeystoneApps(GetKeystoneFolderPath(updater_scope()).value(),
                             register_callback);
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerPosix>();
}

}  // namespace updater
