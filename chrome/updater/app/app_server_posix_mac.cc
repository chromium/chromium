// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/app_server_posix.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
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

void AppServerPosix::RepairUpdater(UpdaterScope scope, bool is_internal) {
  // Repair broken ksadmin shims - Chrome M119 and before can delete them
  // during user->system promotion.
  std::optional<base::FilePath> ksadmin_path = GetKSAdminPath(scope);
  if (ksadmin_path && !base::PathExists(*ksadmin_path)) {
    VLOG(2) << "Reinstalling Keystone shims.";
    InstallKeystone(scope);
  }
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerPosix>();
}

}  // namespace updater
