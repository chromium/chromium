// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_internal_proxy.h"

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_scope.h"

// Interface to communicate with the XPC Update Service Internal.
@interface CRUUpdateServiceInternalProxyImpl
    : NSObject <CRUUpdateServicingInternal>

- (instancetype)initPrivileged;

@end

@implementation CRUUpdateServiceInternalProxyImpl {
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;
}

- (instancetype)init {
  return [self initWithConnectionOptions:0];
}

- (instancetype)initPrivileged {
  return [self initWithConnectionOptions:NSXPCConnectionPrivileged];
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options {
  if ((self = [super init])) {
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetUpdateServiceInternalMachName()
                                    .get()
                        options:options]);

    _xpcConnection.get().remoteObjectInterface =
        updater::GetXPCUpdateServicingInternalInterface();

    _xpcConnection.get().interruptionHandler = ^{
      LOG(WARNING) << "CRUUpdateServicingInternal: XPC connection interrupted.";
    };

    _xpcConnection.get().invalidationHandler = ^{
      LOG(WARNING) << "CRUUpdateServicingInternal: XPC connection invalidated.";
    };

    [_xpcConnection resume];
  }

  return self;
}

- (void)dealloc {
  [_xpcConnection invalidate];
  [super dealloc];
}

- (void)performTasksWithReply:(void (^)(void))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply();
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      performTasksWithReply:reply];
}

- (void)performInitializeUpdateServiceWithReply:(void (^)(void))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply();
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      performInitializeUpdateServiceWithReply:reply];
}

@end

namespace updater {

UpdateServiceInternalProxy::UpdateServiceInternalProxy(UpdaterScope scope)
    : callback_runner_(base::SequencedTaskRunnerHandle::Get()) {
  switch (scope) {
    case UpdaterScope::kSystem:
      client_.reset([[CRUUpdateServiceInternalProxyImpl alloc] initPrivileged]);
      break;
    case UpdaterScope::kUser:
      client_.reset([[CRUUpdateServiceInternalProxyImpl alloc] init]);
      break;
  }
}

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(block_callback)));
  };

  [client_ performTasksWithReply:reply];
}

void UpdateServiceInternalProxy::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(block_callback)));
  };

  [client_ performInitializeUpdateServiceWithReply:reply];
}

void UpdateServiceInternalProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

UpdateServiceInternalProxy::~UpdateServiceInternalProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
