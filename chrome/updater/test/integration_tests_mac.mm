// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/mac/privileged_helper/service.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#import "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater::test {
namespace {

base::FilePath GetExecutablePath() {
  base::FilePath out_dir;
  if (!base::PathService::Get(base::DIR_EXE, &out_dir))
    return base::FilePath();
  return out_dir.Append(GetExecutableRelativePath());
}

std::optional<base::FilePath> GetActiveFile(UpdaterScope /*scope*/,
                                            const std::string& id) {
  // The active user is always managed in the updater scope for the user.
  const std::optional<base::FilePath> path =
      GetLibraryFolderPath(UpdaterScope::kUser);
  if (!path)
    return std::nullopt;

  return path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

}  // namespace

base::FilePath GetSetupExecutablePath() {
  // There is no metainstaller on mac, use the main executable for setup.
  return GetExecutablePath();
}

void EnterTestMode(const GURL& update_url,
                   const GURL& crash_upload_url,
                   const GURL& device_management_url,
                   const base::TimeDelta& idle_timeout) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL(std::vector<std::string>{update_url.spec()})
                  .SetCrashUploadURL(crash_upload_url.spec())
                  .SetDeviceManagementURL(device_management_url.spec())
                  .SetUseCUP(false)
                  .SetInitialDelay(base::Milliseconds(100))
                  .SetServerKeepAliveTime(base::Seconds(2))
                  .SetCrxVerifierFormat(crx_file::VerifierFormat::CRX3)
                  .SetOverinstallTimeout(base::Seconds(5))
                  .SetIdleCheckPeriod(idle_timeout)
                  .Modify());
}

void Clean(UpdaterScope scope) {
  CleanProcesses();

  std::optional<base::FilePath> path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  }
  EXPECT_TRUE(base::DeleteFile(*GetWakeTaskPlistPath(scope)));

  path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  }

  std::optional<base::FilePath> keystone_path = GetKeystoneFolderPath(scope);
  EXPECT_TRUE(keystone_path);
  if (keystone_path) {
    EXPECT_TRUE(base::DeletePathRecursively(*keystone_path));
  }

  std::optional<base::FilePath> cache_path = GetCacheBaseDirectory(scope);
  EXPECT_TRUE(cache_path);
  if (cache_path) {
    EXPECT_TRUE(base::DeletePathRecursively(*cache_path));
  }
  EXPECT_TRUE(RemoveWakeJobFromLaunchd(scope));

  // Also clean up any other versions of the updater that are around.
  base::CommandLine launchctl(base::FilePath("/bin/launchctl"));
  launchctl.AppendArg("list");
  std::string out;
  ASSERT_TRUE(base::GetAppOutput(launchctl, &out));
  for (const auto& token : base::SplitStringPiece(out, base::kWhitespaceASCII,
                                                  base::TRIM_WHITESPACE,
                                                  base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(token, MAC_BUNDLE_IDENTIFIER_STRING)) {
      std::string out_rm;
      base::CommandLine launchctl_rm(base::FilePath("/bin/launchctl"));
      launchctl_rm.AppendArg("remove");
      launchctl_rm.AppendArg(token);
      ASSERT_TRUE(base::GetAppOutput(launchctl_rm, &out_rm));
    }
  }
}

void DeleteLegacyUpdater(UpdaterScope scope) {
  std::optional<base::FilePath> keystone = GetKeystoneFolderPath(scope);
  ASSERT_TRUE(keystone);
  ASSERT_TRUE(base::DeletePathRecursively(*keystone));
}

