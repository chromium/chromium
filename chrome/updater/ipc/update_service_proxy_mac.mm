// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_mac.h"

#import <Foundation/Foundation.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/update_client_errors.h"

using base::SysUTF8ToNSString;

// Interface to communicate with the XPC Updater Service.
@interface CRUUpdateServiceProxyImpl : NSObject <CRUUpdateServicing>

- (instancetype)initWithScope:(updater::UpdaterScope)scope;

@end

@implementation CRUUpdateServiceProxyImpl {
  base::scoped_nsobject<NSXPCConnection> _updateCheckXPCConnection;
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
    _updateCheckXPCConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetUpdateServiceMachName(scope).get()
                        options:options]);

    _updateCheckXPCConnection.get().remoteObjectInterface =
        updater::GetXPCUpdateServicingInterface();

    _updateCheckXPCConnection.get().interruptionHandler = ^{
      LOG(WARNING) << "CRUUpdateServicingService: XPC connection interrupted.";
    };

    _updateCheckXPCConnection.get().invalidationHandler = ^{
      LOG(WARNING) << "CRUUpdateServicingService: XPC connection invalidated.";
    };

    [_updateCheckXPCConnection resume];
  }

  return self;
}

- (void)dealloc {
  [_updateCheckXPCConnection invalidate];
  [super dealloc];
}

- (void)getVersionWithReply:(void (^_Nonnull)(NSString* version))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      getVersionWithReply:reply];
}

- (void)fetchPoliciesWithReply:(void (^)(int))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(xpcError.code);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      fetchPoliciesWithReply:reply];
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                          brandPath:(NSString* _Nullable)brandPath
                                tag:(NSString* _Nullable)ap
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection.get()
      remoteObjectProxyWithErrorHandler:errorHandler]
      registerForUpdatesWithAppId:appId
                        brandCode:brandCode
                        brandPath:brandPath
                              tag:ap
                          version:version
             existenceCheckerPath:existenceCheckerPath
                            reply:reply];
}

- (void)getAppStatesWithReply:(void (^_Nonnull)(CRUAppStatesWrapper*))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      getAppStatesWithReply:reply];
}

- (void)runPeriodicTasksWithReply:(void (^)(void))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply();
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      runPeriodicTasksWithReply:reply];
}

- (void)checkForUpdatesWithUpdateState:
            (id<CRUUpdateStateObserving> _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdatesWithUpdateState:updateState
                               reply:reply];
}

- (void)checkForUpdateWithAppId:(NSString* _Nonnull)appId
               installDataIndex:(NSString* _Nullable)installDataIndex
                       priority:(CRUPriorityWrapper* _Nonnull)priority
        policySameVersionUpdate:
            (CRUPolicySameVersionUpdateWrapper* _Nonnull)policySameVersionUpdate
                    updateState:
                        (id<CRUUpdateStateObserving> _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdateWithAppId:appId
             installDataIndex:installDataIndex
                     priority:priority
      policySameVersionUpdate:policySameVersionUpdate
                  updateState:updateState
                        reply:reply];
}

- (void)installWithAppId:(NSString* _Nonnull)appId
               brandCode:(NSString* _Nullable)brandCode
               brandPath:(NSString* _Nullable)brandPath
                     tag:(NSString* _Nullable)ap
                 version:(NSString* _Nullable)version
    existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
       clientInstallData:(NSString* _Nullable)clientInstallData
        installDataIndex:(NSString* _Nullable)installDataIndex
                priority:(CRUPriorityWrapper* _Nonnull)priority
             updateState:(id<CRUUpdateStateObserving> _Nonnull)updateState
                   reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
          installWithAppId:appId
                 brandCode:brandCode
                 brandPath:brandPath
                       tag:ap
                   version:version
      existenceCheckerPath:existenceCheckerPath
         clientInstallData:clientInstallData
          installDataIndex:installDataIndex
                  priority:priority
               updateState:updateState
                     reply:reply];
}

- (void)cancelInstallsWithAppId:(NSString* _Nonnull)appId {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      cancelInstallsWithAppId:appId];
}

- (void)runInstallerWithAppId:(NSString* _Nonnull)appId
                installerPath:(NSString* _Nonnull)installerPath
                  installArgs:(NSString* _Nullable)installArgs
                  installData:(NSString* _Nullable)installData
              installSettings:(NSString* _Nullable)installSettings
                  updateState:(id<CRUUpdateStateObserving> _Nonnull)updateState
                        reply:(void (^_Nonnull)(
                                  updater::UpdateService::Result rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(updater::UpdateService::Result::kServiceFailed);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      runInstallerWithAppId:appId
              installerPath:installerPath
                installArgs:installArgs
                installData:installData
            installSettings:installSettings
                updateState:updateState
                      reply:reply];
}
@end

namespace updater {

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope,
    const base::TimeDelta& get_version_timeout) {
  return base::MakeRefCounted<UpdateServiceProxy>(updater_scope,
                                                  get_version_timeout);
}

