// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/linux/server.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/server/linux/update_service_stub.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/posix/setup.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

AppServerLinux::AppServerLinux() = default;
AppServerLinux::~AppServerLinux() = default;

void AppServerLinux::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  active_duty_stub_ = std::make_unique<UpdateServiceStub>(
      std::move(update_service), updater_scope(),
      base::BindRepeating(&AppServerLinux::TaskStarted, this),
      base::BindRepeating(&AppServerLinux::TaskCompleted, this));
}

void AppServerLinux::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  active_duty_internal_stub_ = std::make_unique<UpdateServiceInternalStub>(
      std::move(update_service_internal), updater_scope(),
      base::BindRepeating(&AppServerLinux::TaskStarted, this),
      base::BindRepeating(&AppServerLinux::TaskCompleted, this));
}

bool AppServerLinux::SwapInNewVersion() {
  // TODO(crbug.com/1276117): Install systemd units.
  return true;
}

bool AppServerLinux::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // There is not a legacy update client for Linux.
  return true;
}

void AppServerLinux::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerLinux>();
}

}  // namespace updater
