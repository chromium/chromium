// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/privileged_helper/service.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>
#include <pwd.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/privileged_helper/server.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  auto cb = base::BindOnce(^(const int rc) {
    VLOG(0) << "SetupSystemUpdaterWithUpdaterPath complete. Result: " << rc;
    if (reply) {
      reply(rc);
    }
    // This block is fired and then released, so this strong reference to the
    // PrivilegedHelperServiceProtocol is OK.
    self->_server->TaskCompleted();
  });

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

  newConnection.exportedObject =
      [[PrivilegedHelperServiceImpl alloc] initWithService:_service.get()
                                                    server:_server
                                            callbackRunner:_callbackRunner];
  [newConnection resume];
  return YES;
}
@end

namespace updater {
namespace {

constexpr base::FilePath::CharType kFrameworksPath[] =
    FILE_PATH_LITERAL("Contents/Frameworks/" BROWSER_PRODUCT_NAME_STRING
                      " Framework.framework/Helpers");
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
constexpr int kFailedToCreateTempDir = -4;
constexpr int kFailedToCopyToTempDir = -5;
constexpr int kFailedToVerifyUpdater = -6;

int InstallUpdater(const base::FilePath& browser_path) {
  std::string user_temp_dir(PATH_MAX, std::string::value_type());
  size_t len = confstr(_CS_DARWIN_USER_TEMP_DIR, user_temp_dir.data(),
                       user_temp_dir.size());
  if (len > user_temp_dir.size() || len == 0) {
    return kFailedToCreateTempDir;
  }
  user_temp_dir.resize(len);

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(base::FilePath(user_temp_dir))) {
    return kFailedToCreateTempDir;
  }

  if (!base::CopyDirectory(base::FilePath(browser_path)
                               .Append(kFrameworksPath)
                               .Append(kProductBundleName),
                           temp_dir.GetPath(), true)) {
    return kFailedToCopyToTempDir;
  }

  if (!VerifyUpdaterSignature(temp_dir.GetPath().Append(kProductBundleName))) {
    return kFailedToVerifyUpdater;
  }

  base::CommandLine command(temp_dir.GetPath()
                                .Append(kProductBundleName)
                                .Append("Contents/MacOS")
                                .Append(PRODUCT_FULLNAME_STRING));
  command.AppendSwitch(kInstallSwitch);
  command.AppendSwitch(kSystemSwitch);
  command.AppendSwitch(
      base::StrCat({kLoggingModuleSwitch, kLoggingModuleSwitchValue}));

  std::string output;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command, &output, &exit_code)) {
    return kFailedToInstall;
  }

  if (exit_code) {
    VLOG(0) << "Output from attempting to install system-level updater: "
            << output;
    VLOG(0) << "Exit code: " << exit_code;
  }
  return exit_code;
}

}  // namespace

bool VerifyUpdaterSignature(const base::FilePath& updater_app_bundle) {
  base::ScopedCFTypeRef<SecRequirementRef> requirement;
  base::ScopedCFTypeRef<SecStaticCodeRef> code;
  base::ScopedCFTypeRef<CFErrorRef> errors;
  if (SecStaticCodeCreateWithPath(
          base::mac::FilePathToCFURL(updater_app_bundle), kSecCSDefaultFlags,
          code.InitializeInto()) != errSecSuccess) {
    return false;
  }
  if (SecRequirementCreateWithString(
          CFSTR("anchor apple generic and ("
                " identifier \"" MAC_BUNDLE_IDENTIFIER_STRING "\""
                " or identifier \"" LEGACY_GOOGLE_UPDATE_APPID "\""
                " or identifier \"" LEGACY_GOOGLE_UPDATE_APPID ".Agent\""
                ") and certificate leaf[subject.OU] "
                "= " MAC_TEAM_IDENTIFIER_STRING),
          kSecCSDefaultFlags, requirement.InitializeInto()) != errSecSuccess) {
    return false;
  }
  if (SecStaticCodeCheckValidityWithErrors(
          code, kSecCSCheckAllArchitectures | kSecCSCheckNestedCode,
          requirement, errors.InitializeInto()) != errSecSuccess) {
    return false;
  }
  return true;
}

PrivilegedHelperService::PrivilegedHelperService()
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

PrivilegedHelperService::~PrivilegedHelperService() = default;

void PrivilegedHelperService::SetupSystemUpdater(
    const std::string& browser_path,
    base::OnceCallback<void(int)> result) {
  // Get the updater path via the browser path.
  const int install_result = InstallUpdater(base::FilePath(browser_path));
  if (install_result != kSuccess) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(result), install_result));
    return;
  }

  struct passwd* root = getpwnam("root");
  if (!root) {
    PLOG(ERROR) << "Could not find root user.";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result), kFailedToAlterBrowserOwnership));
    return;
  }

  // Recursively change |browser_path| to be owned by root, deliberately not
  // following symlinks.
  base::FileEnumerator file_enumerator(
      base::FilePath(browser_path), true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    if (lchown(name.value().c_str(), root->pw_uid, root->pw_gid)) {
      PLOG(ERROR) << "Could not alter ownership of " << name;
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(result), kFailedToAlterBrowserOwnership));
      return;
    }
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
