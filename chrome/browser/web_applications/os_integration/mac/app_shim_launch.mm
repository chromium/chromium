// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/app_shim_launch.h"

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#import "base/mac/launch_application.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version_info/version_info.h"
#import "chrome/browser/web_applications/os_integration/mac/app_shim_termination_observer.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/common/chrome_constants.h"
#import "chrome/common/mac/app_mode_common.h"
#include "components/variations/net/variations_command_line.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace web_app {

namespace {

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
  [AppShimTerminationObserver
      startObservingForRunningApplication:app
                             withCallback:std::move(termination_callback)];
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
              // TODO(mek): Use "current" task runner?
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
                            bool use_ad_hoc_signing_for_web_app_shims,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback,
                            std::unique_ptr<ShortcutInfo> shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Recreate shims if requested, and populate |shim_paths| with the paths to
  // attempt to launch.
  bool launched_after_rebuild = false;
  std::vector<base::FilePath> shim_paths;
  bool shortcuts_updated = true;
  std::string bundle_id;

  {
    // Nested scope ensures that `shortcut_creator` is destroyed before the
    // `shortcut_info` it references.
    WebAppShortcutCreator shortcut_creator(
        internals::GetShortcutDataDir(*shortcut_info), GetChromeAppsFolder(),
        shortcut_info.get(), use_ad_hoc_signing_for_web_app_shims);

    // Recreate shims if requested, and populate |shim_paths| with the paths
    // to attempt to launch.
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
    LOG_IF(ERROR, !shortcuts_updated)
        << "Could not write shortcut for app shim.";

    bundle_id = shortcut_creator.GetAppBundleId();
  }

  // shortcut_info is no longer needed. Destroy it on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(shortcut_info)));

  LaunchTheFirstShimThatWorksOnFileThread(
      shim_paths, launched_after_rebuild, launch_mode, bundle_id,
      std::move(launched_callback), std::move(terminated_callback));
}

}  // namespace

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

  internals::PostAsyncShortcutIOTask(
      base::BindOnce(&LaunchShimOnFileThread, update_behavior, launch_mode,
                     UseAdHocSigningForWebAppShims(),
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
  [AppShimTerminationObserver
      startObservingForRunningApplication:matching_app
                             withCallback:loop.QuitClosure()];
  loop.Run();
}

}  // namespace web_app
