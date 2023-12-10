// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/os_integration/web_app_shortcut_mac.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#import "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#include "cc/paint/paint_flags.h"
#import "chrome/browser/mac/dock.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/os_integration/icns_encoder.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/crx_file/id_util.h"
#include "components/variations/net/variations_command_line.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/features.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_family.h"

#if defined(COMPONENT_BUILD)
#include <mach-o/loader.h>

#include "base/bits.h"
#include "base/process/launch.h"
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

// A TerminationObserver observes a NSRunningApplication for when it
// terminates. On termination, it will run the specified callback on the UI
// thread and release itself.
@interface TerminationObserver : NSObject

+ (void)startObservingForRunningApplication:(NSRunningApplication*)app
                               withCallback:(base::OnceClosure)callback;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation TerminationObserver {
  NSRunningApplication* __strong _app;
  base::OnceClosure _callback;
}

+ (NSMutableSet<TerminationObserver*>*)allObservers {
  static NSMutableSet<TerminationObserver*>* set = [NSMutableSet set];
  return set;
}

+ (void)startObservingForRunningApplication:(NSRunningApplication*)app
                               withCallback:(base::OnceClosure)callback {
  TerminationObserver* observer = [[TerminationObserver alloc]
      initWithRunningApplication:app
                        callback:std::move(callback)];

  if (observer) {
    [[TerminationObserver allObservers] addObject:observer];
  }
}

- (instancetype)initWithRunningApplication:(NSRunningApplication*)app
                                  callback:(base::OnceClosure)callback {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (self = [super init]) {
    _callback = std::move(callback);
    _app = app;
    // Note that |observeValueForKeyPath| will be called with the initial value
    // within the |addObserver| call.
    [_app addObserver:self
           forKeyPath:@"isTerminated"
              options:NSKeyValueObservingOptionNew |
                      NSKeyValueObservingOptionInitial
              context:nullptr];
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSNumber* newNumberValue = change[NSKeyValueChangeNewKey];
  BOOL newValue = newNumberValue.boolValue;
  if (newValue) {
    // Note that a block is posted, which will hold a retain on `self`.
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                                   [self onTerminated];
                                                 }));
  }
}

- (void)onTerminated {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If |onTerminated| is called repeatedly (which in theory it should not),
  // then ensure that we only call removeObserver and release once by doing an
  // early-out if |callback_| has already been made.
  if (!_callback) {
    return;
  }
  std::move(_callback).Run();
  DCHECK(!_callback);

  [_app removeObserver:self forKeyPath:@"isTerminated" context:nullptr];

  [[TerminationObserver allObservers] performSelector:@selector(removeObject:)
                                           withObject:self
                                           afterDelay:0];
}
@end

// TODO(https://crbug.com/941909): Change all launch functions to take a single
// callback that returns a NSRunningApplication, rather than separate launch and
// termination callbacks.
void RunAppLaunchCallbacks(
    NSRunningApplication* app,
    base::OnceCallback<void(base::Process)> launch_callback,
    base::OnceClosure termination_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(app);

  // If the app doesn't have a valid pid, or if the application has been
  // terminated, then indicate failure in |launch_callback|.
  base::Process process(app.processIdentifier);
  if (!process.IsValid() || app.terminated) {
    LOG(ERROR) << "Application has already been terminated.";
    std::move(launch_callback).Run(base::Process());
    return;
  }

  // Otherwise, indicate successful launch, and watch for termination.
  // TODO(https://crbug.com/941909): This watches for termination indefinitely,
  // but we only need to watch for termination until the app establishes a
  // (whereupon termination will be noticed by the mojo connection closing).
  std::move(launch_callback).Run(std::move(process));
  [TerminationObserver
      startObservingForRunningApplication:app
                             withCallback:std::move(termination_callback)];
}

namespace web_app {

BASE_FEATURE(kWebAppMaskableIconsOnMac,
             "WebAppMaskableIconsOnMac",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

WebAppAutoLoginUtil* g_auto_login_util_for_testing = nullptr;

// UMA metric name for creating shortcut result.
constexpr const char* kCreateShortcutResult = "Apps.CreateShortcuts.Mac.Result";

// Result of creating app shortcut.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreateShortcutResult {
  kSuccess = 0,
  kApplicationDirNotFound = 1,
  kFailToLocalizeApplication = 2,
  kFailToGetApplicationPaths = 3,
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
  UMA_HISTOGRAM_ENUMERATION(kCreateShortcutResult, result);
}

// The maximum number to append to to an app name before giving up and using the
// extension id.
constexpr int kMaxConflictNumber = 999;

// Remove the leading . from the entries of |extensions|. Any items that do not
// have a leading . are removed.
std::set<std::string> GetFileHandlerExtensionsWithoutDot(
    const std::set<std::string>& file_extensions) {
  std::set<std::string> result;
  for (const auto& file_extension : file_extensions) {
    if (file_extension.length() <= 1 || file_extension[0] != '.')
      continue;
    result.insert(file_extension.substr(1));
  }
  return result;
}

bool AppShimRevealDisabledForTest() {
  // Disable app shim reveal in the Finder during tests, to avoid
  // creating Finder windows that are never closed.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kTestType) ||
         OsIntegrationTestOverride::Get();
}

base::FilePath GetWritableApplicationsDirectory() {
  base::FilePath path;
  if (base::apple::GetUserDirectory(NSApplicationDirectory, &path)) {
    if (!base::DirectoryExists(path)) {
      if (!base::CreateDirectory(path))
        return base::FilePath();

      // Create a zero-byte ".localized" file to inherit localizations from
      // macOS for folders that have special meaning.
      base::WriteFile(path.Append(".localized"), "");
    }
    return base::PathIsWritable(path) ? path : base::FilePath();
  }
  return base::FilePath();
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

// Data and helpers for an Info.plist under a given bundle path.
class BundleInfoPlist {
 public:
  // Retrieve info from all app shims found in |apps_path|.
  static std::list<BundleInfoPlist> GetAllInPath(
      const base::FilePath& apps_path,
      bool recursive) {
    std::list<BundleInfoPlist> bundles;
    base::FileEnumerator enumerator(apps_path, recursive,
                                    base::FileEnumerator::DIRECTORIES);
    for (base::FilePath shim_path = enumerator.Next(); !shim_path.empty();
         shim_path = enumerator.Next()) {
      bundles.emplace_back(shim_path);
    }
    return bundles;
  }

  // Retrieve info from the specified app shim in |bundle_path|.
  explicit BundleInfoPlist(const base::FilePath& bundle_path)
      : bundle_path_(bundle_path) {
    plist_ = [NSDictionary dictionaryWithContentsOfURL:GetPlistURL(bundle_path_)
                                                 error:nil];
  }
  BundleInfoPlist(const BundleInfoPlist& other) = default;
  BundleInfoPlist& operator=(const BundleInfoPlist& other) = default;
  ~BundleInfoPlist() = default;

  const base::FilePath& bundle_path() const { return bundle_path_; }

  // Checks that the CrAppModeUserDataDir in the Info.plist is, or is a subpath
  // of the current user_data_dir. This uses starts with instead of equals
  // because the CrAppModeUserDataDir could be the user_data_dir or the
  // |app_data_dir_|.
  bool IsForCurrentUserDataDir() const {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    DCHECK(!user_data_dir.empty());
    return base::StartsWith(
        base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeUserDataDirKey]),
        user_data_dir.value(), base::CompareCase::SENSITIVE);
  }

