// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/privileged_helper/service.h"

#include "base/memory/raw_ptr.h"

#import <Foundation/Foundation.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/privileged_helper/server.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util.h"

@interface PrivilegedHelperServiceImpl
    : NSObject <PrivilegedHelperServiceProtocol> {
  raw_ptr<updater::PrivilegedHelperService> _service;
  scoped_refptr<updater::PrivilegedHelperServer> _server;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}
@end

@implementation PrivilegedHelperServiceImpl

- (instancetype)
    initWithService:(updater::PrivilegedHelperService*)service
             server:(scoped_refptr<updater::PrivilegedHelperServer>)server
     callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)callbackRunner {
  if (self = [super init]) {
    _service = service;
    _server = server;
    _callbackRunner = callbackRunner;
  }

  return self;
}

#pragma mark PrivilegedHelperServiceProtocol
- (void)setupSystemUpdaterWithBrowserPath:(NSString* _Nonnull)browserPath
                                    reply:(void (^_Nonnull)(int rc))reply {
  auto cb = base::BindOnce(base::RetainBlock(^(const int rc) {
    VLOG(0) << "SetupSystemUpdaterWithUpdaterPath complete. Result: " << rc;
    if (reply)
      reply(rc);
    _server->TaskCompleted();
  }));

  _server->TaskStarted();
  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&updater::PrivilegedHelperService::SetupSystemUpdater,
                     _service, base::SysNSStringToUTF8(browserPath),
                     std::move(cb)));
}
@end

@implementation PrivilegedHelperServiceXPCDelegate {
  scoped_refptr<updater::PrivilegedHelperService> _service;
  scoped_refptr<updater::PrivilegedHelperServer> _server;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithService:(scoped_refptr<updater::PrivilegedHelperService>)service
             server:(scoped_refptr<updater::PrivilegedHelperServer>)server {
  if (self = [super init]) {
    _service = service;
    _server = server;
    _callbackRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::WithBaseSyncPrimitives(),
         base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  return self;
}

- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
  newConnection.exportedInterface = [NSXPCInterface
      interfaceWithProtocol:@protocol(PrivilegedHelperServiceProtocol)];

  base::scoped_nsobject<PrivilegedHelperServiceImpl> obj(
      [[PrivilegedHelperServiceImpl alloc] initWithService:_service.get()
                                                    server:_server
                                            callbackRunner:_callbackRunner]);
  newConnection.exportedObject = obj.get();
  [newConnection resume];
  return YES;
}
@end

namespace updater {
namespace {

constexpr base::FilePath::CharType kFrameworksPath[] = FILE_PATH_LITERAL(
    "Contents/Frameworks/" BROWSER_NAME_STRING " Framework.framework/Helpers");
constexpr base::FilePath::CharType kProductBundleName[] =
    FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".app");
constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                 base::FILE_PERMISSION_GROUP_MASK |
                                 base::FILE_PERMISSION_READ_BY_OTHERS |
                                 base::FILE_PERMISSION_EXECUTE_BY_OTHERS;

// Exit codes
constexpr int kSuccess = 0;
constexpr int kFailedToInstall = -1;
constexpr int kFailedToAlterBrowserOwnership = -2;
constexpr int kFailedToConfirmPermissionChanges = -3;
}

PrivilegedHelperService::PrivilegedHelperService()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

PrivilegedHelperService::~PrivilegedHelperService() = default;

void PrivilegedHelperService::SetupSystemUpdater(
    const std::string& browser_path,
    base::OnceCallback<void(int)> result) {
  // Get the updater path via the browser path.
  base::FilePath updater_path_exec_path = base::FilePath(browser_path)
                                              .Append(kFrameworksPath)
                                              .Append(kProductBundleName)
                                              .Append("Contents/MacOS")
                                              .Append(PRODUCT_FULLNAME_STRING);

  base::CommandLine command(updater_path_exec_path);
  command.AppendSwitch(kInstallSwitch);
  command.AppendSwitch(kSystemSwitch);
  command.AppendSwitch(
      base::StrCat({kLoggingModuleSwitch, kLoggingModuleSwitchValue}));

  std::string output;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command, &output, &exit_code)) {
    VLOG(0) << "Something went wrong with installing the updater: "
            << updater_path_exec_path;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(result), kFailedToInstall));
    return;
  }

  if (exit_code) {
    VLOG(0) << "Output from attempting to install system-level updater: "
            << output;
    VLOG(0) << "Exit code: " << exit_code;
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(result), exit_code));
    return;
  }

  base::CommandLine chown_cmd(base::FilePath("/usr/sbin/chown"));
  chown_cmd.AppendArg("-hR");
  chown_cmd.AppendArg("root:wheel");
  chown_cmd.AppendArg(browser_path);

  if (!base::GetAppOutputWithExitCode(chown_cmd, &output, &exit_code)) {
    VLOG(0) << "Something went wrong with altering the browser ownership: "
            << browser_path;
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result), kFailedToAlterBrowserOwnership));
    return;
  }

  if (exit_code) {
    VLOG(0) << "Output from attempting to alter browser ownership: " << output;
    VLOG(0) << "Exit code: " << exit_code;
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(result), exit_code));
    return;
  }

  if (!ConfirmFilePermissions(base::FilePath(browser_path), kPermissionsMask)) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result), kFailedToConfirmPermissionChanges));
    return;
  }

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(result), kSuccess));
}

}  // namespace updater
