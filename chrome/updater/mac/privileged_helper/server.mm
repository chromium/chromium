// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/privileged_helper/server.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/privileged_helper/helper_branding.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

namespace {
constexpr int kServerKeepAliveSeconds = 1;
}

PrivilegedHelperServer::PrivilegedHelperServer() = default;
PrivilegedHelperServer::~PrivilegedHelperServer() = default;

int PrivilegedHelperServer::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_delegate_ = [[PrivilegedHelperServiceXPCDelegate alloc]
      initWithService:service_
               server:scoped_refptr<PrivilegedHelperServer>(this)];
  service_listener_ = [[NSXPCListener alloc]
      initWithMachServiceName:base::SysUTF8ToNSString(PRIVILEGED_HELPER_NAME)];
  service_listener_.delegate = service_delegate_;
  [service_listener_ resume];
  return kErrorOk;
}

void PrivilegedHelperServer::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivilegedHelperServer::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_delegate_ = nil;
  service_listener_ = nil;
  Uninstall();
}

void PrivilegedHelperServer::TaskStarted() {
  main_task_runner_->PostTask(
      FROM_HERE, BindOnce(&PrivilegedHelperServer::MarkTaskStarted, this));
}

void PrivilegedHelperServer::MarkTaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++tasks_running_;
}

base::TimeDelta PrivilegedHelperServer::ServerKeepAlive() {
  int seconds = kServerKeepAliveSeconds;
  return base::Seconds(seconds);
}

void PrivilegedHelperServer::TaskCompleted() {
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrivilegedHelperServer::AcknowledgeTaskCompletion, this),
      ServerKeepAlive());
}

void PrivilegedHelperServer::Uninstall() {
  base::DeleteFile(base::FilePath("/Library/LaunchDaemons")
                       .Append(UPDATER_HELPER_BUNDLE_ID ".plist"));
  base::DeleteFile(base::FilePath("/Library/PrivilegedHelperTools")
                       .Append(UPDATER_HELPER_BUNDLE_ID));
  base::CommandLine launchctl(base::FilePath("/bin/launchctl"));
  launchctl.AppendArg("remove");
  launchctl.AppendArg(UPDATER_HELPER_BUNDLE_ID);
  base::LaunchProcess(launchctl, {});
}

void PrivilegedHelperServer::AcknowledgeTaskCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (--tasks_running_ < 1) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PrivilegedHelperServer::Shutdown, this, 0));
  }
}

scoped_refptr<App> PrivilegedHelperServerInstance() {
  return base::MakeRefCounted<PrivilegedHelperServer>();
}

}  // namespace updater