void ExpectClean(UpdaterScope scope) {
  ExpectCleanProcesses();

  // Files must not exist on the file system.
  EXPECT_FALSE(base::PathExists(*GetWakeTaskPlistPath(scope)));

  // Caches must have been removed. On Mac, this is separate from other
  // updater directories, so we can reliably remove it completely.
  std::optional<base::FilePath> cache_path = GetCacheBaseDirectory(scope);
  EXPECT_TRUE(cache_path);
  if (cache_path) {
    EXPECT_FALSE(base::PathExists(*cache_path));
  }

  std::optional<base::FilePath> path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path && base::PathExists(*path)) {
    // If the path exists, then expect only the log and json files to be
    // present.
    int count = CountDirectoryFiles(*path);
    EXPECT_LE(count, 1) << base::JoinString(
        [](const base::FilePath& dir) {
          std::vector<base::FilePath::StringType> files;
          base::FileEnumerator(dir, false, base::FileEnumerator::FILES)
              .ForEach([&files](const base::FilePath& name) {
                files.push_back(name.value());
              });

          return files;
        }(*path),
        FILE_PATH_LITERAL(","));

    if (count >= 1) {
      EXPECT_TRUE(base::PathExists(path->AppendASCII("updater.log")));
    }
  }
  // Keystone must not exist on the file system.
  std::optional<base::FilePath> keystone_path = GetKeystoneFolderPath(scope);
  EXPECT_TRUE(keystone_path);
  if (keystone_path) {
    EXPECT_FALSE(
        base::PathExists(keystone_path->AppendASCII(KEYSTONE_NAME ".bundle")));
  }
}

void ExpectInstalled(UpdaterScope scope) {
  std::optional<base::FilePath> keystone_path = GetKeystoneFolderPath(scope);
  ASSERT_TRUE(keystone_path);

  // Files must exist on the file system.
  for (const auto& path :
       {GetInstallDirectory(scope), keystone_path, GetKSAdminPath(scope)}) {
    ASSERT_TRUE(path) << path;
    EXPECT_TRUE(base::PathExists(*path)) << path;
  }

  EXPECT_TRUE(base::PathExists(*GetWakeTaskPlistPath(scope)));
}

std::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  return GetUpdaterExecutablePath(scope);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  std::optional<base::FilePath> versioned_folder_path =
      GetVersionedInstallDirectory(scope);
  ASSERT_TRUE(versioned_folder_path);
  EXPECT_FALSE(base::PathExists(*versioned_folder_path));
}

void Uninstall(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetExecutablePath();
  ASSERT_TRUE(path);
  base::CommandLine command_line(*path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, 0);
}

void SetActive(UpdaterScope scope, const std::string& app_id) {
  const std::optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  VLOG(0) << "Actives file: " << *path;
  base::File::Error err = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path->DirName(), &err))
      << "Error: " << err;
  EXPECT_TRUE(base::WriteFile(*path, ""));
}

void ExpectActive(UpdaterScope scope, const std::string& app_id) {
  const std::optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  EXPECT_TRUE(base::PathExists(*path));
  EXPECT_TRUE(base::PathIsWritable(*path));
}

void ExpectNotActive(UpdaterScope scope, const std::string& app_id) {
  const std::optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(base::PathIsWritable(*path));
}

bool WaitForUpdaterExit(UpdaterScope /*scope*/) {
  return WaitFor(
      [] {
        std::string ps_stdout;
        EXPECT_TRUE(
            base::GetAppOutput({"ps", "ax", "-o", "command"}, &ps_stdout));
        if (ps_stdout.find(GetExecutablePath().BaseName().AsUTF8Unsafe()) ==
            std::string::npos) {
          return true;
        }
        return false;
      },
      [] { VLOG(0) << "Still waiting for updater to exit..."; });
}

void SetupRealUpdaterLowerVersion(UpdaterScope scope) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath old_updater_path = exe_path.Append("old_updater");
#if BUILDFLAG(CHROMIUM_BRANDING)
#if defined(ARCH_CPU_ARM64)
  old_updater_path = old_updater_path.Append("chromium_mac_arm64");
#elif defined(ARCH_CPU_X86_64)
  old_updater_path = old_updater_path.Append("chromium_mac_amd64");
#endif
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  old_updater_path = old_updater_path.Append("chrome_mac_universal");
#endif
  base::CommandLine command_line(
      old_updater_path.Append(PRODUCT_FULLNAME_STRING "_test.app")
          .Append("Contents")
          .Append("MacOS")
          .Append(PRODUCT_FULLNAME_STRING "_test"));
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, 0);
}

