// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/service_delegate.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#import "chrome/updater/app/server/mac/app_server.h"
#import "chrome/updater/app/server/mac/server.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#include "chrome/updater/control_service.h"
#include "chrome/updater/mac/setup/setup.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"

@interface CRUUpdateCheckServiceXPCImpl : NSObject <CRUUpdateChecking>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)
    initWithUpdateService:(updater::UpdateService*)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer
           callbackRunner:
               (scoped_refptr<base::SequencedTaskRunner>)callbackRunner
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateCheckServiceXPCImpl {
  updater::UpdateService* _service;
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

#pragma mark CRUUpdateChecking
- (void)checkForUpdatesWithUpdateState:(id<CRUUpdateStateObserving>)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        if (reply)
          reply(static_cast<int>(error));

        _appServer->TaskCompleted();
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      updater::UpdateService::UpdateState state) {
    NSString* version = base::SysUTF8ToNSString(
        state.next_version.IsValid() ? state.next_version.GetString() : "");

    base::scoped_nsobject<CRUUpdateStateStateWrapper> updateStateStateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:state.state],
        base::scoped_policy::RETAIN);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:state.error_category],
        base::scoped_policy::RETAIN);

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
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(id<CRUUpdateStateObserving>)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "Update complete: error = " << static_cast<int>(error);
        if (reply)
          reply(static_cast<int>(error));

        _appServer->TaskCompleted();
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      updater::UpdateService::UpdateState state) {
    NSString* version = base::SysUTF8ToNSString(
        state.next_version.IsValid() ? state.next_version.GetString() : "");

    base::scoped_nsobject<CRUUpdateStateStateWrapper> updateStateStateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:state.state],
        base::scoped_policy::RETAIN);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:state.error_category],
        base::scoped_policy::RETAIN);

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
                     base::SysNSStringToUTF8(appID), [priority priority],
                     std::move(sccb), std::move(cb)));
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  updater::RegistrationRequest request;
  request.app_id = base::SysNSStringToUTF8(appId);
  request.brand_code = base::SysNSStringToUTF8(brandCode);
  request.tag = base::SysNSStringToUTF8(tag);
  request.version = base::Version(base::SysNSStringToUTF8(version));
  request.existence_checker_path =
      base::FilePath(base::SysNSStringToUTF8(existenceCheckerPath));

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

@end

@interface CRUControlServiceXPCImpl : NSObject <CRUControlling>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)
    initWithControlService:(updater::ControlService*)service
                 appServer:(scoped_refptr<updater::AppServerMac>)appServer
            callbackRunner:
                (scoped_refptr<base::SequencedTaskRunner>)callbackRunner
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUControlServiceXPCImpl {
  updater::ControlService* _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithControlService:(updater::ControlService*)service
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

#pragma mark CRUControlling
- (void)performControlTasksWithReply:(void (^)(void))reply {
  auto cb = base::BindOnce(base::RetainBlock(^(void) {
    VLOG(0) << "performControlTasks complete.";
    if (reply)
      reply();

    _appServer->TaskCompleted();
  }));

  _appServer->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&updater::ControlService::Run, _service, std::move(cb)));
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
      base::BindOnce(&updater::ControlService::InitializeUpdateService,
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
  // Check to see if the other side of the connection is "okay";
  // if not, invalidate newConnection and return NO.

  newConnection.exportedInterface = updater::GetXPCUpdateCheckingInterface();

  base::scoped_nsobject<CRUUpdateCheckServiceXPCImpl> object(
      [[CRUUpdateCheckServiceXPCImpl alloc]
          initWithUpdateService:_service.get()
                      appServer:_appServer
                 callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end

@implementation CRUControlServiceXPCDelegate {
  scoped_refptr<updater::ControlService> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithControlService:(scoped_refptr<updater::ControlService>)service
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
  // Check to see if the other side of the connection is "okay";
  // if not, invalidate newConnection and return NO.

  newConnection.exportedInterface = updater::GetXPCControllingInterface();

  base::scoped_nsobject<CRUControlServiceXPCImpl> object(
      [[CRUControlServiceXPCImpl alloc]
          initWithControlService:_service.get()
                       appServer:_appServer
                  callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
