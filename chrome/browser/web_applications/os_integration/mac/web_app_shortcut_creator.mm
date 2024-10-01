// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/info_plist_data.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/launch.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/shortcuts/platform_util_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/bundle_info_plist.h"
#include "chrome/browser/web_applications/os_integration/mac/icns_encoder.h"
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#import "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

#if defined(COMPONENT_BUILD)
#include <mach-o/loader.h>

#include "base/base_paths.h"
#include "base/bits.h"
#include "base/path_service.h"
#endif

// <https://github.com/apple-oss-distributions/Security/blob/Security-60420.101.4/OSX/libsecurity_codesigning/lib/SecCodeSigner.h>
extern "C" {

extern const CFStringRef kSecCodeSignerFlags;
extern const CFStringRef kSecCodeSignerIdentity;
extern const CFStringRef kSecCodeSignerEntitlements;

const uint32_t kSecCodeMagicEntitlement = 0xfade7171;

typedef struct __SecCodeSigner* SecCodeSignerRef;

OSStatus SecCodeSignerCreate(CFDictionaryRef parameters,
                             SecCSFlags flags,
                             SecCodeSignerRef* signer);

OSStatus SecCodeSignerAddSignatureWithErrors(SecCodeSignerRef signer,
                                             SecStaticCodeRef code,
                                             SecCSFlags flags,
                                             CFErrorRef* errors);

}  // extern "C"