void SetupFakeLegacyUpdater(UpdaterScope scope) {
  base::FilePath updater_test_data_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &updater_test_data_path));
  updater_test_data_path =
      updater_test_data_path.Append(FILE_PATH_LITERAL("updater"));

  base::FilePath keystone_path = GetKeystoneFolderPath(scope).value();
  base::FilePath keystone_ticket_store_path =
      keystone_path.Append(FILE_PATH_LITERAL("TicketStore"));
  ASSERT_TRUE(base::CreateDirectory(keystone_ticket_store_path));
  ASSERT_TRUE(base::CopyFile(
      updater_test_data_path.AppendASCII("Keystone.legacy.ticketstore"),
      keystone_ticket_store_path.AppendASCII("Keystone.ticketstore")));
  ASSERT_TRUE(base::CopyFile(
      updater_test_data_path.AppendASCII("CountingMetrics.plist"),
      keystone_path.AppendASCII("CountingMetrics.plist")));
}

void ExpectLegacyUpdaterMigrated(UpdaterScope scope) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  auto persisted_data = base::MakeRefCounted<PersistedData>(
      scope, global_prefs->GetPrefService());

  // Keystone should not be migrated.
  EXPECT_FALSE(
      persisted_data->GetProductVersion("com.google.keystone").IsValid());

  // Uninstalled app should be migrated.
  EXPECT_TRUE(
      persisted_data->GetProductVersion("com.chromium.NonExistApp").IsValid());

  // App Kipple.
  const std::string kKippleApp = "com.chromium.kipple";
  EXPECT_EQ(persisted_data->GetProductVersion(kKippleApp),
            base::Version("1.2.3.4"));
  EXPECT_EQ(persisted_data->GetExistenceCheckerPath(kKippleApp),
            base::FilePath("/"));
  EXPECT_TRUE(persisted_data->GetAP(kKippleApp).empty());
  EXPECT_TRUE(persisted_data->GetBrandCode(kKippleApp).empty());
  EXPECT_TRUE(persisted_data->GetBrandPath(kKippleApp).empty());
  EXPECT_TRUE(persisted_data->GetFingerprint(kKippleApp).empty());
  EXPECT_FALSE(persisted_data->GetDateLastActive(kKippleApp));    // no data.
  EXPECT_FALSE(persisted_data->GetDateLastRollcall(kKippleApp));  // wrong type.

  // App PopularApp.
  const std::string kPopularApp = "com.chromium.PopularApp";
  EXPECT_EQ(persisted_data->GetProductVersion(kPopularApp),
            base::Version("101.100.1000.9999"));
  EXPECT_EQ(persisted_data->GetExistenceCheckerPath(kPopularApp),
            base::FilePath("/"));
  EXPECT_EQ(persisted_data->GetAP(kPopularApp), "GOOG");
  EXPECT_TRUE(persisted_data->GetBrandCode(kPopularApp).empty());
  EXPECT_EQ(persisted_data->GetBrandPath(kPopularApp), base::FilePath("/"));
  EXPECT_TRUE(persisted_data->GetFingerprint(kPopularApp).empty());
  EXPECT_EQ(persisted_data->GetDateLastActive(kPopularApp).value(), 5921);
  EXPECT_EQ(persisted_data->GetDateLastRollcall(kPopularApp).value(), 5922);

  EXPECT_EQ(persisted_data->GetCohort(kPopularApp), "TestCohort");
  EXPECT_EQ(persisted_data->GetCohortName(kPopularApp), "TestCohortName");
  EXPECT_EQ(persisted_data->GetCohortHint(kPopularApp), "TestCohortHint");

  // App CorruptedApp (client-regulated counting data is corrupted).
  const std::string kCorruptedApp = "com.chromium.CorruptedApp";
  EXPECT_EQ(persisted_data->GetProductVersion(kCorruptedApp),
            base::Version("1.2.1"));
  EXPECT_EQ(persisted_data->GetExistenceCheckerPath(kCorruptedApp),
            base::FilePath("/"));
  EXPECT_EQ(persisted_data->GetAP(kCorruptedApp), "canary");
  EXPECT_FALSE(persisted_data->GetDateLastActive(kCorruptedApp));
  EXPECT_FALSE(persisted_data->GetDateLastRollcall(kCorruptedApp));
}

void InstallApp(UpdaterScope scope,
                const std::string& app_id,
                const base::Version& version) {
  RegisterApp(scope, app_id, version);
}