  // Checks if kCrAppModeProfileDirKey corresponds to the specified profile
  // path.
  bool IsForProfile(const base::FilePath& test_profile_path) const {
    std::string profile_path =
        base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeProfileDirKey]);
    return profile_path == test_profile_path.BaseName().value();
  }

  // Return the full profile path (as a subpath of the user_data_dir).
  base::FilePath GetFullProfilePath() const {
    // Figure out the profile_path. Since the user_data_dir could contain the
    // path to the web app data dir.
    base::FilePath user_data_dir = base::apple::NSStringToFilePath(
        plist_[app_mode::kCrAppModeUserDataDirKey]);
    base::FilePath profile_base_name = base::apple::NSStringToFilePath(
        plist_[app_mode::kCrAppModeProfileDirKey]);
    if (user_data_dir.DirName().DirName().BaseName() == profile_base_name)
      return user_data_dir.DirName().DirName();
    return user_data_dir.Append(profile_base_name);
  }

  std::string GetExtensionId() const {
    return base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeShortcutIDKey]);
  }
  std::string GetProfileName() const {
    return base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeProfileNameKey]);
  }
  GURL GetURL() const {
    return GURL(
        base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeShortcutURLKey]));
  }
  std::u16string GetTitle() const {
    return base::SysNSStringToUTF16(
        plist_[app_mode::kCrAppModeShortcutNameKey]);
  }
  base::Version GetVersion() const {
    NSString* version_string = plist_[app_mode::kCrBundleVersionKey];
    if (!version_string) {
      // Older bundles have the Chrome version in the following key.
      version_string = plist_[app_mode::kCFBundleShortVersionStringKey];
    }
    return base::Version(base::SysNSStringToUTF8(version_string));
  }
  std::string GetBundleId() const {
    return base::SysNSStringToUTF8(
        plist_[base::apple::CFToNSPtrCast(kCFBundleIdentifierKey)]);
  }

 private:
  // The path of the app bundle from this this info was read.
  base::FilePath bundle_path_;

  // Data read from the Info.plist.
  NSDictionary* __strong plist_;
};

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

base::CommandLine BuildCommandLineForShimLaunch() {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(
      app_mode::kLaunchedByChromeProcessId,
      base::NumberToString(base::GetCurrentProcId()));
  command_line.AppendSwitchPath(app_mode::kLaunchedByChromeBundlePath,
                                base::apple::MainBundlePath());

  // When running unbundled (e.g, when running browser_tests), the path
  // returned by base::apple::FrameworkBundlePath will not include the version.
  // Manually append it.
  // https://crbug.com/1286681
  const base::FilePath framework_bundle_path =
      base::apple::AmIBundled() ? base::apple::FrameworkBundlePath()
                                : base::apple::FrameworkBundlePath()
                                      .Append("Versions")
                                      .Append(version_info::GetVersionNumber());
  command_line.AppendSwitchPath(app_mode::kLaunchedByChromeFrameworkBundlePath,
                                framework_bundle_path);
  command_line.AppendSwitchPath(
      app_mode::kLaunchedByChromeFrameworkDylibPath,
      framework_bundle_path.Append(chrome::kFrameworkExecutableName));

  return command_line;
}

NSRunningApplication* FindRunningApplicationForBundleIdAndPath(
    const std::string& bundle_id,
    const base::FilePath& bundle_path) {
  NSArray<NSRunningApplication*>* apps = [NSRunningApplication
      runningApplicationsWithBundleIdentifier:base::SysUTF8ToNSString(
                                                  bundle_id)];
  for (NSRunningApplication* app in apps) {
    if (base::apple::NSURLToFilePath(app.bundleURL) == bundle_path) {
      return app;
    }
  }

  // Sometimes runningApplicationsWithBundleIdentifier incorrectly fails to
  // return all apps with the provided bundle id. So also scan over the full
  // list of running applications.
  apps = NSWorkspace.sharedWorkspace.runningApplications;
  for (NSRunningApplication* app in apps) {
    if (base::SysNSStringToUTF8(app.bundleIdentifier) == bundle_id &&
        base::apple::NSURLToFilePath(app.bundleURL) == bundle_path) {
      return app;
    }
  }

  return nil;
}

