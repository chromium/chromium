// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/service_delegate.h"

#import <Foundation/Foundation.h>

#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#import "chrome/updater/app/server/mac/app_server.h"
#import "chrome/updater/app/server/mac/server.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/setup/setup.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

@interface CRUUpdateServiceXPCImpl : NSObject <CRUUpdateServicing>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)
    initWithUpdateService:(updater::UpdateService*)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer
           callbackRunner:
               (scoped_refptr<base::SequencedTaskRunner>)callbackRunner
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateServiceXPCImpl {
  scoped_refptr<updater::UpdateService> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithUpdateService:(updater::UpdateService*)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer
           callbackRunner:
               (scoped_refptr<base::SequencedTaskRunner>)callbackRunner {
  if (self = [super init]) {
    _service = service;
    _appServer = appServer;
    _callbackRunner = callbackRunner;
  }
  return self;
}

#pragma mark CRUUpdateServicing
- (void)getVersionWithReply:(void (^_Nonnull)(NSString* version))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(const base::Version& updaterVersion) {
        VLOG(0) << "GetVersion complete: version = "
                << (updaterVersion.IsValid() ? updaterVersion.GetString() : "");
        if (reply) {
          reply(base::SysUTF8ToNSString(
              updaterVersion.IsValid() ? updaterVersion.GetString() : nil));
        }

        _appServer->TaskCompleted();
      }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::GetVersion, _service,
                                std::move(cb)));
}

- (void)runPeriodicTasksWithReply:(void (^)(void))reply {
  auto cb = base::BindOnce(base::RetainBlock(^(void) {
    VLOG(0) << "RunPeriodicTasks complete.";
    if (reply)
      reply();

    _appServer->TaskCompleted();
  }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::RunPeriodicTasks,
                                _service, std::move(cb)));
}

- (void)checkForUpdatesWithUpdateState:(id<CRUUpdateStateObserving>)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "UpdateAll complete: error = " << error;
        if (reply)
          reply(static_cast<int>(error));

        _appServer->TaskCompleted();
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      const updater::UpdateService::UpdateState& state) {
    NSString* version = base::SysUTF8ToNSString(
        state.next_version.IsValid() ? state.next_version.GetString() : "");

    base::scoped_nsobject<CRUUpdateStateStateWrapper> updateStateStateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:state.state]);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:state.error_category]);

    base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(state.app_id)
                      state:updateStateStateWrapper.get()
                    version:version
            downloadedBytes:state.downloaded_bytes
                 totalBytes:state.total_bytes
            installProgress:state.install_progress
              errorCategory:errorCategoryWrapper.get()
                  errorCode:state.error_code
                  extraCode:state.extra_code1]);
    [updateState observeUpdateState:updateStateWrapper.get()];
  }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::UpdateAll, _service,
                                std::move(sccb), std::move(cb)));
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
               installDataIndex:(NSString* _Nullable)installDataIndex
                       priority:(CRUPriorityWrapper* _Nonnull)priority
        policySameVersionUpdate:
            (CRUPolicySameVersionUpdateWrapper* _Nonnull)policySameVersionUpdate
                    updateState:(id<CRUUpdateStateObserving>)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  // This function may be called by any user.
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "Update complete: error = " << error;
        if (reply)
          reply(static_cast<int>(error));

        _appServer->TaskCompleted();
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      const updater::UpdateService::UpdateState& state) {
    NSString* version = base::SysUTF8ToNSString(
        state.next_version.IsValid() ? state.next_version.GetString() : "");

    base::scoped_nsobject<CRUUpdateStateStateWrapper> updateStateStateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:state.state]);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:state.error_category]);

    base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(state.app_id)
                      state:updateStateStateWrapper.get()
                    version:version
            downloadedBytes:state.downloaded_bytes
                 totalBytes:state.total_bytes
            installProgress:state.install_progress
              errorCategory:errorCategoryWrapper.get()
                  errorCode:state.error_code
                  extraCode:state.extra_code1]);
    [updateState observeUpdateState:updateStateWrapper.get()];
  }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&updater::UpdateService::Update, _service,
                     base::SysNSStringToUTF8(appID),
                     base::SysNSStringToUTF8(installDataIndex),
                     [priority priority],
                     [policySameVersionUpdate policySameVersionUpdate],
                     std::move(sccb), std::move(cb)));
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                          brandPath:(NSString* _Nullable)brandPath
                                tag:(NSString* _Nullable)ap
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  updater::RegistrationRequest request;
  request.app_id = base::SysNSStringToUTF8(appId);
  request.brand_code = base::SysNSStringToUTF8(brandCode);
  request.brand_path = base::mac::NSStringToFilePath(brandPath);
  request.ap = base::SysNSStringToUTF8(ap);
  request.version = base::Version(base::SysNSStringToUTF8(version));
  request.existence_checker_path =
      base::mac::NSStringToFilePath(existenceCheckerPath);

  auto cb = base::BindOnce(
      base::RetainBlock(^(const updater::RegistrationResponse& response) {
        VLOG(0) << "Registration complete: status code = "
                << response.status_code;
        if (reply)
          reply(response.status_code);

        _appServer->TaskCompleted();
      }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::RegisterApp, _service,
                                request, std::move(cb)));
}