namespace web_app {

BASE_FEATURE(kWebAppMaskableIconsOnMac,
             "WebAppMaskableIconsOnMac",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// Result of creating app shortcut.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreateShortcutResult {
  kSuccess = 0,
  kApplicationDirNotFound = 1,
  // Obsolete: kFailToLocalizeApplication = 2,
  // Obsolete: kFailToGetApplicationPaths = 3,
  kFailToCreateTempDir = 4,
  kStagingDirectoryNotExist = 5,
  kFailToCreateExecutablePath = 6,
  kFailToCopyExecutablePath = 7,
  kFailToCopyPlist = 8,
  kFailToWritePkgInfoFile = 9,
  kFailToUpdatePlist = 10,
  kFailToUpdateDisplayName = 11,
  kFailToUpdateIcon = 12,
  kFailToCreateParentDir = 13,
  kFailToCopyApp = 14,
  kFailToSign = 15,
  kMaxValue = kFailToSign,
};

// Records the result of creating shortcut to UMA.
void RecordCreateShortcut(CreateShortcutResult result) {
  base::UmaHistogramEnumeration("Apps.CreateShortcuts.Mac.Result2", result);
}

#if defined(COMPONENT_BUILD)
// Adds `new_rpath` to the paths the binary at `executable_path` will look at
// when loading shared libraries. Assumes there is enough room in the headers of
// the binary to fit the added path.
bool AddPathToRPath(const base::FilePath& executable_path,
                    const base::FilePath& new_rpath) {
  rpath_command new_rpath_command;
  new_rpath_command.cmd = LC_RPATH;
  // Size is size of the command struct + size of the path + a null terminator,
  // all rounded up to a multiple of 8 bytes.
  new_rpath_command.cmdsize = base::bits::AlignUp<uint32_t>(
      sizeof new_rpath_command + new_rpath.value().size() + 1, 8);
  new_rpath_command.path.offset = sizeof new_rpath_command;

  base::File executable_file(executable_path, base::File::FLAG_OPEN |
                                                  base::File::FLAG_WRITE |
                                                  base::File::FLAG_READ);
  if (!executable_file.IsValid()) {
    LOG(ERROR) << "Failed to open executable file at: " << executable_path
               << ", error: " << executable_file.error_details();
    return false;
  }

  mach_header_64 header;
  if (!executable_file.ReadAtCurrentPosAndCheck(
          base::as_writable_bytes(base::span_from_ref(header))) ||
      header.magic != MH_MAGIC_64 || header.filetype != MH_EXECUTE) {
    LOG(ERROR) << "File at " << executable_path
               << " is not a valid Mach-O executable";
    return false;
  }

  // Read existing load commands.
  std::vector<uint8_t> commands(header.sizeofcmds);
  if (!executable_file.ReadAtCurrentPosAndCheck(base::make_span(commands))) {
    LOG(ERROR) << "Failed to read load commands from " << executable_path;
    return false;
  }

  // Scan over the commands, finding the first LC_RPATH command. We'll insert
  // our new command right after it.
  auto commands_it = commands.begin();
  for (unsigned i = 0; i < header.ncmds; ++i) {
    load_command cmd;
    if (commands.end() - commands_it < int{sizeof cmd}) {
      LOG(ERROR) << "Reached end of commands before getting all commands";
      return false;
    }
    memcpy(&cmd, &*commands_it, sizeof cmd);
    if (commands.end() - commands_it < cmd.cmdsize) {
      LOG(ERROR) << "Command ends past the end of the load commands";
      return false;
    }
    commands_it += cmd.cmdsize;

    if (cmd.cmd == LC_RPATH) {
      // Insert the new command, padding the extra space with `0` bytes.
      auto it = commands.insert(commands_it, new_rpath_command.cmdsize, 0);
      memcpy(&*it, &new_rpath_command, sizeof new_rpath_command);
      memcpy(&*it + sizeof new_rpath_command, new_rpath.value().data(),
             new_rpath.value().size());

      header.ncmds++;
      header.sizeofcmds += new_rpath_command.cmdsize;

      // Write the updated header and commands back to the file.
      if (!executable_file.WriteAndCheck(0, base::byte_span_from_ref(header)) ||
          !executable_file.WriteAndCheck(sizeof header,
                                         base::make_span(commands))) {
        LOG(ERROR) << "Failed to write updated load commands to "
                   << executable_path;
        return false;
      }

      executable_file.Close();

      // And finally re-sign the resulting binary.
      std::string codesign_output;
      std::vector<std::string> codesign_argv = {"codesign", "--force", "--sign",
                                                "-", executable_path.value()};
      if (!base::GetAppOutputAndError(base::CommandLine(codesign_argv),
                                      &codesign_output)) {
        LOG(ERROR) << "Failed to sign executable at " << executable_path << ": "
                   << codesign_output;
        return false;
      }

      return true;
    }
  }
  LOG(ERROR) << "Did not find any LC_RPATH commands in " << executable_path;
  return false;
}
#endif

// Returns a reference to the static UpdateShortcuts lock.
// See https://crbug.com/1090548 for more info.
base::Lock& GetUpdateShortcutsLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

bool AppShimRevealDisabledForTest() {
  // Disable app shim reveal in the Finder during tests, to avoid
  // creating Finder windows that are never closed.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kTestType) ||
         OsIntegrationTestOverride::Get();
}

bool CopyStagingBundleToDestination(bool use_ad_hoc_signing_for_web_app_shims,
                                    base::FilePath staging_path,
                                    base::FilePath dst_app_path) {
  if (!use_ad_hoc_signing_for_web_app_shims) {
    return base::CopyDirectory(staging_path, dst_app_path, true);
  }

  // When using ad-hoc signing for web app shims, the final app shim must be
  // written to disk by a separate helper tool. This helper tool is used
  // so that binary authorization tools, such as Santa, can transitively trust
  // app shims that it creates without trusting all files written by Chrome.
  // This allows app shims to be trusted by the binary authorization tool
  // despite having only ad-hoc code signatures.

  base::FilePath web_app_shortcut_copier_path =
      base::apple::FrameworkBundlePath().Append("Helpers").Append(
          "web_app_shortcut_copier");
  base::CommandLine command_line(web_app_shortcut_copier_path);
  command_line.AppendArgPath(staging_path);
  command_line.AppendArgPath(dst_app_path);

  // Pass NSBundle's cached copy of the app's Info.plist data to the helper tool
  // for use in dynamic signature validation. The data is validated against a
  // hash recorded in the code signature before being used during requirement
  // validation.
  std::vector<uint8_t> info_plist_data =
      base::mac::OuterBundleCachedInfoPlistData();
  command_line.AppendArg(base::as_string_view(info_plist_data));

  // Synchronously wait for the copy to complete to match the semantics of
  // `base::CopyDirectory`.
  std::string command_output;
  int exit_code;
  if (base::GetAppOutputWithExitCode(command_line, &command_output,
                                     &exit_code)) {
    return !exit_code;
  }

  return false;
}

// Remove the leading . from the entries of |extensions|. Any items that do not
// have a leading . are removed.
std::set<std::string> GetFileHandlerExtensionsWithoutDot(
    const std::set<std::string>& file_extensions) {
  std::set<std::string> result;
  for (const auto& file_extension : file_extensions) {
    if (file_extension.length() <= 1 || file_extension[0] != '.') {
      continue;
    }
    result.insert(file_extension.substr(1));
  }
  return result;
}

// Given the path to an app bundle, return the resources directory.
base::FilePath GetResourcesPath(const base::FilePath& app_path) {
  return app_path.Append("Contents").Append("Resources");
}

// Given the path to an app bundle, return the URL of the Info.plist file.
NSURL* GetPlistURL(const base::FilePath& bundle_path) {
  return base::apple::FilePathToNSURL(
      bundle_path.Append("Contents").Append("Info.plist"));
}

bool HasExistingExtensionShimForDifferentProfile(
    const base::FilePath& destination_directory,
    const std::string& extension_id,
    const base::FilePath& profile_dir) {
  std::list<BundleInfoPlist> bundles_info =
      BundleInfoPlist::GetAllInPath(destination_directory, /*recursive=*/false);
  for (const auto& info : bundles_info) {
    if (info.GetExtensionId() == extension_id &&
        !info.IsForProfile(profile_dir)) {
      return true;
    }
  }
  return false;
}

base::FilePath GetMultiProfileAppDataDir(base::FilePath app_data_dir) {
  // The kCrAppModeUserDataDirKey is expected to be a path in kWebAppDirname,
  // and the true user data dir is extracted by going three directories up.
  // For profile-agnostic apps, remove this reference to the profile name.
  // TODO(crbug.com/40656955): Do not specify kCrAppModeUserDataDirKey
  // if Chrome is using the default user data dir.

  // Strip the app name directory.
  base::FilePath app_name_dir = app_data_dir.BaseName();
  app_data_dir = app_data_dir.DirName();

  // Strip kWebAppDirname.
  base::FilePath web_app_dir = app_data_dir.BaseName();
  app_data_dir = app_data_dir.DirName();

  // Strip the profile and replace it with kNewProfilePath.
  app_data_dir = app_data_dir.DirName();
  const std::string kNewProfilePath("-");
  return app_data_dir.Append(kNewProfilePath)
      .Append(web_app_dir)
      .Append(app_name_dir);
}

NSData* AppShimEntitlements() {
  // Entitlement data to disable library validation with the hardened runtime.
  // The first 8 bytes of the entitlement data consists of two 32-bit values:
  // a magic constant and the length of the data. They are populated below.
  char entitlement_bytes[] =
      R"xml(12345678<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
</dict>
</plist>
)xml";