// Wrapper around base::mac::LaunchApplication that attempts to retry the launch
// once, if the initial launch fails. This helps reduce test flakiness on older
// Mac OS bots (Mac 11).
void LaunchApplicationWithRetry(const base::FilePath& app_bundle_path,
                                const base::CommandLine& command_line,
                                const std::vector<std::string>& url_specs,
                                base::mac::LaunchApplicationOptions options,
                                base::mac::LaunchApplicationCallback callback) {
  base::mac::LaunchApplication(
      app_bundle_path, command_line, url_specs, options,
      base::BindOnce(
          [](const base::FilePath& app_bundle_path,
             const base::CommandLine& command_line,
             const std::vector<std::string>& url_specs,
             base::mac::LaunchApplicationOptions options,
             base::mac::LaunchApplicationCallback callback,
             NSRunningApplication* app, NSError* error) {
            if (app) {
              std::move(callback).Run(app, nil);
              return;
            }

            if (@available(macOS 12.0, *)) {
              // In newer Mac OS versions this workaround isn't needed, and in
              // fact can itself cause flaky tests by launching the app twice
              // when only one launch is expected.
              std::move(callback).Run(app, error);
              return;
            }

            // Only retry for the one specific error code that seems to need
            // this. Like above, retrying in all cases can otherwise itself
            // cause flaky tests.
            if (error.domain == NSCocoaErrorDomain &&
                error.code == NSFileReadCorruptFileError) {
              LOG(ERROR) << "Failed to open application with path: "
                         << app_bundle_path << ", retrying in 100ms";
              internals::GetShortcutIOTaskRunner()->PostDelayedTask(
                  FROM_HERE,
                  base::BindOnce(&base::mac::LaunchApplication, app_bundle_path,
                                 command_line, url_specs, options,
                                 std::move(callback)),
                  base::Milliseconds(100));
              return;
            }
            std::move(callback).Run(nil, error);
          },
          app_bundle_path, command_line, url_specs, options,
          std::move(callback)));
}

void LaunchTheFirstShimThatWorksOnFileThread(
    std::vector<base::FilePath> shim_paths,
    bool launched_after_rebuild,
    ShimLaunchMode launch_mode,
    const std::string& bundle_id,
    ShimLaunchedCallback launched_callback,
    ShimTerminatedCallback terminated_callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Avoid trying to launch known non-existent paths. This loop might
  // (technically) be O(n^2) but there will be too few paths for this to matter.
  while (!shim_paths.empty() && !base::PathExists(shim_paths.front())) {
    shim_paths.erase(shim_paths.begin());
  }
  if (shim_paths.empty()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(launched_callback), base::Process()));
    return;
  }

  base::FilePath shim_path = shim_paths.front();
  shim_paths.erase(shim_paths.begin());

  base::CommandLine command_line = BuildCommandLineForShimLaunch();

  if (launched_after_rebuild) {
    command_line.AppendSwitch(app_mode::kLaunchedAfterRebuild);
  }

  // The shim must have the same feature parameter and field trial state as
  // Chrome, so pass this over the command line. This is not done as part of
  // `BuildcommandLineForShimLaunch`, as the other caller of that method is
  // to simulate a launch by the OS, which would not have these arguments.
  variations::VariationsCommandLine::GetForCurrentProcess().ApplyToCommandLine(
      command_line);

  LaunchApplicationWithRetry(
      shim_path, command_line, /*url_specs=*/{},
      {.activate = false,
       .hidden_in_background = launch_mode == ShimLaunchMode::kBackground},
      base::BindOnce(
          [](base::FilePath shim_path,
             std::vector<base::FilePath> remaining_shim_paths,
             bool launched_after_rebuild, ShimLaunchMode launch_mode,
             const std::string& bundle_id,
             ShimLaunchedCallback launched_callback,
             ShimTerminatedCallback terminated_callback,
             NSRunningApplication* app, NSError* error) {
            if (app) {
              RunAppLaunchCallbacks(app, std::move(launched_callback),
                                    std::move(terminated_callback));
              return;
            }

            LOG(ERROR) << "Failed to open application with path: " << shim_path;

            internals::GetShortcutIOTaskRunner()->PostTask(
                FROM_HERE,
                base::BindOnce(&LaunchTheFirstShimThatWorksOnFileThread,
                               remaining_shim_paths, launched_after_rebuild,
                               launch_mode, bundle_id,
                               std::move(launched_callback),
                               std::move(terminated_callback)));
          },
          shim_path, shim_paths, launched_after_rebuild, launch_mode, bundle_id,
          std::move(launched_callback), std::move(terminated_callback)));
}

void LaunchShimOnFileThread(LaunchShimUpdateBehavior update_behavior,
                            ShimLaunchMode launch_mode,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback,
                            const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  WebAppShortcutCreator shortcut_creator(
      internals::GetShortcutDataDir(shortcut_info), &shortcut_info);

  // Recreate shims if requested, and populate |shim_paths| with the paths to
  // attempt to launch.
  bool launched_after_rebuild = false;
  std::vector<base::FilePath> shim_paths;
  bool shortcuts_updated = true;
  switch (update_behavior) {
    case LaunchShimUpdateBehavior::kDoNotRecreate:
      // Attempt to locate the shim's path using LaunchServices.
      shim_paths = shortcut_creator.GetAppBundlesById();
      break;
    case LaunchShimUpdateBehavior::kRecreateIfInstalled:
      // Only attempt to launch shims that were updated.
      launched_after_rebuild = true;
      shortcuts_updated = shortcut_creator.UpdateShortcuts(
          /*create_if_needed=*/false, &shim_paths);
      break;
    case LaunchShimUpdateBehavior::kRecreateUnconditionally:
      // Likewise, only attempt to launch shims that were updated.
      launched_after_rebuild = true;
      shortcuts_updated = shortcut_creator.UpdateShortcuts(
          /*create_if_needed=*/true, &shim_paths);
      break;
  }
  LOG_IF(ERROR, !shortcuts_updated) << "Could not write shortcut for app shim.";

  LaunchTheFirstShimThatWorksOnFileThread(
      shim_paths, launched_after_rebuild, launch_mode,
      shortcut_creator.GetAppBundleId(), std::move(launched_callback),
      std::move(terminated_callback));
}

