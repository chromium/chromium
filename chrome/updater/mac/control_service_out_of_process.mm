// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/control_service_out_of_process.h"

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/service_scope.h"

// Interface to communicate with the XPC Control Service.
@interface CRUControlServiceOutOfProcessImpl : NSObject <CRUControlling>

- (instancetype)initPrivileged;

@end

@implementation CRUControlServiceOutOfProcessImpl {
  base::scoped_nsobject<NSXPCConnection> _controlXPCConnection;
}

- (instancetype)init {
  return [self initWithConnectionOptions:0];
}

- (instancetype)initPrivileged {
  return [self initWithConnectionOptions:NSXPCConnectionPrivileged];
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options {
  if ((self = [super init])) {
    _controlXPCConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetVersionedServiceMachName().get()
                        options:options]);

    _controlXPCConnection.get().remoteObjectInterface =
        updater::GetXPCControllingInterface();

    _controlXPCConnection.get().interruptionHandler = ^{
      LOG(WARNING) << "CRUControlling: XPC connection interrupted.";
    };

    _controlXPCConnection.get().invalidationHandler = ^{
      LOG(WARNING) << "CRUControlling: XPC connection invalidated.";
    };

    [_controlXPCConnection resume];
  }

  return self;
}

- (void)dealloc {
  [_controlXPCConnection invalidate];
  [super dealloc];
}

- (void)performControlTasksWithReply:(void (^)(void))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply();
  };

  [[_controlXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      performControlTasksWithReply:reply];
}

- (void)performInitializeUpdateServiceWithReply:(void (^)(void))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply();
  };

  [[_controlXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      performInitializeUpdateServiceWithReply:reply];
}

@end

namespace updater {

ControlServiceOutOfProcess::ControlServiceOutOfProcess(ServiceScope scope)
    : callback_runner_(base::SequencedTaskRunnerHandle::Get()) {
  switch (scope) {
    case ServiceScope::kSystem:
      client_.reset([[CRUControlServiceOutOfProcessImpl alloc] initPrivileged]);
      break;
    case ServiceScope::kUser:
      client_.reset([[CRUControlServiceOutOfProcessImpl alloc] init]);
      break;
  }
}

void ControlServiceOutOfProcess::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(block_callback)));
  };

  [client_ performControlTasksWithReply:reply];
}

void ControlServiceOutOfProcess::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(block_callback)));
  };

  [client_ performInitializeUpdateServiceWithReply:reply];
}

void ControlServiceOutOfProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ControlServiceOutOfProcess::~ControlServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