  // The magic constant and length are expected to be big endian.
  uint32_t* entitlement_header = reinterpret_cast<uint32_t*>(entitlement_bytes);
  entitlement_header[0] = CFSwapInt32HostToBig(kSecCodeMagicEntitlement);
  entitlement_header[1] = CFSwapInt32HostToBig(sizeof(entitlement_bytes) - 1);

  return [NSData dataWithBytes:static_cast<void*>(entitlement_bytes)
                        length:sizeof(entitlement_bytes) - 1];
}

}  // namespace

WebAppShortcutCreator::WebAppShortcutCreator(
    const base::FilePath& app_data_dir,
    const base::FilePath& chrome_apps_dir,
    const ShortcutInfo* shortcut_info,
    bool use_ad_hoc_signing_for_web_app_shims)
    : app_data_dir_(app_data_dir),
      chrome_apps_dir_(chrome_apps_dir),
      info_(shortcut_info),
      use_ad_hoc_signing_for_web_app_shims_(
          use_ad_hoc_signing_for_web_app_shims) {
  DCHECK(shortcut_info);
}

WebAppShortcutCreator::~WebAppShortcutCreator() = default;

base::FilePath WebAppShortcutCreator::GetShortcutBasename() const {
  // For profile-less shortcuts, use the fallback naming scheme to avoid change.
  if (info_->profile_name.empty()) {
    return GetFallbackBasename();
  }

  std::u16string title = info_->title;
  std::optional<base::SafeBaseName> base_name =
      shortcuts::SanitizeTitleForFileName(base::UTF16ToUTF8(info_->title));
  if (!base_name.has_value()) {
    return GetFallbackBasename();
  }

  return base_name->path().AddExtension(".app");
}

base::FilePath WebAppShortcutCreator::GetFallbackBasename() const {
  std::string app_name;
  // Check if there should be a separate shortcut made for different profiles.
  // Such shortcuts will have a |profile_name| set on the ShortcutInfo,
  // otherwise it will be empty.
  if (!info_->profile_name.empty()) {
    app_name += info_->profile_path.BaseName().value();
    app_name += ' ';
  }
  app_name += info_->app_id;
  return base::FilePath(app_name).ReplaceExtension("app");
}

base::FilePath WebAppShortcutCreator::GetApplicationsShortcutPath(
    bool avoid_conflicts) const {
  base::FilePath applications_dir = chrome_apps_dir_;
  if (applications_dir.empty()) {
    return base::FilePath();
  }

  if (!avoid_conflicts) {
    return applications_dir.Append(GetShortcutBasename());
  }

  // Attempt to use the application's title for the file name.
  base::FilePath path = base::GetUniquePathWithSuffixFormat(
      applications_dir.Append(GetShortcutBasename()), " %d");
  if (!path.empty()) {
    return path;
  }

  // If all of those are taken, then use the combination of profile and
  // extension id.
  return applications_dir.Append(GetFallbackBasename());
}

std::vector<base::FilePath> WebAppShortcutCreator::GetAppBundlesById() const {
  std::vector<base::FilePath> paths = GetAppBundlesByIdUnsorted();

  // Sort the matches by preference.
  base::FilePath default_path =
      GetApplicationsShortcutPath(/*avoid_conflicts=*/false);

  base::FilePath apps_dir = chrome_apps_dir_;
  auto compare = [default_path, apps_dir](const base::FilePath& a,
                                          const base::FilePath& b) {
    if (a == b) {
      return false;
    }
    // The default install path is preferred above all others.
    if (a == default_path) {
      return true;
    }
    if (b == default_path) {
      return false;
    }
    // Paths in ~/Applications are preferred to paths not in ~/Applications.
    bool a_in_apps_dir = apps_dir.IsParent(a);
    bool b_in_apps_dir = apps_dir.IsParent(b);
    if (a_in_apps_dir != b_in_apps_dir) {
      return a_in_apps_dir > b_in_apps_dir;
    }
    return a < b;
  };
  std::sort(paths.begin(), paths.end(), compare);
  return paths;
}