base::FilePath GetLocalizableAppShortcutsSubdirName() {
  static const char kChromiumAppDirName[] = "Chromium Apps.localized";
  static const char kChromeAppDirName[] = "Chrome Apps.localized";
  static const char kChromeCanaryAppDirName[] = "Chrome Canary Apps.localized";

  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return base::FilePath(kChromiumAppDirName);

    case version_info::Channel::CANARY:
      return base::FilePath(kChromeCanaryAppDirName);

    default:
      return base::FilePath(kChromeAppDirName);
  }
}

// Creates a canvas the same size as |overlay|, copies the appropriate
// representation from |background| into it (according to Cocoa), then draws
// |overlay| over it using NSCompositingOperationSourceOver.
NSImageRep* OverlayImageRep(NSImage* background, NSImageRep* overlay) {
  DCHECK(background);
  NSInteger dimension = overlay.pixelsWide;
  DCHECK_EQ(dimension, overlay.pixelsHigh);
  NSBitmapImageRep* canvas = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nullptr
                    pixelsWide:dimension
                    pixelsHigh:dimension
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];

  // There isn't a colorspace name constant for sRGB, so retag.
  canvas = [canvas
      bitmapImageRepByRetaggingWithColorSpace:NSColorSpace.sRGBColorSpace];

  // Communicate the DIP scale (1.0). TODO(tapted): Investigate HiDPI.
  canvas.size = NSMakeSize(dimension, dimension);

  NSGraphicsContext* drawing_context =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:canvas];
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext.currentContext = drawing_context;
  [background drawInRect:NSMakeRect(0, 0, dimension, dimension)
                fromRect:NSZeroRect
               operation:NSCompositingOperationCopy
                fraction:1.0];
  [overlay drawInRect:NSMakeRect(0, 0, dimension, dimension)
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:1.0
       respectFlipped:NO
                hints:nil];
  [NSGraphicsContext restoreGraphicsState];
  return canvas;
}

// Helper function to extract the single NSImageRep held in a resource bundle
// image.
NSImageRep* ImageRepForGFXImage(const gfx::Image& image) {
  NSArray* image_reps = image.AsNSImage().representations;
  DCHECK_EQ(1u, image_reps.count);
  return image_reps[0];
}

using ResourceIDToImage = std::map<int, NSImageRep*>;

// Generates a map of NSImageReps used by SetWorkspaceIconOnFILEThread and
// passes it to |io_task|. Since ui::ResourceBundle can only be used on UI
// thread, this function also needs to run on UI thread, and the gfx::Images
// need to be converted to NSImageReps on the UI thread due to non-thread-safety
// of gfx::Image.
void GetImageResourcesOnUIThread(
    base::OnceCallback<void(std::unique_ptr<ResourceIDToImage>)> io_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<ResourceIDToImage> result =
      std::make_unique<ResourceIDToImage>();

  // These resource ID should match to the ones used by
  // SetWorkspaceIconOnFILEThread below.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32,
                 IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    gfx::Image image = resource_bundle.GetNativeImageNamed(id);
    (*result)[id] = ImageRepForGFXImage(image);
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(std::move(io_task), std::move(result)));
}

void SetWorkspaceIconOnWorkerThread(const base::FilePath& apps_directory,
                                    std::unique_ptr<ResourceIDToImage> images) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSImage* folder_icon_image = [[NSImage alloc] init];
  // Use complete assets for the small icon sizes. -[NSWorkspace setIcon:] has a
  // bug when dealing with named NSImages where it incorrectly handles alpha
  // premultiplication. This is most noticeable with small assets since the 1px
  // border is a much larger component of the small icons.
  // See http://crbug.com/305373 for details.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32}) {
    const auto& found = images->find(id);
    DCHECK(found != images->end());
    [folder_icon_image addRepresentation:found->second];
  }

  // Brand larger folder assets with an embossed app launcher logo to
  // conserve distro size and for better consistency with changing hue
  // across macOS versions. The folder is textured, so compresses poorly
  // without this.
  NSImage* base_image = [NSImage imageNamed:NSImageNameFolder];
  for (int id : {IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    const auto& found = images->find(id);
    DCHECK(found != images->end());
    NSImageRep* with_overlay = OverlayImageRep(base_image, found->second);
    DCHECK(with_overlay);
    if (with_overlay)
      [folder_icon_image addRepresentation:with_overlay];
  }
  [NSWorkspace.sharedWorkspace
      setIcon:folder_icon_image
      forFile:base::apple::FilePathToNSString(apps_directory)
      options:0];
}

// Adds a localized strings file for the Chrome Apps directory using the current
// locale. macOS will use this for the display name.
// + Chrome Apps.localized (|apps_directory|)
// | + .localized
// | | en.strings
// | | de.strings
bool UpdateAppShortcutsSubdirLocalizedName(
    const base::FilePath& apps_directory) {
  base::FilePath localized = apps_directory.Append(".localized");
  if (!base::CreateDirectory(localized))
    return false;

  base::FilePath directory_name = apps_directory.BaseName().RemoveExtension();
  std::u16string localized_name =
      shell_integration::GetAppShortcutsSubdirName();
  NSDictionary* strings_dict = @{
    base::apple::FilePathToNSString(directory_name) :
        base::SysUTF16ToNSString(localized_name)
  };

  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(std::string()));

  NSString* strings_path =
      base::apple::FilePathToNSString(localized.Append(locale + ".strings"));
  [strings_dict writeToFile:strings_path atomically:YES];

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetImageResourcesOnUIThread,
                                base::BindOnce(&SetWorkspaceIconOnWorkerThread,
                                               apps_directory)));
  return true;
}

