// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/linux/server.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

AppServerLinux::AppServerLinux() = default;
AppServerLinux::~AppServerLinux() = default;

// TODO(crbug.com/1276117) - implement.
void AppServerLinux::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  NOTIMPLEMENTED();
}

void AppServerLinux::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  NOTIMPLEMENTED();
}

bool AppServerLinux::SwapInNewVersion() {
  NOTIMPLEMENTED();
  return false;
}

bool AppServerLinux::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  NOTIMPLEMENTED();
  return false;
}

void AppServerLinux::UninstallSelf() {
  NOTIMPLEMENTED();
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerLinux>();
}

}  // namespace updater