std::string WebAppShortcutCreator::GetAppBundleId() const {
  return GetBundleIdentifierForShim(
      info_->app_id, IsMultiProfile() ? base::FilePath() : info_->profile_path);
}

bool WebAppShortcutCreator::CreateShortcuts(
    ShortcutCreationReason creation_reason,
    ShortcutLocations creation_locations) {
  DCHECK_NE(creation_locations.applications_menu_location,
            APP_MENU_LOCATION_HIDDEN);
  std::vector<base::FilePath> updated_app_paths;
  if (!UpdateShortcuts(/*create_if_needed=*/true, &updated_app_paths)) {
    return false;
  }
  if (creation_locations.in_startup) {
    // Only add the first app to run at OS login.
    WebAppAutoLoginUtil::GetInstance()->AddToLoginItems(updated_app_paths[0],
                                                        false);
  }
  if (creation_reason == SHORTCUT_CREATION_BY_USER) {
    RevealAppShimInFinder(updated_app_paths[0]);
  }
  RecordCreateShortcut(CreateShortcutResult::kSuccess);
  return true;
}

bool WebAppShortcutCreator::UpdateShortcuts(
    bool create_if_needed,
    std::vector<base::FilePath>* updated_paths) {
  DCHECK(updated_paths && updated_paths->empty());

  if (create_if_needed) {
    const base::FilePath applications_dir = chrome_apps_dir_;
    if (applications_dir.empty() ||
        !base::DirectoryExists(applications_dir.DirName())) {
      RecordCreateShortcut(CreateShortcutResult::kApplicationDirNotFound);
      LOG(ERROR) << "Couldn't find an Applications directory to copy app to.";
      return false;
    }
  }

  // Acquire the UpdateShortcuts lock.  This ensures only a single
  // UpdateShortcuts call at a time will run at once past here.  Not
  // protecting against that can result in multiple CreateShortcutsAt()
  // calls deleting and creating the app shim folder at once.
  // See https://crbug.com/1090548 for more info.
  base::AutoLock auto_lock(GetUpdateShortcutsLock());

  // Get the list of paths to (re)create by bundle id (wherever it was moved
  // or copied by the user).
  std::vector<base::FilePath> app_paths = GetAppBundlesById();

  // If that path does not exist, create a new entry in ~/Applications if
  // requested.
  if (app_paths.empty() && create_if_needed) {
    app_paths.push_back(GetApplicationsShortcutPath(/*avoid_conflicts=*/true));
  }
  if (app_paths.empty()) {
    // If `create_if_needed` is false, we've succesfully updated shortcuts if no
    // shortcuts have been found.
    return true;
  }

  CreateShortcutsAt(app_paths, updated_paths);
  return updated_paths->size() == app_paths.size();
}

void WebAppShortcutCreator::RevealAppShimInFinder(
    const base::FilePath& app_path) const {
  auto closure = base::BindOnce(
      [](const base::FilePath& app_path) {
        // The Finder creates a new window each time the app shim is revealed.
        // Skip revealing the app shim during testing to avoid an avalanche of
        // new Finder windows.
        if (AppShimRevealDisabledForTest()) {
          return;
        }
        NSURL* path_url = base::apple::FilePathToNSURL(app_path);
        [[NSWorkspace sharedWorkspace]
            activateFileViewerSelectingURLs:@[ path_url ]];
      },
      app_path);
  // Perform the call to NSWorkspace on the UI thread. Calling it on the IO
  // thread appears to cause crashes.
  // https://crbug.com/1067367
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(closure));
}

std::vector<base::FilePath> WebAppShortcutCreator::GetAppBundlesByIdUnsorted()
    const {
  // Search using LaunchServices using the default bundle id.
  const std::string bundle_id = GetBundleIdentifierForShim(
      info_->app_id, IsMultiProfile() ? base::FilePath() : info_->profile_path);
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, chrome_apps_dir_);

  // If in multi-profile mode, search using the profile-scoped bundle id, in
  // case the user has an old shim hanging around.
  if (bundle_infos.empty() && IsMultiProfile()) {
    const std::string profile_scoped_bundle_id =
        GetBundleIdentifierForShim(info_->app_id, info_->profile_path);
    bundle_infos = BundleInfoPlist::SearchForBundlesById(
        profile_scoped_bundle_id, chrome_apps_dir_);
  }

  std::vector<base::FilePath> bundle_paths;
  for (const auto& bundle_info : bundle_infos) {
    bundle_paths.push_back(bundle_info.bundle_path());
  }
  return bundle_paths;
}

bool WebAppShortcutCreator::IsMultiProfile() const {
  return info_->is_multi_profile;
}

