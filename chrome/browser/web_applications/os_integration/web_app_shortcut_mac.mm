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
#include <string_view>
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
#include "chrome/browser/shortcuts/platform_util_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#import "chrome/browser/web_applications/os_integration/mac/bundle_info_plist.h"
#include "chrome/browser/web_applications/os_integration/mac/icns_encoder.h"
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/chrome_unscaled_resources.h"
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

// TODO(crbug.com/41446873): Change all launch functions to take a single
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
  // TODO(crbug.com/41446873): This watches for termination indefinitely,
  // but we only need to watch for termination until the app establishes a
  // (whereupon termination will be noticed by the mojo connection closing).
  std::move(launch_callback).Run(std::move(process));
  [TerminationObserver
      startObservingForRunningApplication:app
                             withCallback:std::move(termination_callback)];
}

namespace web_app {

namespace {

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
      internals::GetShortcutDataDir(shortcut_info), GetChromeAppsFolder(),
      &shortcut_info);

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

}  // namespace

bool AppShimCreationAndLaunchDisabledForTest() {
  // Note: The kTestType switch is only added on browser tests, but not unit
  // tests. Unit tests need to set the test override.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kTestType) &&
         !OsIntegrationTestOverride::Get();
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
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder());
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

  WebAppShortcutCreator shortcut_creator(app_data_path, GetChromeAppsFolder(),
                                         &shortcut_info);
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
      internals::GetShortcutDataDir(shortcut_info), GetChromeAppsFolder(),
      &shortcut_info);
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
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder());
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
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder());
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
    base::DeletePathRecursively(bundle_info.bundle_path());
  }
}

Result UpdatePlatformShortcuts(
    const base::FilePath& app_data_path,
    const std::u16string& old_app_title,
    std::optional<ShortcutLocations> user_specified_locations,
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

  WebAppShortcutCreator shortcut_creator(app_data_path, GetChromeAppsFolder(),
                                         &shortcut_info);
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
