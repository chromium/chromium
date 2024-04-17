// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/mac/app_mode_chrome_locator.h"

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>

#include <optional>
#include <set>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/mac/app_mode_common.h"

namespace app_mode {

namespace {

struct PathAndStructure {
  NSString* __strong framework_dylib_path;
  bool is_new_app_structure;
};

std::optional<PathAndStructure> GetFrameworkDylibPathAndStructure(
    NSString* bundle_path,
    NSString* version) {
  // NEW STYLE:
  // Chromium.app/Contents/Frameworks/Chromium Framework.framework/
  //   Versions/<version>/Chromium Framework
  NSString* path = [NSString pathWithComponents:@[
    bundle_path, @"Contents", @"Frameworks", @(chrome::kFrameworkName),
    @"Versions", version, @(chrome::kFrameworkExecutableName)
  ]];

  if ([NSFileManager.defaultManager fileExistsAtPath:path]) {
    return PathAndStructure{path, true};
  }

  // OLD STYLE:
  // Chromium.app/Contents/Versions/<version>/Chromium Framework.framework/
  //   Versions/A/Chromium Framework
  path = [NSString pathWithComponents:@[
    bundle_path, @"Contents", @"Versions", version, @(chrome::kFrameworkName),
    @"Versions", @"A", @(chrome::kFrameworkExecutableName)
  ]];

  if ([NSFileManager.defaultManager fileExistsAtPath:path]) {
    return PathAndStructure{path, false};
  }

  return std::nullopt;
}

bool IsPathValidForBundle(const base::FilePath& bundle_path,
                          NSString* bundle_id) {
  if (bundle_path.empty())
    return false;

  if (!base::DirectoryExists(bundle_path))
    return false;

  NSString* ns_bundle_path = base::SysUTF8ToNSString(bundle_path.value());
  NSBundle* bundle = [NSBundle bundleWithPath:ns_bundle_path];
  if (!bundle || ![bundle_id isEqualToString:bundle.bundleIdentifier]) {
    return false;
  }

  return true;
}

}  // namespace

bool FindChromeBundle(NSString* bundle_id, base::FilePath* out_bundle) {
  // Retrieve the last-run Chrome bundle location.
  base::FilePath last_run_bundle_path;
  {
    NSString* cr_bundle_path_ns = base::apple::CFToNSOwnershipCast(
        base::apple::CFCastStrict<CFStringRef>(CFPreferencesCopyAppValue(
            base::apple::NSToCFPtrCast(app_mode::kLastRunAppBundlePathPrefsKey),
            base::apple::NSToCFPtrCast(bundle_id))));
    last_run_bundle_path = base::apple::NSStringToFilePath(cr_bundle_path_ns);
  }

  // Look up running instances of the specified bundle ID.
  {
    // Note that IsPathValidForBundle is guaranteed to be true for all elements
    // in `running_bundle_paths` because runningApplicationsWithBundleIdentifier
    // returned them.
    std::set<base::FilePath> running_bundle_paths;
    NSArray<NSRunningApplication*>* running_applications = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:bundle_id];
    for (NSRunningApplication* running_application : running_applications) {
      base::FilePath bundle_path =
          base::apple::NSURLToFilePath(running_application.bundleURL);
      DCHECK(!bundle_path.empty());
      running_bundle_paths.insert(bundle_path);
    }

    // If the last-run instance is still running, then use that instance.
    if (running_bundle_paths.count(last_run_bundle_path)) {
      *out_bundle = last_run_bundle_path;
      return true;
    }

    // Otherwise, select a running bundle path arbitrarily.
    // TODO(crbug.com/40208159): This choice should not be made
    // arbitrarily.
    if (!running_bundle_paths.empty()) {
      *out_bundle = *running_bundle_paths.begin();
      return true;
    }
  }

  // Next, use the last run bundle path, if it is valid.
  if (IsPathValidForBundle(last_run_bundle_path, bundle_id)) {
    *out_bundle = last_run_bundle_path;
    return true;
  }