base::FilePath GetMultiProfileAppDataDir(base::FilePath app_data_dir) {
  // The kCrAppModeUserDataDirKey is expected to be a path in kWebAppDirname,
  // and the true user data dir is extracted by going three directories up.
  // For profile-agnostic apps, remove this reference to the profile name.
  // TODO(https://crbug.com/1021237): Do not specify kCrAppModeUserDataDirKey
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

// Return all bundles with the specified |bundle_id| which are for the current
// user data dir.
std::list<BundleInfoPlist> SearchForBundlesById(const std::string& bundle_id) {
  std::list<BundleInfoPlist> infos;

  // First search using LaunchServices.
  NSArray* bundle_urls =
      base::apple::CFToNSOwnershipCast(LSCopyApplicationURLsForBundleIdentifier(
          base::SysUTF8ToCFStringRef(bundle_id).get(), /*outError=*/nullptr));
  for (NSURL* url : bundle_urls) {
    base::FilePath bundle_path = base::apple::NSURLToFilePath(url);
    BundleInfoPlist info(bundle_path);
    if (!info.IsForCurrentUserDataDir())
      continue;
    infos.push_back(info);
  }
  if (!infos.empty()) {
    return infos;
  }

  // LaunchServices can fail to locate a recently-created bundle. Search
  // for an app in the applications folder to handle this case.
  // https://crbug.com/937703
  infos = BundleInfoPlist::GetAllInPath(GetChromeAppsFolder(),
                                        /*recursive=*/true);
  for (auto it = infos.begin(); it != infos.end();) {
    const BundleInfoPlist& info = *it;
    if (info.GetBundleId() == bundle_id && info.IsForCurrentUserDataDir()) {
      ++it;
    } else {
      infos.erase(it++);
    }
  }
  return infos;
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
          base::as_writable_bytes(base::make_span(&header, 1u))) ||
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
      if (!executable_file.WriteAndCheck(
              0, base::as_bytes(base::make_span(&header, 1u))) ||
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

// Creates a masked icon image from a base icon image.
gfx::Image MaskedIcon(const gfx::Image& base_icon) {
  // According to Apple design templates, a macOS icon should be a rounded
  // rect surrounded by some transparent padding.  The rounded rect's size
  // is approximately 80% of the overall icon, but this percentage varies.
  // Exact mask size and shape gleaned from Apple icon design templates,
  // specifically the March 2023 macOS Production Templates Sketch file at
  // https://developer.apple.com/design/resources/.  A few corner radius
  // values were unavailable in the file because the relevant shapes were
  // represenated as plain paths rather than rounded rects.
  //
  // The Web App Manifest spec defines a safe zone for maskable icons
  // (https://www.w3.org/TR/appmanifest/#icon-masks) in a centered circle
  // with diameter 80% of the overall icon.  Since the macOS icon mask
  // is a rounded rect that is never smaller than 80% of the overall icon,
  // it is within spec to simply draw our base icon full size and clip
  // whatever is outside of the rounded rect.  This is what is currently
  // implemented, even though is is different from what Apple does in macOS
  // Sonoma web apps (where instead they first scale the icon to cover just
  // the rounded rect, only clipping the corners).  Somewhere in the middle
  // of these options might be ideal, although with the current icon loading
  // code icons have already been resized to neatly fill entire standard sized
  // icons by the time this code runs, so doing any more resizing here would
  // not be great.
  int base_size = base_icon.Width();
  SkScalar icon_grid_bounding_box_inset;
  SkScalar icon_grid_bounding_box_corner_radius;
  SkScalar shadow_y_offset;
  SkScalar shadow_blur_radius;
  switch (base_size) {
    case 16:
      // An exact value for the 16 corner radius was not available.
      // this corner radius is derived by dividing the 32 radius by 2
      icon_grid_bounding_box_inset = 1.0;
      icon_grid_bounding_box_corner_radius = 2.785;
      shadow_y_offset = 0.5;
      shadow_blur_radius = 0.5;
      break;
    case 32:
      icon_grid_bounding_box_inset = 2.0;
      icon_grid_bounding_box_corner_radius = 5.75;
      shadow_y_offset = 1.0;
      shadow_blur_radius = 1.0;
      break;
    case 64:
      icon_grid_bounding_box_inset = 6.0;
      icon_grid_bounding_box_corner_radius = 11.5;
      shadow_y_offset = 2;
      shadow_blur_radius = 2;
      break;
    case 128:
      // An exact value for the 128 corner radius was not available.
      // this corner radius is derived by dividing the 256 radius by 2
      // or by multiplying the 64 radius by 2, both calculations
      // have the same result.
      icon_grid_bounding_box_inset = 12.0;
      icon_grid_bounding_box_corner_radius = 23.0;
      shadow_y_offset = 1.25;
      shadow_blur_radius = 1.25;
      break;
    case 256:
      icon_grid_bounding_box_inset = 25.0;
      icon_grid_bounding_box_corner_radius = 46.0;
      shadow_y_offset = 2.5;
      shadow_blur_radius = 2.5;
      break;
    case 512:
      icon_grid_bounding_box_inset = 50.0;
      icon_grid_bounding_box_corner_radius = 92.0;
      shadow_y_offset = 5.0;
      shadow_blur_radius = 5.0;
      break;
    case 1024:
      // An exact value for the 1024 corner radius was not available.
      // this corner radius is derived by multiplying the 512 radius by 2
      icon_grid_bounding_box_inset = 100.0;
      icon_grid_bounding_box_corner_radius = 184.0;
      shadow_y_offset = 10.0;
      shadow_blur_radius = 10.0;
      break;
    default:
      // Use 1024 sizes as a reference for approximating any size mask if needed
      icon_grid_bounding_box_inset = base_size * 100.0 / 1024.0;
      icon_grid_bounding_box_corner_radius = base_size * 184.0 / 1024.0;
      shadow_y_offset = base_size * 10.0 / 1024.0;
      shadow_blur_radius = base_size * 10.0 / 1024.0;
      break;
  }

  // Creat a bitmap and canvas for drawing the masked icon
  SkImageInfo info =
      SkImageInfo::Make(base_size, base_size, SkColorType::kN32_SkColorType,
                        SkAlphaType::kUnpremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  SkCanvas canvas(bitmap);
  SkRect base_rect = SkRect::MakeIWH(base_size, base_size);

  // Create a path for the icon mask. The mask will match Apple's icon grid
  // bounding box.
  SkPath icon_grid_bounding_box_path;
  SkRect icon_grid_bounding_box_rect = base_rect.makeInset(
      icon_grid_bounding_box_inset, icon_grid_bounding_box_inset);
  icon_grid_bounding_box_path.addRoundRect(
      icon_grid_bounding_box_rect, icon_grid_bounding_box_corner_radius,
      icon_grid_bounding_box_corner_radius);

  // Draw the shadow
  SkPaint shadowPaint;
  shadowPaint.setStyle(SkPaint::kFill_Style);
  shadowPaint.setARGB(77, 0, 0, 0);
  shadowPaint.setImageFilter(
      SkImageFilters::Blur(shadow_blur_radius, shadow_blur_radius, nullptr));
  canvas.save();
  canvas.translate(0, shadow_y_offset);
  canvas.drawPath(icon_grid_bounding_box_path, shadowPaint);
  canvas.restore();

  // Clip to the mask
  canvas.clipPath(icon_grid_bounding_box_path, /*doAntiAlias=*/true);

  // Draw the base icon on a white background
  // If the base icon is opaque, we shouldn't see any white. Unfortunately,
  // first filling the clip with white and then overlaying the base icon
  // results in white artifacts around the corners.  So, we'll use an unclipped
  // intermediate canvas to overlay the base icon on a full white background,
  // and then draw the intermediate canvas in the clip in one shot to avoid
  // white around the edges.
  SkBitmap opaque_bitmap;
  opaque_bitmap.allocPixels(info);
  SkCanvas opaque_canvas(opaque_bitmap);
  SkPaint backgroundPaint;
  backgroundPaint.setStyle(SkPaint::kFill_Style);
  backgroundPaint.setARGB(255, 255, 255, 255);
  opaque_canvas.drawRect(base_rect, backgroundPaint);
  opaque_canvas.drawImage(SkImages::RasterFromBitmap(base_icon.AsBitmap()), 0,
                          0);
  canvas.drawImage(SkImages::RasterFromBitmap(opaque_bitmap), 0, 0);

  // Create the final image.
  return gfx::Image::CreateFrom1xBitmap(bitmap);
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

bool AppShimCreationAndLaunchDisabledForTest() {
  // Note: The kTestType switch is only added on browser tests, but not unit
  // tests. Unit tests need to set the test override.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kTestType) &&
         !OsIntegrationTestOverride::Get();
}

base::FilePath GetChromeAppsFolder() {
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    if (os_override->IsChromeAppsValid()) {
      return os_override->chrome_apps_folder();
    }
    return base::FilePath();
  }

  base::FilePath path = GetWritableApplicationsDirectory();
  if (path.empty())
    return path;

  return path.Append(GetLocalizableAppShortcutsSubdirName());
}

// static
WebAppAutoLoginUtil* WebAppAutoLoginUtil::GetInstance() {
  if (g_auto_login_util_for_testing)
    return g_auto_login_util_for_testing;

  static base::NoDestructor<WebAppAutoLoginUtil> instance;
  return instance.get();
}

// static
void WebAppAutoLoginUtil::SetInstanceForTesting(
    WebAppAutoLoginUtil* auto_login_util) {
  g_auto_login_util_for_testing = auto_login_util;
}

void WebAppAutoLoginUtil::AddToLoginItems(const base::FilePath& app_bundle_path,
                                          bool hide_on_startup) {
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    os_override->EnableOrDisablePathOnLogin(app_bundle_path,
                                            /*enable_on_login=*/true);
  } else {
    base::mac::AddToLoginItems(app_bundle_path, hide_on_startup);
  }
}

void WebAppAutoLoginUtil::RemoveFromLoginItems(
    const base::FilePath& app_bundle_path) {
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    os_override->EnableOrDisablePathOnLogin(app_bundle_path,
                                            /*enable_on_login=*/false);
  } else {
    base::mac::RemoveFromLoginItems(app_bundle_path);
  }
}

WebAppShortcutCreator::WebAppShortcutCreator(const base::FilePath& app_data_dir,
                                             const ShortcutInfo* shortcut_info)
    : app_data_dir_(app_data_dir), info_(shortcut_info) {
  DCHECK(shortcut_info);
}

WebAppShortcutCreator::~WebAppShortcutCreator() = default;

base::FilePath WebAppShortcutCreator::GetApplicationsShortcutPath(
    bool avoid_conflicts) const {
  base::FilePath applications_dir = GetChromeAppsFolder();
  if (applications_dir.empty()) {
    return base::FilePath();
  }

  if (!avoid_conflicts) {
    return applications_dir.Append(GetShortcutBasename());
  }

  // Attempt to use the application's title for the file name. Resolve conflicts
  // by appending 1 through kMaxConflictNumber, before giving up and using the
  // concatenated profile and extension for a name name.
  for (int i = 1; i <= kMaxConflictNumber; ++i) {
    base::FilePath path = applications_dir.Append(GetShortcutBasename(i));
    if (base::DirectoryExists(path)) {
      continue;
    }
    return path;
  }

  // If all of those are taken, then use the combination of profile and
  // extension id.
  return applications_dir.Append(GetFallbackBasename());
}

base::FilePath WebAppShortcutCreator::GetShortcutBasename(
    int copy_number) const {
  // For profile-less shortcuts, use the fallback naming scheme to avoid change.
  if (info_->profile_name.empty()) {
    return GetFallbackBasename();
  }

  // Strip all preceding '.'s from the path.
  std::u16string title = info_->title;
  size_t first_non_dot = 0;
  while (first_non_dot < title.size() && title[first_non_dot] == '.')
    first_non_dot += 1;
  title = title.substr(first_non_dot);
  if (title.empty()) {
    return GetFallbackBasename();
  }

  // Finder will display ':' as '/', so replace all '/' instances with ':'.
  std::replace(title.begin(), title.end(), '/', ':');

  // Append the copy number.
  std::string title_utf8 = base::UTF16ToUTF8(title);
  if (copy_number != 1)
    title_utf8 += base::StringPrintf(" %d", copy_number);
  return base::FilePath(title_utf8 + ".app");
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

// Returns a reference to the static UpdateShortcuts lock.
// See https://crbug.com/1090548 for more info.
base::Lock& GetUpdateShortcutsLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
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
  if (!BuildShortcut(staging_path))
    return;

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
    if (!base::CopyDirectory(staging_path, dst_app_path, true)) {
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
  if (creation_reason == SHORTCUT_CREATION_BY_USER)
    RevealAppShimInFinder(updated_app_paths[0]);
  RecordCreateShortcut(CreateShortcutResult::kSuccess);
  return true;
}

static bool g_have_localized_app_dir_name = false;

// static
void WebAppShortcutCreator::ResetHaveLocalizedAppDirNameForTesting() {
  g_have_localized_app_dir_name = false;
}

bool WebAppShortcutCreator::UpdateShortcuts(
    bool create_if_needed,
    std::vector<base::FilePath>* updated_paths) {
  DCHECK(updated_paths && updated_paths->empty());

  if (create_if_needed) {
    const base::FilePath applications_dir = GetChromeAppsFolder();
    if (applications_dir.empty() ||
        !base::DirectoryExists(applications_dir.DirName())) {
      RecordCreateShortcut(CreateShortcutResult::kApplicationDirNotFound);
      LOG(ERROR) << "Couldn't find an Applications directory to copy app to.";
      return false;
    }
    // Only set folder icons and a localized name once. This avoids concurrent
    // calls to -[NSWorkspace setIcon:..], which is not reentrant.
    if (!g_have_localized_app_dir_name) {
      g_have_localized_app_dir_name =
          UpdateAppShortcutsSubdirLocalizedName(applications_dir);
    }
    if (!g_have_localized_app_dir_name) {
      RecordCreateShortcut(CreateShortcutResult::kFailToLocalizeApplication);
      LOG(ERROR) << "Failed to localize " << applications_dir.value();
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
    RecordCreateShortcut(CreateShortcutResult::kFailToGetApplicationPaths);
    LOG(ERROR) << "Failed to get application paths.";
    return false;
  }

  CreateShortcutsAt(app_paths, updated_paths);
  return updated_paths->size() == app_paths.size();
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
    if (substitution)
      plist[key] = substitution;
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

  // 3. Fill in file handlers.
  // The plist needs to contain file handlers for all profiles the app is
  // installed in. `info_->file_handler_extensions` only contains information
  // for the current profile, so combine that with the information from
  // `info_->handlers_per_profile`.
  auto file_handler_extensions =
      GetFileHandlerExtensionsWithoutDot(info_->file_handler_extensions);
  auto file_handler_mime_types = info_->file_handler_mime_types;
  for (const auto& profile_handlers : info_->handlers_per_profile) {
    if (profile_handlers.first == info_->profile_path)
      continue;
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
    if (profile_handlers.first == info_->profile_path)
      continue;
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

  // TODO(crbug.com/1273526): If we decide to rename app bundles on app title
  // changes, instead of relying on localization, then this will need to change
  // to use GetShortcutBaseName, most likely only for non-legacy-apps
  // (in other words, revert to what the code looked like before on these
  // lines). See also crbug.com/1021804.
  base::FilePath app_name = app_path.BaseName().RemoveFinalExtension();
  plist[base::apple::CFToNSPtrCast(kCFBundleNameKey)] =
      base::apple::FilePathToNSString(app_name);

  return [plist writeToURL:plist_url error:nil];
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
  if (!base::CreateDirectory(localized_dir))
    return false;

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
          GetChromeAppsFolder(), info_->app_id, info_->profile_path)) {
    display_name = [bundle_name
        stringByAppendingString:base::SysUTF8ToNSString(
                                    " (" + info_->profile_name + ")")];
  }

  NSDictionary* strings_plist = @{
    base::apple::CFToNSPtrCast(kCFBundleNameKey) : bundle_name,
    app_mode::kCFBundleDisplayNameKey : display_name
  };

  NSString* localized_path = base::apple::FilePathToNSString(
      localized_dir.Append("InfoPlist.strings"));
  return [strings_plist writeToFile:localized_path atomically:YES];
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
      if (icns_encoder.AddImage(MaskedIcon(*it))) {
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
  if (!app_mode::UseAdHocSigningForWebAppShims()) {
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
  std::vector<uint8_t> cd_hash(
      CFDataGetBytePtr(cd_hash_data),
      CFDataGetBytePtr(cd_hash_data) + CFDataGetLength(cd_hash_data));

  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AppShimRegistry::SaveCdHashForApp,
                                base::Unretained(AppShimRegistry::Get()),
                                info_->app_id, std::move(cd_hash)));

  return true;
}

std::vector<base::FilePath> WebAppShortcutCreator::GetAppBundlesByIdUnsorted()
    const {
  // Search using LaunchServices using the default bundle id.
  const std::string bundle_id = GetBundleIdentifierForShim(
      info_->app_id, IsMultiProfile() ? base::FilePath() : info_->profile_path);
  auto bundle_infos = SearchForBundlesById(bundle_id);

  // If in multi-profile mode, search using the profile-scoped bundle id, in
  // case the user has an old shim hanging around.
  if (bundle_infos.empty() && IsMultiProfile()) {
    const std::string profile_scoped_bundle_id =
        GetBundleIdentifierForShim(info_->app_id, info_->profile_path);
    bundle_infos = SearchForBundlesById(profile_scoped_bundle_id);
  }

  std::vector<base::FilePath> bundle_paths;
  for (const auto& bundle_info : bundle_infos)
    bundle_paths.push_back(bundle_info.bundle_path());
  return bundle_paths;
}

std::vector<base::FilePath> WebAppShortcutCreator::GetAppBundlesById() const {
  std::vector<base::FilePath> paths = GetAppBundlesByIdUnsorted();

  // Sort the matches by preference.
  base::FilePath default_path =
      GetApplicationsShortcutPath(/*avoid_conflicts=*/false);

  base::FilePath apps_dir = GetChromeAppsFolder();
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

bool WebAppShortcutCreator::IsMultiProfile() const {
  return info_->is_multi_profile;
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

void LaunchShim(LaunchShimUpdateBehavior update_behavior,
                ShimLaunchMode launch_mode,
                ShimLaunchedCallback launched_callback,
                ShimTerminatedCallback terminated_callback,
                std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (AppShimCreationAndLaunchDisabledForTest() || !shortcut_info) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(launched_callback), base::Process()));
    return;
  }

  internals::PostShortcutIOTask(
      base::BindOnce(&LaunchShimOnFileThread, update_behavior, launch_mode,
                     std::move(launched_callback),
                     std::move(terminated_callback)),
      std::move(shortcut_info));
}

void LaunchShimForTesting(const base::FilePath& shim_path,  // IN-TEST
                          const std::vector<GURL>& urls,
                          ShimLaunchedCallback launched_callback,
                          ShimTerminatedCallback terminated_callback,
                          const base::FilePath& chromium_path) {
  base::CommandLine command_line = BuildCommandLineForShimLaunch();
  command_line.AppendSwitch(app_mode::kLaunchedForTest);
  command_line.AppendSwitch(app_mode::kIsNormalLaunch);
  command_line.AppendSwitchPath(app_mode::kLaunchChromeForTest, chromium_path);

  std::vector<std::string> url_specs;
  url_specs.reserve(urls.size());
  for (const GURL& url : urls) {
    url_specs.push_back(url.spec());
  }

  LaunchApplicationWithRetry(
      shim_path, command_line, url_specs, {.activate = false},
      base::BindOnce(
          [](const base::FilePath& shim_path,
             ShimLaunchedCallback launched_callback,
             ShimTerminatedCallback terminated_callback,
             NSRunningApplication* app, NSError* error) {
            if (error) {
              LOG(ERROR) << "Failed to open application with path: "
                         << shim_path;

              std::move(launched_callback).Run(base::Process());
              return;
            }
            RunAppLaunchCallbacks(app, std::move(launched_callback),
                                  std::move(terminated_callback));
          },
          shim_path, std::move(launched_callback),
          std::move(terminated_callback)));
}

void WaitForShimToQuitForTesting(const base::FilePath& shim_path,  // IN-TEST
                                 const std::string& app_id,
                                 bool terminate_shim) {
  std::string bundle_id = GetBundleIdentifierForShim(app_id);
  NSRunningApplication* matching_app =
      FindRunningApplicationForBundleIdAndPath(bundle_id, shim_path);
  if (!matching_app) {
    LOG(ERROR) << "No matching applications found for app_id " << app_id
               << " and path " << shim_path;
    return;
  }

  if (terminate_shim) {
    [matching_app terminate];
  }

  base::RunLoop loop;
  [TerminationObserver startObservingForRunningApplication:matching_app
                                              withCallback:loop.QuitClosure()];
  loop.Run();
}

// Removes the app shim from the list of Login Items.
void RemoveAppShimFromLoginItems(const std::string& app_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const std::string bundle_id = GetBundleIdentifierForShim(app_id);
  auto bundle_infos = SearchForBundlesById(bundle_id);
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
  }
}

