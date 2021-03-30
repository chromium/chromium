// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/server.h"

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_server.h"
#import "chrome/updater/app/server/mac/app_server.h"
#include "chrome/updater/app/server/mac/service_delegate.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/setup/setup.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

AppServerMac::AppServerMac()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
AppServerMac::~AppServerMac() = default;

void AppServerMac::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // These delegates need to have a reference to the AppServer. To break the
  // circular reference, we need to reset them.
  update_check_delegate_.reset();
  update_service_internal_delegate_.reset();

  AppServer::Uninitialize();
}

void AppServerMac::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  @autoreleasepool {
    // Sets up a listener and delegate for the
    // CRUUpdateServicingInternal XPC connection.
    update_service_internal_delegate_.reset(
        [[CRUUpdateServiceInternalXPCDelegate alloc]
            initWithUpdateServiceInternal:update_service_internal
                                appServer:scoped_refptr<AppServerMac>(this)]);

    update_service_internal_listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:GetUpdateServiceInternalMachName().get()]);
    update_service_internal_listener_.get().delegate =
        update_service_internal_delegate_.get();

    [update_service_internal_listener_ resume];
  }
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
        initWithMachServiceName:GetUpdateServiceMachName().get()]);
    update_check_listener_.get().delegate = update_check_delegate_.get();

    [update_check_listener_ resume];
  }
}

void AppServerMac::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

bool AppServerMac::SwapRPCInterfaces() {
  return PromoteCandidate(updater_scope()) == setup_exit_codes::kSuccess;
}

void AppServerMac::TaskStarted() {
  main_task_runner_->PostTask(FROM_HERE,
                              BindOnce(&AppServerMac::MarkTaskStarted, this));
}

void AppServerMac::MarkTaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++tasks_running_;
}

void AppServerMac::TaskCompleted() {
  main_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AppServerMac::AcknowledgeTaskCompletion, this),
      base::TimeDelta::FromSeconds(config() ? config()->ServerKeepAliveSeconds()
                                            : kServerKeepAliveSeconds));
}

void AppServerMac::AcknowledgeTaskCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (--tasks_running_ < 1) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AppServerMac::Shutdown, this, 0));
  }
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerMac>();
}

}  // namespace updater