bool WebAppShortcutCreator::BuildShortcut(
    const base::FilePath& staging_path) const {
  if (!base::DirectoryExists(staging_path.DirName())) {
    RecordCreateShortcut(CreateShortcutResult::kStagingDirectoryNotExist);
    LOG(ERROR) << "Staging path directory does not exist: "
               << staging_path.DirName();
    return false;
  }

  const base::FilePath framework_bundle_path =
      base::apple::FrameworkBundlePath();

  const base::FilePath executable_path =
      framework_bundle_path.Append("Helpers").Append("app_mode_loader");
  const base::FilePath plist_path =
      framework_bundle_path.Append("Resources").Append("app_mode-Info.plist");

  const base::FilePath destination_contents_path =
      staging_path.Append("Contents");
  const base::FilePath destination_executable_path =
      destination_contents_path.Append("MacOS");

  // First create the .app bundle directory structure.
  // Use NSFileManager so that the permissions can be set appropriately. The
  // base::CreateDirectory() routine forces mode 0700.
  NSError* error = nil;
  if (![NSFileManager.defaultManager
                 createDirectoryAtURL:base::apple::FilePathToNSURL(
                                          destination_executable_path)
          withIntermediateDirectories:YES
                           attributes:@{
                             NSFilePosixPermissions : @(0755)
                           }
                                error:&error]) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCreateExecutablePath);
    LOG(ERROR) << "Failed to create destination executable path: "
               << destination_executable_path
               << ", error=" << base::SysNSStringToUTF8([error description]);
    return false;
  }

  // Copy the executable file.
  if (!base::CopyFile(executable_path, destination_executable_path.Append(
                                           executable_path.BaseName()))) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCopyExecutablePath);
    LOG(ERROR) << "Failed to copy executable: " << executable_path;
    return false;
  }

#if defined(COMPONENT_BUILD)
  // Test bots could have the build in a different path than where it was on a
  // build bot. If this is the case in a component build, we'll need to fix the
  // rpath of app_mode_loader to make sure it can still find its dynamic
  // libraries.
  base::FilePath rpath_to_add;
  if (!base::PathService::Get(base::DIR_MODULE, &rpath_to_add)) {
    LOG(ERROR) << "Failed to get module path";
    return false;
  }
  if (!AddPathToRPath(
          destination_executable_path.Append(executable_path.BaseName()),
          rpath_to_add)) {
    return false;
  }
#endif

#if defined(ADDRESS_SANITIZER)
  const base::FilePath asan_library_path =
      framework_bundle_path.Append("Versions")
          .Append("Current")
          .Append("libclang_rt.asan_osx_dynamic.dylib");
  if (!base::CopyFile(asan_library_path, destination_executable_path.Append(
                                             asan_library_path.BaseName()))) {
    LOG(ERROR) << "Failed to copy asan library: " << asan_library_path;
    return false;
  }

  // The address sanitizer runtime must have a valid signature in order for the
  // containing app bundle to be signed. On Apple Silicon the address sanitizer
  // runtime library has a linker-generated ad-hoc code signature, but this is
  // treated as equivalent to being unsigned when signing the containing app
  // bundle.
  std::string codesign_output;
  std::vector<std::string> codesign_argv = {
      "codesign", "--sign", "-",
      destination_executable_path.Append(asan_library_path.BaseName()).value()};
  CHECK(base::GetAppOutputAndError(base::CommandLine(codesign_argv),
                                   &codesign_output))
      << "Failed to sign executable at "
      << destination_executable_path.Append(asan_library_path.BaseName())
             .value()
      << ": " << codesign_output;
#endif

  // Copy the Info.plist.
  if (!base::CopyFile(plist_path,
                      destination_contents_path.Append("Info.plist"))) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCopyPlist);
    LOG(ERROR) << "Failed to copy plist: " << plist_path;
    return false;
  }

  // Write the PkgInfo file.
  constexpr char kPkgInfoData[] = "APPL????";
  if (!base::WriteFile(destination_contents_path.Append("PkgInfo"),
                       kPkgInfoData)) {
    RecordCreateShortcut(CreateShortcutResult::kFailToWritePkgInfoFile);
    LOG(ERROR) << "Failed to write PkgInfo file: " << destination_contents_path;
    return false;
  }

  bool result = UpdatePlist(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToUpdatePlist);
    return result;
  }
  result = UpdateDisplayName(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToUpdateDisplayName);
    return result;
  }
  result = UpdateIcon(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToUpdateIcon);
  }
  result = UpdateSignature(staging_path);
  if (!result) {
    RecordCreateShortcut(CreateShortcutResult::kFailToSign);
  }
  return result;
}