std::string GetBundleIdentifierForShim(const std::string& app_id,
                                       const base::FilePath& profile_path) {
  // Note that this matches APP_MODE_APP_BUNDLE_ID in chrome/chrome.gyp.
  if (!profile_path.empty()) {
    // Replace spaces in the profile path with hyphen.
    std::string normalized_profile_path;
    base::ReplaceChars(profile_path.BaseName().value(), " ", "-",
                       &normalized_profile_path);
    return base::apple::BaseBundleID() + std::string(".app.") +
           normalized_profile_path + "-" + app_id;
  }
  return base::apple::BaseBundleID() + std::string(".app.") + app_id;
}

namespace internals {

bool CreatePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  if (AppShimCreationAndLaunchDisabledForTest()) {
    return true;
  }

  WebAppShortcutCreator shortcut_creator(app_data_path, &shortcut_info);
  return shortcut_creator.CreateShortcuts(creation_reason, creation_locations);
}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  WebAppShortcutCreator shortcut_creator(
      internals::GetShortcutDataDir(shortcut_info), &shortcut_info);
  ShortcutLocations locations;
  if (!shortcut_creator.GetAppBundlesById().empty()) {
    locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }
  return locations;
}

void DeletePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             DeleteShortcutsCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();

  if (test_override) {
    CHECK_IS_TEST();
    test_override->RegisterProtocolSchemes(shortcut_info.app_id,
                                           std::vector<std::string>());
  }
  const std::string bundle_id = GetBundleIdentifierForShim(
      shortcut_info.app_id, shortcut_info.profile_path);
  auto bundle_infos = SearchForBundlesById(bundle_id);
  bool result = true;
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
    if (!base::DeletePathRecursively(bundle_info.bundle_path())) {
      result = false;
    }
  }
  result_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), result));
}

