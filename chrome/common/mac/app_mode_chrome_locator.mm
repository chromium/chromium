// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/mac/app_mode_chrome_locator.h"

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/mac/app_mode_common.h"

namespace app_mode {

namespace {

struct PathAndStructure {
  NSString* framework_dylib_path;  // weak
  bool is_new_app_structure;
};

base::Optional<PathAndStructure> GetFrameworkDylibPathAndStructure(
    NSString* bundle_path,
    NSString* version) {
  // NEW STYLE:
  // Chromium.app/Contents/Frameworks/Chromium Framework.framework/
  //   Versions/<version>/Chromium Framework
  NSString* path = [NSString pathWithComponents:@[
    bundle_path, @"Contents", @"Frameworks", @(chrome::kFrameworkName),
    @"Versions", version, @(chrome::kFrameworkExecutableName)
  ]];

  if ([[NSFileManager defaultManager] fileExistsAtPath:path])
    return PathAndStructure{path, true};

  // OLD STYLE:
  // Chromium.app/Contents/Versions/<version>/Chromium Framework.framework/
  //   Versions/A/Chromium Framework
  path = [NSString pathWithComponents:@[
    bundle_path, @"Contents", @"Versions", version, @(chrome::kFrameworkName),
    @"Versions", @"A", @(chrome::kFrameworkExecutableName)
  ]];

  if ([[NSFileManager defaultManager] fileExistsAtPath:path])
    return PathAndStructure{path, false};

  return base::nullopt;
}

}  // namespace

bool FindBundleById(NSString* bundle_id, base::FilePath* out_bundle) {
  NSWorkspace* ws = [NSWorkspace sharedWorkspace];
  NSString *bundlePath = [ws absolutePathForAppBundleWithIdentifier:bundle_id];
  if (!bundlePath)
    return false;

  *out_bundle = base::mac::NSStringToFilePath(bundlePath);
  return true;
}

bool GetChromeBundleInfo(const base::FilePath& chrome_bundle,
                         const std::string& version_str,
                         base::FilePath* executable_path,
                         base::FilePath* framework_path,
                         base::FilePath* framework_dylib_path) {
  NSString* cr_bundle_path = base::mac::FilePathToNSString(chrome_bundle);
  NSBundle* cr_bundle = [NSBundle bundleWithPath:cr_bundle_path];
  if (!cr_bundle)
    return false;

  // Try to get the version requested, if present.
  base::Optional<PathAndStructure> framework_path_and_structure;
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
    NSString* cr_version = base::mac::ObjCCast<NSString>([cr_bundle
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
  BOOL exists = [[NSFileManager defaultManager]
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
  *executable_path = base::mac::NSStringToFilePath([cr_bundle executablePath]);
  *framework_path = base::mac::NSStringToFilePath(cr_framework_path);
  *framework_dylib_path = base::mac::NSStringToFilePath(
      framework_path_and_structure->framework_dylib_path);
  return true;
}

}  // namespace app_mode