void WebAppShortcutCreator::CreateShortcutsAt(
    const std::vector<base::FilePath>& dst_app_paths,
    std::vector<base::FilePath>* updated_paths) const {
  DCHECK(updated_paths && updated_paths->empty());
  DCHECK(!dst_app_paths.empty());

  // CreateShortcutsAt() modifies the app shim on disk, first by deleting
  // the destination app shim (if it exists), then by copying a new app shim
  // from the source app to the destination.  To ensure that process works,
  // we must guarantee that no more than one CreateShortcutsAt() call will
  // ever run at a time.  We have an UpdateShortcuts lock for this purpose,
  // so check that lock has been acquired on this thread before proceeding.
  // See https://crbug.com/1090548 for more info.
  GetUpdateShortcutsLock().AssertAcquired();

  base::ScopedTempDir scoped_temp_dir;
  if (!scoped_temp_dir.CreateUniqueTempDir()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCreateTempDir);
    return;
  }

  // Create the bundle in |staging_path|. Note that the staging path will be
  // encoded in CFBundleName, and only .apps with that exact name will have
  // their display name overridden by localization. To that end, use the base
  // name from dst_app_paths.front(), to ensure that the Applications copy has
  // its display name set appropriately.
  base::FilePath staging_path =
      scoped_temp_dir.GetPath().Append(dst_app_paths.front().BaseName());
  if (!BuildShortcut(staging_path)) {
    return;
  }

  // Copy to each destination in |dst_app_paths|.
  for (const auto& dst_app_path : dst_app_paths) {
    // Create the parent directory for the app.
    base::FilePath dst_parent_dir = dst_app_path.DirName();
    if (!base::CreateDirectory(dst_parent_dir)) {
      RecordCreateShortcut(CreateShortcutResult::kFailToCreateParentDir);
      LOG(ERROR) << "Creating directory " << dst_parent_dir.value()
                 << " failed.";
      continue;
    }

    // Delete any old copies that may exist.
    base::DeletePathRecursively(dst_app_path);

    // Copy the bundle to |dst_app_path|.
    if (!CopyStagingBundleToDestination(UseAdHocSigningForWebAppShims(),
                                        staging_path, dst_app_path)) {
      RecordCreateShortcut(CreateShortcutResult::kFailToCopyApp);
      LOG(ERROR) << "Copying app to dst dir: " << dst_parent_dir.value()
                 << " failed";
      continue;
    }

    // Remove the quarantine attribute from both the bundle and the executable.
    base::mac::RemoveQuarantineAttribute(dst_app_path);
    base::mac::RemoveQuarantineAttribute(dst_app_path.Append("Contents")
                                             .Append("MacOS")
                                             .Append("app_mode_loader"));

    // LaunchServices will eventually detect the (updated) app, but explicitly
    // calling LSRegisterURL ensures tests see the right state immediately.
    LSRegisterURL(base::apple::FilePathToCFURL(dst_app_path).get(), true);

    updated_paths->push_back(dst_app_path);
  }
}

bool WebAppShortcutCreator::UpdateDisplayName(
    const base::FilePath& app_path) const {
  // Localization is used to display the app name (rather than the bundle
  // filename). macOS searches for the best language in the order of preferred
  // languages, but one of them must be found otherwise it will default to
  // the filename.
  NSString* language = NSLocale.preferredLanguages[0];
  base::FilePath localized_dir = GetResourcesPath(app_path).Append(
      base::SysNSStringToUTF8(language) + ".lproj");
  if (!base::CreateDirectory(localized_dir)) {
    return false;
  }

  // Colon is not a valid token in the display name, and although it will be
  // shown correctly, the user has to remove it if they want to rename the
  // app bundle. Therefore we just remove it. Note also that the OS will
  // collapse multiple consecutive forward-slashes in the display name into one.
  std::u16string title_normalized = info_->title;
  base::RemoveChars(title_normalized, u":", &title_normalized);

  NSString* bundle_name = base::SysUTF16ToNSString(info_->title);
  NSString* display_name = base::SysUTF16ToNSString(title_normalized);

  if (!IsMultiProfile() &&
      HasExistingExtensionShimForDifferentProfile(
          chrome_apps_dir_, info_->app_id, info_->profile_path)) {
    display_name = [bundle_name
        stringByAppendingString:base::SysUTF8ToNSString(
                                    " (" + info_->profile_name + ")")];
  }

  NSDictionary* strings_plist = @{
    base::apple::CFToNSPtrCast(kCFBundleNameKey) : bundle_name,
    app_mode::kCFBundleDisplayNameKey : display_name
  };

  NSURL* localized_url =
      base::apple::FilePathToNSURL(localized_dir.Append("InfoPlist.strings"));
  return [strings_plist writeToURL:localized_url error:nil];
}

