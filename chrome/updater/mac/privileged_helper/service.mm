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

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
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
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"

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
constexpr base::FilePath::CharType kKeystoneBundleName[] =
    FILE_PATH_LITERAL(KEYSTONE_NAME ".bundle");
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
constexpr int kFailedToReadBrowserPlist = -7;
constexpr int kFailedToRegister = -8;

}  // namespace

int InstallUpdater(const base::FilePath& browser_path) {
  base::FilePath browser_plist = browser_path.Append("Contents/Info.plist");
  std::optional<std::string> browser_app_id =
      ReadValueFromPlist(browser_plist, "KSProductID");
  std::optional<std::string> browser_version =
      ReadValueFromPlist(browser_plist, "KSVersion");
  if (!browser_app_id || !browser_version) {
    return kFailedToReadBrowserPlist;
  }

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

  if (!CopyDir(base::FilePath(browser_path)
                   .Append(kFrameworksPath)
                   .Append(kProductBundleName),
               temp_dir.GetPath(), false)) {
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
    return exit_code;
  }

  base::CommandLine ksadmin_command(temp_dir.GetPath()
                                        .Append(kProductBundleName)
                                        .Append("Contents/Helpers")
                                        .Append(kKeystoneBundleName)
                                        .Append("Contents/Helpers/ksadmin"));
  // ksadmin does not support --switch=value, only --switch value, except for
  // logging arguments.
  ksadmin_command.AppendArg("--register");
  ksadmin_command.AppendArg("--productid");
  ksadmin_command.AppendArg(*browser_app_id);
  ksadmin_command.AppendArg("--tag-key");
  ksadmin_command.AppendArg("KSChannelID");
  ksadmin_command.AppendArg("--tag-path");
  ksadmin_command.AppendArgPath(browser_plist);
  ksadmin_command.AppendArg("--version");
  ksadmin_command.AppendArg(*browser_version);
  ksadmin_command.AppendArg("--version-key");
  ksadmin_command.AppendArg("KSVersion");
  ksadmin_command.AppendArg("--version-path");
  ksadmin_command.AppendArgPath(browser_path.Append("Contents/Info.plist"));
  ksadmin_command.AppendArg("--brand-path");
  ksadmin_command.AppendArg("/Library/" COMPANY_SHORTNAME_STRING
                            "/" BROWSER_PRODUCT_NAME_STRING " Brand.plist");
  ksadmin_command.AppendArg("--brand-key");
  ksadmin_command.AppendArg("KSBrandID");
  ksadmin_command.AppendArg("--xcpath");
  ksadmin_command.AppendArgPath(browser_path);
  ksadmin_command.AppendArg("--system-store");
  if (!base::GetAppOutputWithExitCode(ksadmin_command, &output, &exit_code)) {
    return kFailedToRegister;
  }
  if (exit_code) {
    VLOG(0) << "Output from attempting to register the browser: " << output;
    VLOG(0) << "Exit code: " << exit_code;
    return exit_code;
  }
  return 0;
}

bool VerifyUpdaterSignature(const base::FilePath& updater_app_bundle) {
  base::apple::ScopedCFTypeRef<SecRequirementRef> requirement;
  base::apple::ScopedCFTypeRef<SecStaticCodeRef> code;
  base::apple::ScopedCFTypeRef<CFErrorRef> errors;
  if (SecStaticCodeCreateWithPath(
          base::apple::FilePathToCFURL(updater_app_bundle).get(),
          kSecCSDefaultFlags, code.InitializeInto()) != errSecSuccess) {
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
          code.get(), kSecCSCheckAllArchitectures | kSecCSCheckNestedCode,
          requirement.get(), errors.InitializeInto()) != errSecSuccess) {
    return false;
  }
  return true;
}

PrivilegedHelperService::PrivilegedHelperService() = default;
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
