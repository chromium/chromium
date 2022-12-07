// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/server.h"

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_server.h"
#import "chrome/updater/app/server/mac/app_server.h"
#include "chrome/updater/app/server/mac/service_delegate.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/setup/keystone.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/posix/setup.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

AppServerMac::AppServerMac() = default;
AppServerMac::~AppServerMac() = default;

void AppServerMac::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // These delegates need to have a reference to the AppServer. To break the
  // circular reference, we need to reset them.
  active_duty_internal_stub_.reset();
  update_check_delegate_.reset();

  AppServer::Uninitialize();
}

void AppServerMac::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_duty_internal_stub_ = std::make_unique<UpdateServiceInternalStub>(
      std::move(update_service_internal), updater_scope(),
      base::BindRepeating(&AppServerMac::TaskStarted, this),
      base::BindRepeating(&AppServerMac::TaskCompleted, this));
}

void AppServerMac::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  @autoreleasepool {
    // Sets up a listener and delegate for the CRUUpdateServicing XPC
    // connection.
    update_check_delegate_.reset([[CRUUpdateCheckServiceXPCDelegate alloc]
        initWithUpdateService:update_service
                    appServer:scoped_refptr<AppServerMac>(this)]);

    update_check_listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:GetUpdateServiceMachName(updater_scope())
                                    .get()]);
    update_check_listener_.get().delegate = update_check_delegate_.get();

    [update_check_listener_ resume];
  }
}

void AppServerMac::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

bool AppServerMac::SwapInNewVersion() {
  return PromoteCandidate(updater_scope()) == kErrorOk;
}

bool AppServerMac::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // TODO(crbug.com/1250524): This must not run concurrently with Keystone.
  MigrateKeystoneTickets(updater_scope(), register_callback);

  return true;
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerMac>();
}

}  // namespace updater