UpdateServiceProxy::UpdateServiceProxy(
    UpdaterScope scope,
    const base::TimeDelta& get_version_timeout)
    : scope_(scope), get_version_timeout_(get_version_timeout) {
  client_.reset([[CRUUpdateServiceProxyImpl alloc] initWithScope:scope]);
  callback_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << __func__ << " with timeout " << get_version_timeout_;
  auto timeout_callback = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce(&UpdateServiceProxy::Reset, base::Unretained(this)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_callback->callback(), get_version_timeout_);

  __block base::OnceCallback<void(const base::Version&)> block_callback =
      std::move(callback).Then(base::BindOnce(
          &base::CancelableOnceClosure::Cancel, std::move(timeout_callback)));
  auto reply = ^(NSString* version) {
    callback_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(block_callback),
                       base::Version(base::SysNSStringToUTF8(version))));
  };
  [client_ getVersionWithReply:reply];
}

void UpdateServiceProxy::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << __func__;
  __block base::OnceCallback<void(int)> block_callback = std::move(callback);
  auto reply = ^(int result) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), result));
  };
  [client_ fetchPoliciesWithReply:reply];
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  __block base::OnceCallback<void(int)> block_callback = std::move(callback);

  auto reply = ^(int error) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), error));
  };

  [client_
      registerForUpdatesWithAppId:SysUTF8ToNSString(request.app_id)
                        brandCode:SysUTF8ToNSString(request.brand_code)
                        brandPath:base::mac::FilePathToNSString(
                                      request.brand_path)
                              tag:SysUTF8ToNSString(request.ap)
                          version:SysUTF8ToNSString(request.version.GetString())
             existenceCheckerPath:base::mac::FilePathToNSString(
                                      request.existence_checker_path)
                            reply:reply];
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<
        void(const std::vector<updater::UpdateService::AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  __block base::OnceCallback<void(
      const std::vector<updater::UpdateService::AppState>&)>
      block_callback = std::move(callback);

  auto reply = ^(CRUAppStatesWrapper* wrapper) {
    callback_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(block_callback),
                       wrapper
                           ? wrapper.states
                           : std::vector<updater::UpdateService::AppState>()));
  };
  [client_ getAppStatesWithReply:reply];
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE, std::move(block_callback));
  };
  [client_ runPeriodicTasksWithReply:reply];
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);
  auto reply = ^(int error) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);
  [client_ checkForUpdatesWithUpdateState:stateObserver.get() reply:reply];
}

void UpdateServiceProxy::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);
  auto reply = ^(int error) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  base::scoped_nsobject<CRUPriorityWrapper> priorityWrapper(
      [[CRUPriorityWrapper alloc] initWithPriority:priority]);
  base::scoped_nsobject<CRUPolicySameVersionUpdateWrapper>
      policySameVersionUpdateWrapper([[CRUPolicySameVersionUpdateWrapper alloc]
          initWithPolicySameVersionUpdate:policy_same_version_update]);
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);

  [client_ checkForUpdateWithAppId:SysUTF8ToNSString(app_id)
                  installDataIndex:SysUTF8ToNSString(install_data_index)
                          priority:priorityWrapper.get()
           policySameVersionUpdate:policySameVersionUpdateWrapper.get()
                       updateState:stateObserver.get()
                             reply:reply];
}

void UpdateServiceProxy::Install(const RegistrationRequest& registration,
                                 const std::string& client_install_data,
                                 const std::string& install_data_index,
                                 Priority priority,
                                 StateChangeCallback state_update,
                                 Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);
  auto reply = ^(int error) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  base::scoped_nsobject<CRUPriorityWrapper> priorityWrapper(
      [[CRUPriorityWrapper alloc] initWithPriority:priority]);
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);

  [client_ installWithAppId:SysUTF8ToNSString(registration.app_id)
                  brandCode:SysUTF8ToNSString(registration.brand_code)
                  brandPath:base::mac::FilePathToNSString(
                                registration.brand_path)
                        tag:SysUTF8ToNSString(registration.ap)
                    version:SysUTF8ToNSString(registration.version.GetString())
       existenceCheckerPath:base::mac::FilePathToNSString(
                                registration.existence_checker_path)
          clientInstallData:SysUTF8ToNSString(client_install_data)
           installDataIndex:SysUTF8ToNSString(install_data_index)
                   priority:priorityWrapper.get()
                updateState:stateObserver.get()
                      reply:reply];
}

void UpdateServiceProxy::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  [client_ cancelInstallsWithAppId:SysUTF8ToNSString(app_id)];
}

void UpdateServiceProxy::RunInstaller(const std::string& app_id,
                                      const base::FilePath& installer_path,
                                      const std::string& install_args,
                                      const std::string& install_data,
                                      const std::string& install_settings,
                                      StateChangeCallback state_update,
                                      Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);
  auto reply = ^(updater::UpdateService::Result rc) {
    callback_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(block_callback), rc));
  };

  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);

  [client_ runInstallerWithAppId:SysUTF8ToNSString(app_id)
                   installerPath:base::mac::FilePathToNSString(installer_path)
                     installArgs:SysUTF8ToNSString(install_args)
                     installData:SysUTF8ToNSString(install_data)
                 installSettings:SysUTF8ToNSString(install_settings)
                     updateState:stateObserver.get()
                           reply:reply];
}

void UpdateServiceProxy::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  client_.reset([[CRUUpdateServiceProxyImpl alloc] initWithScope:scope_]);
}

UpdateServiceProxy::~UpdateServiceProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
