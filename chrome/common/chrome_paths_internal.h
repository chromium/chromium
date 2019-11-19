// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
#define CHROME_COMMON_CHROME_PATHS_INTERNAL_H_

#include <string>

#include "build/build_config.h"

#if defined(OS_MACOSX)
#if defined(__OBJC__)
@class NSBundle;
#else
class NSBundle;
#endif
#endif

namespace base {
class FilePath;
}

namespace chrome {

// Get the path to the user's data directory, regardless of whether
// DIR_USER_DATA has been overridden by a command-line option.
bool GetDefaultUserDataDirectory(base::FilePath* result);

#if defined(OS_WIN)
// Get the path to the roaming user's data directory, regardless of whether
// DIR_ROAMING_USER_DATA has been overridden by a command-line option.
bool GetDefaultRoamingUserDataDirectory(base::FilePath* result);
#endif

// Get the path to the user's cache directory.  This is normally the
// same as the profile directory, but on Linux it can also be
// $XDG_CACHE_HOME and on Mac it can be under ~/Library/Caches.
// Note that the Chrome cache directories are actually subdirectories
// of this directory, with names like "Cache" and "Media Cache".
// This will always fill in |result| with a directory, sometimes
// just |profile_dir|.
void GetUserCacheDirectory(const base::FilePath& profile_dir, base::FilePath* result);

// Get the path to the user's documents directory.
bool GetUserDocumentsDirectory(base::FilePath* result);

#if defined(OS_WIN) || defined(OS_LINUX)
// Gets the path to a safe default download directory for a user.
bool GetUserDownloadsDirectorySafe(base::FilePath* result);
#endif

// Get the path to the user's downloads directory.
bool GetUserDownloadsDirectory(base::FilePath* result);

// Gets the path to the user's music directory.
bool GetUserMusicDirectory(base::FilePath* result);

// Gets the path to the user's pictures directory.
bool GetUserPicturesDirectory(base::FilePath* result);

// Gets the path to the user's videos directory.
bool GetUserVideosDirectory(base::FilePath* result);

#if defined(OS_MACOSX)
// Most of the application is further contained within the framework, which
// resides in the Frameworks directory of the top-level Contents folder. The
// framework is versioned with the full product version. This function returns
// the full path to the versioned sub-directory of the framework, i.e.:
// Chromium.app/Contents/Frameworks/Chromium Framework.framework/Versions/X.
base::FilePath GetFrameworkBundlePath();

// Get the local library directory.
bool GetLocalLibraryDirectory(base::FilePath* result);

// Get the global Application Support directory (under /Library/).
bool GetGlobalApplicationSupportDirectory(base::FilePath* result);

// Returns the NSBundle for the outer browser application, even when running
// inside the helper. In unbundled applications, such as tests, returns nil.
NSBundle* OuterAppBundle();

// Get the user data directory for the Chrome browser bundle at |bundle|.
// |bundle| should be the same value that would be returned from +[NSBundle
// mainBundle] if Chrome were launched normaly. This is used by app shims,
// which run from a bundle which isn't Chrome itself, but which need access to
// the user data directory to connect to a UNIX-domain socket therein.
// Returns false if there was a problem fetching the app data directory.
bool GetUserDataDirectoryForBrowserBundle(NSBundle* bundle,
                                          base::FilePath* result);

#endif  // OS_MACOSX
// Checks if the |process_type| has the rights to access the profile.
bool ProcessNeedsProfileDir(const std::string& process_type);

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