void DeleteMultiProfileShortcutsForApp(const std::string& app_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  const std::string bundle_id = GetBundleIdentifierForShim(app_id);
  auto bundle_infos = SearchForBundlesById(bundle_id);
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
    base::DeletePathRecursively(bundle_info.bundle_path());
  }
}

Result UpdatePlatformShortcuts(
    const base::FilePath& app_data_path,
    const std::u16string& old_app_title,
    absl::optional<ShortcutLocations> user_specified_locations,
    const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  if (AppShimCreationAndLaunchDisabledForTest()) {
    return Result::kOk;
  }

  WebAppShortcutCreator shortcut_creator(app_data_path, &shortcut_info);
  std::vector<base::FilePath> updated_shim_paths;
  return (shortcut_creator.UpdateShortcuts(/*create_if_needed=*/false,
                                           &updated_shim_paths)
              ? Result::kOk
              : Result::kError);
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  std::list<BundleInfoPlist> bundles_info =
      BundleInfoPlist::GetAllInPath(GetChromeAppsFolder(), /*recursive=*/true);
  for (const auto& info : bundles_info) {
    if (!info.IsForCurrentUserDataDir()) {
      continue;
    }
    if (!info.IsForProfile(profile_path)) {
      continue;
    }
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        info.bundle_path());
    base::DeletePathRecursively(info.bundle_path());
  }
}

}  // namespace internals

}  // namespace web_app
