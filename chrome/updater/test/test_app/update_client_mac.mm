// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/update_client_mac.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/version.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"

@interface CRUUpdateClientOnDemandImpl : NSObject <CRUUpdateServicing> {
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;
}

- (BOOL)CanDialIPC;

@end

@implementation CRUUpdateClientOnDemandImpl

- (instancetype)init {
  if (self = [super init]) {
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetUpdateServiceMachName()
                        options:0]);

    _xpcConnection.get().remoteObjectInterface =
        updater::GetXPCUpdateServicingInterface();

    _xpcConnection.get().interruptionHandler = ^{
      LOG(WARNING)
          << "CRUUpdateClientOnDemandImpl: XPC connection interrupted.";
    };

    _xpcConnection.get().invalidationHandler = ^{
      LOG(WARNING)
          << "CRUUpdateClientOnDemandImpl: XPC connection invalidated.";
    };

    [_xpcConnection resume];
  }

  return self;
}

- (void)getVersionWithReply:(void (^_Nonnull)(NSString* version))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      getVersionWithReply:reply];
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_xpcConnection.get() remoteObjectProxyWithErrorHandler:errorHandler]
      registerForUpdatesWithAppId:appId
                        brandCode:brandCode
                              tag:tag
                          version:version
             existenceCheckerPath:existenceCheckerPath
                            reply:reply];
}

- (void)runPeriodicTasksWithReply:(void (^)(void))reply {
}

- (void)checkForUpdatesWithUpdateState:
            (id<CRUUpdateStateObserving> _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:
                        (id<CRUUpdateStateObserving> _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdateWithAppID:appID
                     priority:priority
                  updateState:updateState
                        reply:reply];
}

- (BOOL)CanDialIPC {
  return true;
}

@end

namespace updater {

UpdateClientMac::UpdateClientMac() {
  client_.reset([[CRUUpdateClientOnDemandImpl alloc] init]);
}

UpdateClientMac::~UpdateClientMac() {}

bool UpdateClientMac::CanDialIPC() {
  return client_.get().CanDialIPC;
}

void UpdateClientMac::BeginRegister(const std::string& brand_code,
                                    const std::string& tag,
                                    const std::string& version,
                                    UpdateService::Callback callback) {
  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);

  auto reply = ^(int error) {
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  [client_.get() registerForUpdatesWithAppId:base::SysUTF8ToNSString(kTestAppId)
                                   brandCode:base::SysUTF8ToNSString(brand_code)
                                         tag:base::SysUTF8ToNSString(tag)
                                     version:base::SysUTF8ToNSString(version)
                        existenceCheckerPath:base::mac::FilePathToNSString(
                                                 base::mac::OuterBundlePath())
                                       reply:reply];
}

void UpdateClientMac::BeginUpdateCheck(
    UpdateService::StateChangeCallback state_update,
    UpdateService::Callback callback) {
  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);

  auto reply = ^(int error) {
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  base::scoped_nsobject<CRUPriorityWrapper> priorityWrapper(
      [[CRUPriorityWrapper alloc]
          initWithPriority:UpdateService::Priority::kForeground]);
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc] initWithRepeatingCallback:state_update
                                                 callbackRunner:task_runner()]);

  [client_.get()
      checkForUpdateWithAppID:base::SysUTF8ToNSString(base::mac::BaseBundleID())
                     priority:priorityWrapper.get()
                  updateState:stateObserver.get()
                        reply:reply];
}

scoped_refptr<UpdateClient> UpdateClient::Create() {
  return base::MakeRefCounted<UpdateClientMac>();
}

}  // namespace updater