void UninstallApp(UpdaterScope scope, const std::string& app_id) {
  const base::FilePath& install_path =
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefs(scope)->GetPrefService())
          ->GetExistenceCheckerPath(app_id);
  VLOG(1) << "Deleting app install path: " << install_path;
  base::DeletePathRecursively(install_path);
  SetExistenceCheckerPath(scope, app_id,
                          base::FilePath(FILE_PATH_LITERAL("NONE")));
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
  command_line.PrependWrapper("/usr/bin/sudo");
  return command_line;
}

void SetPlatformPolicies(const base::Value::Dict& values) {
  const CFStringRef domain = CFSTR("com.google.Keystone");

  // Synchronize just to be safe. Ignore spurious errors if the domain
  // does not yet exist.
  CFPreferencesSynchronize(domain, kCFPreferencesAnyUser,
                           kCFPreferencesCurrentHost);

  NSMutableDictionary* all_policies = [NSMutableDictionary dictionary];
  for (const auto [app_id, policies] : values) {
    ASSERT_TRUE(policies.is_dict());
    NSMutableDictionary* app_policies = [NSMutableDictionary dictionary];
    for (const auto [name, value] : policies.GetDict()) {
      NSString* key = base::SysUTF8ToNSString(name);
      if (value.is_string()) {
        app_policies[key] = base::SysUTF8ToNSString(value.GetString());
      } else if (value.is_int()) {
        app_policies[key] = [NSNumber numberWithInt:value.GetInt()];
      } else if (value.is_bool()) {
        app_policies[key] = [NSNumber numberWithInt:value.GetBool()];
      }
    }
    all_policies[base::SysUTF8ToNSString(app_id)] = app_policies;

    NSURL* const managed_preferences_url = base::apple::FilePathToNSURL(
        GetLibraryFolderPath(UpdaterScope::kSystem)
            ->AppendASCII("Managed Preferences")
            .AppendASCII("com.google.Keystone.plist"));
    ASSERT_TRUE([[NSDictionary dictionaryWithObject:all_policies
                                             forKey:@"updatePolicies"]
        writeToURL:managed_preferences_url
        atomically:YES])
        << "Failed to write " << managed_preferences_url;
  }
  ASSERT_TRUE(CFPreferencesSynchronize(domain, kCFPreferencesAnyUser,
                                       kCFPreferencesCurrentHost));

  // Force flushing preferences cache by killing the defaults server.
  base::Process process = base::LaunchProcess({"killall", "cfprefsd"}, {});
  if (!process.IsValid()) {
    VLOG(2) << "Failed to launch the process to refresh preferences.";
  }
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

void PrivilegedHelperInstall(UpdaterScope scope) {
  ASSERT_EQ(scope, UpdaterScope::kSystem)
      << "The privileged helper only works at system scope.";
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath helpers_dir = temp_dir.GetPath().Append(
      "Contents/Frameworks/" BROWSER_PRODUCT_NAME_STRING
      " Framework.framework/Helpers/");
  ASSERT_TRUE(base::CreateDirectory(helpers_dir));
  ASSERT_TRUE(CopyDir(src_dir.Append("third_party")
                          .Append("updater")
                          .Append("chrome_mac_universal_prod")
                          .Append(PRODUCT_FULLNAME_STRING ".app"),
                      helpers_dir, false));
  ASSERT_TRUE(
      base::WriteFile(temp_dir.GetPath().Append("Contents/Info.plist"),
                      R"(<?xml version="1.0" encoding="UTF-8"?>)"
                      R"(<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN")"
                      R"(    "http://www.apple.com/DTDs/PropertyList-1.0.dtd">)"
                      R"(<plist version="1.0">)"
                      R"(<dict>)"
                      R"(<key>KSProductID</key>)"
                      R"(<string>test1</string>)"
                      R"(<key>KSChannelID</key>)"
                      R"(<string>tag</string>)"
                      R"(<key>KSVersion</key>)"
                      R"(<string>1.2.3.4</string>)"
                      R"(</dict>)"
                      R"(</plist>)"));
  ASSERT_TRUE(VerifyUpdaterSignature(
      helpers_dir.Append(PRODUCT_FULLNAME_STRING ".app")));
  ASSERT_EQ(InstallUpdater(temp_dir.GetPath()), 0);
}

}  // namespace updater::test
