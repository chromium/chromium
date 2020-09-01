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
#include "chrome/updater/control_service_in_process.h"
#include "chrome/updater/mac/setup/setup.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {

AppServerMac::AppServerMac()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
AppServerMac::~AppServerMac() = default;

void AppServerMac::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // These delegates need to have a reference to the AppServer. To break the
  // circular reference, we need to reset them.
  update_check_delegate_.reset();
  control_service_delegate_.reset();

  AppServer::Uninitialize();
}

void AppServerMac::ActiveDuty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(kServerServiceSwitch)) {
    LOG(ERROR) << "Command line is missing " << kServerServiceSwitch
               << " switch.";
    return;
  }
  std::string service = command_line.GetSwitchValueASCII(kServerServiceSwitch);

  if (service == kServerControlServiceSwitchValue) {
    @autoreleasepool {
      // Sets up a listener and delegate for the CRUControlling XPC connection.
      control_service_delegate_.reset([[CRUControlServiceXPCDelegate alloc]
          initWithControlService:base::MakeRefCounted<ControlServiceInProcess>(
                                     config_)
                       appServer:scoped_refptr<AppServerMac>(this)]);

      control_service_listener_.reset([[NSXPCListener alloc]
          initWithMachServiceName:GetVersionedServiceMachName().get()]);
      control_service_listener_.get().delegate =
          control_service_delegate_.get();

      [control_service_listener_ resume];
    }
  } else if (service == kServerUpdateServiceSwitchValue) {
    @autoreleasepool {
      // Sets up a listener and delegate for the CRUUpdateChecking XPC
      // connection.
      update_check_delegate_.reset([[CRUUpdateCheckServiceXPCDelegate alloc]
          initWithUpdateService:base::MakeRefCounted<UpdateServiceInProcess>(
                                    config_)
                      appServer:scoped_refptr<AppServerMac>(this)]);

      update_check_listener_.reset([[NSXPCListener alloc]
          initWithMachServiceName:GetServiceMachName().get()]);
      update_check_listener_.get().delegate = update_check_delegate_.get();

      [update_check_listener_ resume];
    }
  } else {
    LOG(ERROR) << "Unexpected value of command line switch "
               << kServerServiceSwitch << ": " << service;
    return;
  }
}

void AppServerMac::UninstallSelf() {
  UninstallCandidate();
}

bool AppServerMac::SwapRPCInterfaces() {
  return PromoteCandidate() == setup_exit_codes::kSuccess;
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
      base::TimeDelta::FromSeconds(10));
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