  // Finally, search the filesystem for a bundle. If several copies of the
  // bundle are present, this will select one arbitrarily.
  {
    // Note that `IsPathValidForBundle` is guaranteed to be true for
    // `bundle_path` because URLForApplicationWithBundleIdentifier returned it.
    NSURL* url = [NSWorkspace.sharedWorkspace
        URLForApplicationWithBundleIdentifier:bundle_id];
    if (url) {
      *out_bundle = base::apple::NSURLToFilePath(url);
      return true;
    }
  }
  return false;
}

bool GetChromeBundleInfo(const base::FilePath& chrome_bundle,
                         const std::string& version_str,
                         base::FilePath* executable_path,
                         base::FilePath* framework_path,
                         base::FilePath* framework_dylib_path) {
  NSString* cr_bundle_path = base::apple::FilePathToNSString(chrome_bundle);
  NSBundle* cr_bundle = [NSBundle bundleWithPath:cr_bundle_path];
  if (!cr_bundle)
    return false;

  // Try to get the version requested, if present.
  std::optional<PathAndStructure> framework_path_and_structure;
  if (!version_str.empty()) {
    framework_path_and_structure = GetFrameworkDylibPathAndStructure(
        cr_bundle_path, base::SysUTF8ToNSString(version_str));
  }

  // If the version requested is not present, or no specific version was
  // requested, fall back to the "current" version. For new-style bundle
  // structures, use the "Current" symlink. (This will intentionally return nil
  // with the old bundle structure.)
  //
  // Note that the scenario where a specific version was requested but is not
  // present is a "should not happen" scenario. Chromium, while it is running,
  // maintains a link to the currently running version, and this function's
  // caller checked to see if the Chromium was still running. However, even in
  // this bizarre case, it's best to find _some_ Chromium.
  if (!framework_path_and_structure) {
    framework_path_and_structure =
        GetFrameworkDylibPathAndStructure(cr_bundle_path, @"Current");
    if (framework_path_and_structure) {
      framework_path_and_structure->framework_dylib_path =
          [framework_path_and_structure
                  ->framework_dylib_path stringByResolvingSymlinksInPath];
    }
  }

  // At this point it is known that it is an old-style bundle structure (or a
  // rather broken new-style bundle). Try explicitly specifying the version of
  // the framework matching the outer bundle version.
  if (!framework_path_and_structure) {
    NSString* cr_version = base::apple::ObjCCast<NSString>([cr_bundle
        objectForInfoDictionaryKey:app_mode::kCFBundleShortVersionStringKey]);
    if (cr_version) {
      framework_path_and_structure =
          GetFrameworkDylibPathAndStructure(cr_bundle_path, cr_version);
    }
  }

  if (!framework_path_and_structure)
    return false;

  // A few sanity checks.
  BOOL is_directory;
  BOOL exists = [NSFileManager.defaultManager
      fileExistsAtPath:framework_path_and_structure->framework_dylib_path
           isDirectory:&is_directory];
  if (!exists || is_directory)
    return false;

  NSString* cr_framework_path;

  if (framework_path_and_structure->is_new_app_structure) {
    // For the path to the framework version itself, remove the framework name.
    cr_framework_path =
        [framework_path_and_structure
                ->framework_dylib_path stringByDeletingLastPathComponent];
  } else {
    // For the path to the framework itself, remove the framework name ...
    cr_framework_path =
        [framework_path_and_structure
                ->framework_dylib_path stringByDeletingLastPathComponent];
    // ... the "A" ...
    cr_framework_path = [cr_framework_path stringByDeletingLastPathComponent];
    // ... and the "Versions" directory.
    cr_framework_path = [cr_framework_path stringByDeletingLastPathComponent];
  }

  // Everything is OK; copy the output parameters.
  *executable_path = base::apple::NSStringToFilePath(cr_bundle.executablePath);
  *framework_path = base::apple::NSStringToFilePath(cr_framework_path);
  *framework_dylib_path = base::apple::NSStringToFilePath(
      framework_path_and_structure->framework_dylib_path);
  return true;
}

}  // namespace app_mode