bool WebAppShortcutCreator::UpdatePlist(const base::FilePath& app_path) const {
  NSString* app_id = base::SysUTF8ToNSString(info_->app_id);
  NSString* extension_title = base::SysUTF16ToNSString(info_->title);
  NSString* extension_url = base::SysUTF8ToNSString(info_->url.spec());
  NSString* chrome_bundle_id =
      base::SysUTF8ToNSString(base::apple::BaseBundleID());
  NSDictionary* replacement_dict = @{
    app_mode::kShortcutIdPlaceholder : app_id,
    app_mode::kShortcutNamePlaceholder : extension_title,
    app_mode::kShortcutURLPlaceholder : extension_url,
    app_mode::kShortcutBrowserBundleIDPlaceholder : chrome_bundle_id
  };

  NSURL* plist_url = GetPlistURL(app_path);
  NSMutableDictionary* plist =
      [[NSMutableDictionary alloc] initWithContentsOfURL:plist_url error:nil];
  NSArray* keys = plist.allKeys;

  // 1. Fill in variables.
  for (id key in keys) {
    NSString* value = plist[key];
    if (![value isKindOfClass:[NSString class]] || value.length < 2) {
      continue;
    }

    // Remove leading and trailing '@'s.
    NSString* variable =
        [value substringWithRange:NSMakeRange(1, value.length - 2)];

    NSString* substitution = replacement_dict[variable];
    if (substitution) {
      plist[key] = substitution;
    }
  }

  // 2. Fill in other values.
  plist[app_mode::kCrBundleVersionKey] =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  plist[app_mode::kCFBundleShortVersionStringKey] =
      base::SysUTF8ToNSString(info_->version_for_display);
  if (IsMultiProfile()) {
    plist[base::apple::CFToNSPtrCast(kCFBundleIdentifierKey)] =
        base::SysUTF8ToNSString(GetBundleIdentifierForShim(info_->app_id));
    base::FilePath data_dir = GetMultiProfileAppDataDir(app_data_dir_);
    plist[app_mode::kCrAppModeUserDataDirKey] =
        base::apple::FilePathToNSString(data_dir);
  } else {
    plist[base::apple::CFToNSPtrCast(kCFBundleIdentifierKey)] =
        base::SysUTF8ToNSString(
            GetBundleIdentifierForShim(info_->app_id, info_->profile_path));
    plist[app_mode::kCrAppModeUserDataDirKey] =
        base::apple::FilePathToNSString(app_data_dir_);
    plist[app_mode::kCrAppModeProfileDirKey] =
        base::apple::FilePathToNSString(info_->profile_path.BaseName());
    plist[app_mode::kCrAppModeProfileNameKey] =
        base::SysUTF8ToNSString(info_->profile_name);
  }
  plist[app_mode::kLSHasLocalizedDisplayNameKey] = @YES;
  plist[app_mode::kNSHighResolutionCapableKey] = @YES;
  plist[app_mode::kCrAppModeIsAdHocSignedKey] =
      @(UseAdHocSigningForWebAppShims());

  // 3. Fill in file handlers.
  // The plist needs to contain file handlers for all profiles the app is
  // installed in. `info_->file_handler_extensions` only contains information
  // for the current profile, so combine that with the information from
  // `info_->handlers_per_profile`.
  auto file_handler_extensions =
      GetFileHandlerExtensionsWithoutDot(info_->file_handler_extensions);
  auto file_handler_mime_types = info_->file_handler_mime_types;
  for (const auto& profile_handlers : info_->handlers_per_profile) {
    if (profile_handlers.first == info_->profile_path) {
      continue;
    }
    auto extensions = GetFileHandlerExtensionsWithoutDot(
        profile_handlers.second.file_handler_extensions);
    file_handler_extensions.insert(extensions.begin(), extensions.end());
    file_handler_mime_types.insert(
        profile_handlers.second.file_handler_mime_types.begin(),
        profile_handlers.second.file_handler_mime_types.end());
  }
  if (!file_handler_extensions.empty() || !file_handler_mime_types.empty()) {
    NSMutableArray* doc_types_value = [NSMutableArray array];
    NSMutableDictionary* doc_types_dict = [NSMutableDictionary dictionary];
    if (!file_handler_extensions.empty()) {
      NSMutableArray* extensions = [NSMutableArray array];
      for (const auto& file_extension : file_handler_extensions) {
        [extensions addObject:base::SysUTF8ToNSString(file_extension)];
      }
      doc_types_dict[app_mode::kCFBundleTypeExtensionsKey] = extensions;
    }
    if (!file_handler_mime_types.empty()) {
      NSMutableArray* mime_types = [NSMutableArray array];
      for (const auto& mime_type : file_handler_mime_types) {
        [mime_types addObject:base::SysUTF8ToNSString(mime_type)];
      }
      doc_types_dict[app_mode::kCFBundleTypeMIMETypesKey] = mime_types;
    }
    [doc_types_value addObject:doc_types_dict];
    plist[app_mode::kCFBundleDocumentTypesKey] = doc_types_value;
  }

  // 4. Fill in protocol handlers
  // Similarly to file handlers above, here too we need to combine handlers
  // for the current profile with those for other profiles the app is installed
  // in.
  auto protocol_handlers = info_->protocol_handlers;
  for (const auto& profile_handlers : info_->handlers_per_profile) {
    if (profile_handlers.first == info_->profile_path) {
      continue;
    }
    protocol_handlers.insert(profile_handlers.second.protocol_handlers.begin(),
                             profile_handlers.second.protocol_handlers.end());
  }

  if (!protocol_handlers.empty()) {
    scoped_refptr<OsIntegrationTestOverride> os_override =
        OsIntegrationTestOverride::Get();
    if (os_override) {
      CHECK_IS_TEST();
      std::vector<std::string> protocol_handlers_vec;
      protocol_handlers_vec.insert(protocol_handlers_vec.end(),
                                   protocol_handlers.begin(),
                                   protocol_handlers.end());
      os_override->RegisterProtocolSchemes(info_->app_id,
                                           std::move(protocol_handlers_vec));
    }

    NSMutableArray* handlers = [NSMutableArray array];
    for (const auto& protocol_handler : protocol_handlers) {
      [handlers addObject:base::SysUTF8ToNSString(protocol_handler)];
    }

    plist[app_mode::kCFBundleURLTypesKey] = @[ @{
      app_mode::kCFBundleURLNameKey :
          base::SysUTF8ToNSString(GetBundleIdentifierForShim(info_->app_id)),
      app_mode::kCFBundleURLSchemesKey : handlers
    } ];
  }

  // TODO(crbug.com/40807015): If we decide to rename app bundles on app title
  // changes, instead of relying on localization, then this will need to change
  // to use GetShortcutBaseName, most likely only for non-legacy-apps
  // (in other words, revert to what the code looked like before on these
  // lines). See also crbug.com/1021804.
  base::FilePath app_name = app_path.BaseName().RemoveFinalExtension();
  plist[base::apple::CFToNSPtrCast(kCFBundleNameKey)] =
      base::apple::FilePathToNSString(app_name);

  return [plist writeToURL:plist_url error:nil];
}