- (void)getAppStatesWithReply:(void (^_Nonnull)(CRUAppStatesWrapper*))reply {
  [self getAppStatesWithReply:reply restrictedView:NO];
}

- (void)getAppStatesWithReply:(void (^_Nonnull)(CRUAppStatesWrapper*))reply
               restrictedView:(bool)restrictedView {
  auto cb = base::BindOnce(base::RetainBlock(
      ^(const std::vector<updater::UpdateService::AppState>& states) {
        if (reply) {
          base::scoped_nsobject<CRUAppStatesWrapper> appStatesWrapper(
              [[CRUAppStatesWrapper alloc] initWithAppStates:states
                                              restrictedView:restrictedView]);
          reply(appStatesWrapper);
        }

        _appServer->TaskCompleted();
      }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::GetAppStates, _service,
                                std::move(cb)));
}
@end

// CRUUpdateServiceXPCFilterUnprivileged is an implementation of UpdateService
// that rejects sensitive operations (which can only be performed by privileged
// clients). It only accepts operations that are safe for any user on the
// system (even an attacker) to call.
@interface CRUUpdateServiceXPCFilterUnprivileged : NSObject <CRUUpdateServicing>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)initWithService:
    (base::scoped_nsobject<CRUUpdateServiceXPCImpl>)service
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateServiceXPCFilterUnprivileged {
  base::scoped_nsobject<CRUUpdateServiceXPCImpl> _service;
}

- (instancetype)initWithService:
    (base::scoped_nsobject<CRUUpdateServiceXPCImpl>)service {
  if (self = [super init]) {
    _service = service;
  }
  return self;
}

#pragma mark CRUUpdateServicing
- (void)getVersionWithReply:(void (^_Nonnull)(NSString* version))reply {
  // This function may be called by any user.
  [_service getVersionWithReply:reply];
}

- (void)runPeriodicTasksWithReply:(void (^)(void))reply {
  // This function may only be called by the same user.
  VLOG(1) << "Rejecting cross-user attempt to call " << __func__;
  if (reply)
    reply();
}

- (void)checkForUpdatesWithUpdateState:(id<CRUUpdateStateObserving>)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  // This function may only be called by the same user.
  VLOG(1) << "Rejecting cross-user attempt to call " << __func__;
  if (reply)
    reply(updater::kErrorPermissionDenied);
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
               installDataIndex:(NSString* _Nullable)installDataIndex
                       priority:(CRUPriorityWrapper* _Nonnull)priority
        policySameVersionUpdate:
            (CRUPolicySameVersionUpdateWrapper* _Nonnull)policySameVersionUpdate
                    updateState:(id<CRUUpdateStateObserving>)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  // This function may be called by any user.
  [_service checkForUpdateWithAppID:appID
                   installDataIndex:installDataIndex
                           priority:priority
            policySameVersionUpdate:policySameVersionUpdate
                        updateState:updateState
                              reply:reply];
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                          brandPath:(NSString* _Nullable)brandPath
                                tag:(NSString* _Nullable)ap
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  // This function may only be called by the same user.
  VLOG(1) << "Rejecting cross-user attempt to call " << __func__;
  if (reply)
    reply(updater::kErrorPermissionDenied);
}

