// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <string.h>

#include <memory>
#include <string>

#import "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/memory/free_deleter.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"

namespace {

// Return an NSBundle* as the internal implementation of
// chrome::OuterAppBundle(), which should be the only caller.
NSBundle* OuterAppBundleInternal() {
  @autoreleasepool {
    if (!base::apple::AmIBundled()) {
      // If unbundled (as in a test), there's no app bundle.
      return nil;
    }

    if (!base::apple::IsBackgroundOnlyProcess()) {
      // Shortcut: in the browser process, just return the main app bundle.
      return NSBundle.mainBundle;
    }

    // From C.app/Contents/Frameworks/C.framework/Versions/1.2.3.4, go up five
    // steps to C.app.
    base::FilePath framework_path = chrome::GetFrameworkBundlePath();
    base::FilePath outer_app_dir =
        framework_path.DirName().DirName().DirName().DirName().DirName();
    NSString* outer_app_dir_ns = base::SysUTF8ToNSString(outer_app_dir.value());

    return [NSBundle bundleWithPath:outer_app_dir_ns];
  }
}

char* ProductDirNameForBundle(NSBundle* chrome_bundle) {
  @autoreleasepool {
    const char* product_dir_name = nullptr;

    NSString* product_dir_name_ns =
        [chrome_bundle objectForInfoDictionaryKey:@"CrProductDirName"];
    product_dir_name = [product_dir_name_ns fileSystemRepresentation];

    if (!product_dir_name) {
#if BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
      product_dir_name = "Google/Chrome for Testing";
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
      product_dir_name = "Google/Chrome";
#else
      product_dir_name = "Chromium";
#endif
    }

    // Leaked, but the only caller initializes a static with this result, so it
    // only happens once, and that's OK.
    return strdup(product_dir_name);
  }
}

// ProductDirName returns the name of the directory inside
// ~/Library/Application Support that should hold the product application
// data. This can be overridden by setting the CrProductDirName key in the
// outer browser .app's Info.plist. The default is "Google/Chrome" for
// officially-branded builds, and "Chromium" for unbranded builds. For the
// official canary channel, the Info.plist will have CrProductDirName set
// to "Google/Chrome Canary".
std::string ProductDirName() {
  // Use OuterAppBundle() to get the main app's bundle. This key needs to live
  // in the main app's bundle because it will be set differently on the canary
  // channel, and the autoupdate system dictates that there can be no
  // differences between channels within the versioned directory. This would
  // normally use base::apple::FrameworkBundle(), but that references the
  // framework bundle within the versioned directory. Ordinarily, the profile
  // should not be accessed from non-browser processes, but those processes do
  // attempt to get the profile directory, so direct them to look in the outer
  // browser .app's Info.plist for the CrProductDirName key.
  static const char* product_dir_name =
      ProductDirNameForBundle(chrome::OuterAppBundle());
  return std::string(product_dir_name);
}

bool GetDefaultUserDataDirectoryForProduct(const std::string& product_dir,
                                           base::FilePath* result) {
  bool success = false;
  if (result && base::PathService::Get(base::DIR_APP_DATA, result)) {
    *result = result->Append(product_dir);
    success = true;
  }
  return success;
}

}  // namespace

namespace chrome {

bool GetDefaultUserDataDirectory(base::FilePath* result) {
  return GetDefaultUserDataDirectoryForProduct(ProductDirName(), result);
}

bool GetUserDocumentsDirectory(base::FilePath* result) {
  return base::apple::GetUserDirectory(NSDocumentDirectory, result);
}

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  // If the profile directory is under ~/Library/Application Support,
  // use a suitable cache directory under ~/Library/Caches.  For
  // example, a profile directory of ~/Library/Application
  // Support/Google/Chrome/MyProfileName would use the cache directory
  // ~/Library/Caches/Google/Chrome/MyProfileName.

  // Default value in cases where any of the following fails.
  *result = profile_dir;

  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_APP_DATA, &app_data_dir))
    return;
  base::FilePath cache_dir;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir))
    return;
  if (!app_data_dir.AppendRelativePath(profile_dir, &cache_dir))
    return;

  *result = cache_dir;
}

bool GetUserDownloadsDirectory(base::FilePath* result) {
  return base::apple::GetUserDirectory(NSDownloadsDirectory, result);
}

bool GetUserMusicDirectory(base::FilePath* result) {
  return base::apple::GetUserDirectory(NSMusicDirectory, result);
}

bool GetUserPicturesDirectory(base::FilePath* result) {
  return base::apple::GetUserDirectory(NSPicturesDirectory, result);
}

bool GetUserVideosDirectory(base::FilePath* result) {
  return base::apple::GetUserDirectory(NSMoviesDirectory, result);
}

base::FilePath GetFrameworkBundlePath() {
  // It's tempting to use +[NSBundle bundleWithIdentifier:], but it's really
  // slow (about 30ms on 10.5 and 10.6), despite Apple's documentation stating
  // that it may be more efficient than +bundleForClass:.  +bundleForClass:
  // itself takes 1-2ms.  Getting an NSBundle from a path, on the other hand,
  // essentially takes no time at all, at least when the bundle has already
  // been loaded as it will have been in this case.  The FilePath operations
  // needed to compute the framework's path are also effectively free, so that
  // is the approach that is used here.  NSBundle is also documented as being
  // not thread-safe, and thread safety may be a concern here.

  // Start out with the path to the running executable.
  base::FilePath path;
  base::PathService::Get(base::FILE_EXE, &path);

  // One step up to MacOS, another to Contents.
  path = path.DirName().DirName();
  DCHECK_EQ(path.BaseName().value(), "Contents");

  if (base::apple::IsBackgroundOnlyProcess()) {
    // |path| is Chromium.app/Contents/Frameworks/Chromium Framework.framework/
    // Versions/X/Helpers/Chromium Helper.app/Contents. Go up three times to
    // the versioned framework directory.
    path = path.DirName().DirName().DirName();
  } else {
    // |path| is Chromium.app/Contents, so go down to
    // Chromium.app/Contents/Frameworks/Chromium Framework.framework/Versions/X.
    path = path.Append("Frameworks")
               .Append(kFrameworkName)
               .Append("Versions")
               .Append(kChromeVersion);
  }
  DCHECK_EQ(path.BaseName().value(), kChromeVersion);
  DCHECK_EQ(path.DirName().BaseName().value(), "Versions");
  DCHECK_EQ(path.DirName().DirName().BaseName().value(), kFrameworkName);
  DCHECK_EQ(path.DirName().DirName().DirName().BaseName().value(),
            "Frameworks");
  DCHECK_EQ(path.DirName()
                .DirName()
                .DirName()
                .DirName()
                .DirName()
                .BaseName()
                .Extension(),
            ".app");
  return path;
}

bool GetLocalLibraryDirectory(base::FilePath* result) {
  return base::apple::GetLocalDirectory(NSLibraryDirectory, result);
}

bool GetGlobalApplicationSupportDirectory(base::FilePath* result) {
  return base::apple::GetLocalDirectory(NSApplicationSupportDirectory, result);
}

NSBundle* OuterAppBundle() {
  static NSBundle* bundle = OuterAppBundleInternal();
  return bundle;
}

bool ProcessNeedsProfileDir(const std::string& process_type) {
  // For now we have no reason to forbid this on other macOS as we don't
  // have the roaming profile troubles there.
  return true;
}

}  // namespace chrome