bool WebAppShortcutCreator::UpdateIcon(const base::FilePath& app_path) const {
  if (info_->favicon.empty() && info_->favicon_maskable.empty()) {
    return true;
  }

  IcnsEncoder icns_encoder;
  bool has_valid_icons = false;
  if (!info_->favicon_maskable.empty() &&
      base::FeatureList::IsEnabled(kWebAppMaskableIconsOnMac)) {
    for (gfx::ImageFamily::const_iterator it = info_->favicon_maskable.begin();
         it != info_->favicon_maskable.end(); ++it) {
      if (icns_encoder.AddImage(CreateAppleMaskedAppIcon(*it))) {
        has_valid_icons = true;
      }
    }
  }

  if (!has_valid_icons) {
    for (gfx::ImageFamily::const_iterator it = info_->favicon.begin();
         it != info_->favicon.end(); ++it) {
      if (icns_encoder.AddImage(*it)) {
        has_valid_icons = true;
      }
    }
  }

  if (!has_valid_icons) {
    return false;
  }

  base::FilePath resources_path = GetResourcesPath(app_path);
  if (!base::CreateDirectory(resources_path)) {
    return false;
  }

  return icns_encoder.WriteToFile(resources_path.Append("app.icns"));
}

bool WebAppShortcutCreator::UpdateSignature(
    const base::FilePath& app_path) const {
  if (!UseAdHocSigningForWebAppShims()) {
    return true;
  }

  base::apple::ScopedCFTypeRef<CFURLRef> app_url =
      base::apple::FilePathToCFURL(app_path);
  base::apple::ScopedCFTypeRef<SecStaticCodeRef> app_code;
  if (SecStaticCodeCreateWithPath(app_url.get(), kSecCSDefaultFlags,
                                  app_code.InitializeInto()) != errSecSuccess) {
    return false;
  }

  // Use the most restrictive flags possible. Library validation cannot be
  // enabled as an adhoc binary's signing identity inherently does not match the
  // signing identity of the non-system libraries that the app shim loads.
  uint32_t code_signer_flags = kSecCodeSignatureRestrict |
                               kSecCodeSignatureForceKill |
                               kSecCodeSignatureRuntime;

  auto* signer_params = @{
    static_cast<id>(kSecCodeSignerFlags) : @(code_signer_flags),
    static_cast<id>(kSecCodeSignerIdentity) : [NSNull null],
    static_cast<id>(kSecCodeSignerEntitlements) : AppShimEntitlements(),
  };
  base::apple::ScopedCFTypeRef<SecCodeSignerRef> signer;
  if (SecCodeSignerCreate(base::apple::NSToCFPtrCast(signer_params),
                          kSecCSDefaultFlags,
                          signer.InitializeInto()) != errSecSuccess) {
    return false;
  }

  base::apple::ScopedCFTypeRef<CFErrorRef> errors;
  if (SecCodeSignerAddSignatureWithErrors(
          signer.get(), app_code.get(), kSecCSDefaultFlags,
          errors.InitializeInto()) != errSecSuccess) {
    LOG(ERROR) << "Failed to sign web app shim: " << errors.get();
    return false;
  }

  base::apple::ScopedCFTypeRef<CFDictionaryRef> app_shim_info;
  if (SecCodeCopySigningInformation(app_code.get(), kSecCSSigningInformation,
                                    app_shim_info.InitializeInto()) !=
      errSecSuccess) {
    LOG(ERROR) << "Failed to copy signing information from web app shim";
    return false;
  }

  CFDataRef cd_hash_data = base::apple::GetValueFromDictionary<CFDataRef>(
      app_shim_info.get(), kSecCodeInfoUnique);
  auto cd_hash_span = base::apple::CFDataToSpan(cd_hash_data);
  std::vector<uint8_t> cd_hash(cd_hash_span.begin(), cd_hash_span.end());

  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AppShimRegistry::SaveCdHashForApp,
                                base::Unretained(AppShimRegistry::Get()),
                                info_->app_id, std::move(cd_hash)));

  return true;
}

// Return true if ad-hoc signing should be used for web app shims.
bool WebAppShortcutCreator::UseAdHocSigningForWebAppShims() const {
  return use_ad_hoc_signing_for_web_app_shims_;
}

}  // namespace web_app