- (void)getAppStatesWithReply:(void (^_Nonnull)(CRUAppStatesWrapper*))reply {
  // Cross-user gets a restricted view of the app states.
  [_service getAppStatesWithReply:reply restrictedView:YES];
}
@end

@interface CRUUpdateServiceInternalXPCImpl
    : NSObject <CRUUpdateServicingInternal>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)
    initWithUpdateServiceInternal:
        (scoped_refptr<updater::UpdateServiceInternal>)service
                        appServer:
                            (scoped_refptr<updater::AppServerMac>)appServer
                   callbackRunner:
                       (scoped_refptr<base::SequencedTaskRunner>)callbackRunner
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateServiceInternalXPCImpl {
  scoped_refptr<updater::UpdateServiceInternal> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithUpdateServiceInternal:
        (scoped_refptr<updater::UpdateServiceInternal>)service
                        appServer:
                            (scoped_refptr<updater::AppServerMac>)appServer
                   callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                      callbackRunner {
  if (self = [super init]) {
    _service = service;
    _appServer = appServer;
    _callbackRunner = callbackRunner;
  }
  return self;
}

#pragma mark CRUUpdateServicingInternal
- (void)performTasksWithReply:(void (^)(void))reply {
  auto cb = base::BindOnce(base::RetainBlock(^(void) {
    VLOG(0) << "performTasks complete.";
    if (reply)
      reply();

    _appServer->TaskCompleted();
  }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateServiceInternal::Run, _service,
                                std::move(cb)));
}

- (void)performInitializeUpdateServiceWithReply:(void (^)(void))reply {
  auto cb = base::BindOnce(base::RetainBlock(^(void) {
    if (reply)
      reply();

    _appServer->TaskCompleted();
  }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&updater::UpdateServiceInternal::InitializeUpdateService,
                     _service, std::move(cb)));
}

@end

@implementation CRUUpdateCheckServiceXPCDelegate {
  scoped_refptr<updater::UpdateService> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithUpdateService:(scoped_refptr<updater::UpdateService>)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer {
  if (self = [super init]) {
    _service = service;
    _appServer = appServer;
    _callbackRunner = base::SequencedTaskRunnerHandle::Get();
  }
  return self;
}

- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
  newConnection.exportedInterface = updater::GetXPCUpdateServicingInterface();

  base::scoped_nsobject<CRUUpdateServiceXPCImpl> impl(
      [[CRUUpdateServiceXPCImpl alloc]
          initWithUpdateService:_service.get()
                      appServer:_appServer
                 callbackRunner:_callbackRunner.get()]);
  if (newConnection.effectiveUserIdentifier == geteuid()) {
    newConnection.exportedObject = impl.get();
  } else {
    // Other users get an unprivileged implementation.
    base::scoped_nsobject<CRUUpdateServiceXPCFilterUnprivileged> unprivileged(
        [[CRUUpdateServiceXPCFilterUnprivileged alloc] initWithService:impl]);
    newConnection.exportedObject = unprivileged.get();
  }
  [newConnection resume];
  return YES;
}

@end

@implementation CRUUpdateServiceInternalXPCDelegate {
  scoped_refptr<updater::UpdateServiceInternal> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)initWithUpdateServiceInternal:
                    (scoped_refptr<updater::UpdateServiceInternal>)service
                                    appServer:
                                        (scoped_refptr<updater::AppServerMac>)
                                            appServer {
  if (self = [super init]) {
    _service = service;
    _appServer = appServer;
    _callbackRunner = base::SequencedTaskRunnerHandle::Get();
  }
  return self;
}

- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
  // Reject connections from other users.
  if (newConnection.effectiveUserIdentifier != geteuid()) {
    VLOG(1) << "Rejecting UpdateServiceInternal XPC connection from EUID "
            << newConnection.effectiveUserIdentifier << ", required "
            << geteuid() << ".";
    return NO;
  }

  newConnection.exportedInterface =
      updater::GetXPCUpdateServicingInternalInterface();

  base::scoped_nsobject<CRUUpdateServiceInternalXPCImpl> object(
      [[CRUUpdateServiceInternalXPCImpl alloc]
          initWithUpdateServiceInternal:_service.get()
                              appServer:_appServer
                         callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
