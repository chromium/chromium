// Copyright 2020 The Chromium Authors
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

- (instancetype)initWithScope:(updater::UpdaterScope)scope;

@end

@implementation CRUUpdateServiceInternalProxyImpl {
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;
}

- (instancetype)initWithScope:(updater::UpdaterScope)scope {
  switch (scope) {
    case updater::UpdaterScope::kUser:
      return [self initWithConnectionOptions:0 withScope:scope];
    case updater::UpdaterScope::kSystem:
      return [self initWithConnectionOptions:NSXPCConnectionPrivileged
                                   withScope:scope];
  }
  return nil;
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options
                                withScope:(updater::UpdaterScope)scope {
  if ((self = [super init])) {
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetUpdateServiceInternalMachName(scope)
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

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope updater_scope) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(updater_scope);
}

UpdateServiceInternalProxy::UpdateServiceInternalProxy(UpdaterScope scope)
    : callback_runner_(base::SequencedTaskRunnerHandle::Get()) {
  client_.reset(
      [[CRUUpdateServiceInternalProxyImpl alloc] initWithScope:scope]);
}

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE, std::move(block_callback));
  };

  [client_ performTasksWithReply:reply];
}

void UpdateServiceInternalProxy::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE, std::move(block_callback));
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
