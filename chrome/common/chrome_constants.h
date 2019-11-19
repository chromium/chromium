// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H_
#define CHROME_COMMON_CHROME_CONSTANTS_H_

#include <stddef.h>

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace chrome {

extern const char kChromeVersion[];
extern const base::FilePath::CharType kBrowserProcessExecutableName[];
extern const base::FilePath::CharType kHelperProcessExecutableName[];
extern const base::FilePath::CharType kBrowserProcessExecutablePath[];
extern const base::FilePath::CharType kHelperProcessExecutablePath[];
extern const base::FilePath::CharType kBrowserProcessExecutableNameChromium[];
extern const base::FilePath::CharType kHelperProcessExecutableNameChromium[];
extern const base::FilePath::CharType kBrowserProcessExecutablePathChromium[];
extern const base::FilePath::CharType kHelperProcessExecutablePathChromium[];
#if defined(OS_MACOSX)
// NOTE: if you change the value of kFrameworkName, please don't forget to
// update components/test/run_all_unittests.cc as well.
// TODO(tfarina): Remove the comment above, when you fix components to use plist
// on Mac.
extern const base::FilePath::CharType kFrameworkName[];
extern const base::FilePath::CharType kFrameworkExecutableName[];
#endif  // OS_MACOSX
#if defined(OS_WIN)
extern const base::FilePath::CharType kBrowserResourcesDll[];
extern const base::FilePath::CharType kChildDll[];
extern const base::FilePath::CharType kElfDll[];
extern const base::FilePath::CharType kStatusTrayWindowClass[];
#endif  // defined(OS_WIN)

extern const char kInitialProfile[];
extern const char kMultiProfileDirPrefix[];
extern const base::FilePath::CharType kGuestProfileDir[];
extern const base::FilePath::CharType kSystemProfileDir[];

// filenames
extern const base::FilePath::CharType kCacheDirname[];
extern const base::FilePath::CharType kCookieFilename[];
extern const base::FilePath::CharType kCRLSetFilename[];
extern const base::FilePath::CharType kCustomDictionaryFileName[];
extern const base::FilePath::CharType kDownloadServiceStorageDirname[];
extern const base::FilePath::CharType kExtensionActivityLogFilename[];
extern const base::FilePath::CharType kExtensionsCookieFilename[];
extern const base::FilePath::CharType kFeatureEngagementTrackerStorageDirname[];
extern const base::FilePath::CharType kFirstRunSentinel[];
extern const base::FilePath::CharType kGCMStoreDirname[];
extern const base::FilePath::CharType kHeavyAdInterventionOptOutDBFilename[];
extern const base::FilePath::CharType kLocalStateFilename[];
extern const base::FilePath::CharType kMediaCacheDirname[];
extern const base::FilePath::CharType kNetworkPersistentStateFilename[];
extern const base::FilePath::CharType kNotificationSchedulerStorageDirname[];
extern const base::FilePath::CharType kOfflinePageArchivesDirname[];
extern const base::FilePath::CharType kOfflinePageMetadataDirname[];
extern const base::FilePath::CharType kOfflinePagePrefetchStoreDirname[];
extern const base::FilePath::CharType kOfflinePageRequestQueueDirname[];
extern const base::FilePath::CharType kPreferencesFilename[];
extern const base::FilePath::CharType kPreviewsOptOutDBFilename[];
extern const base::FilePath::CharType kReadmeFilename[];
extern const base::FilePath::CharType kSecurePreferencesFilename[];
extern const base::FilePath::CharType kServiceStateFileName[];
extern const base::FilePath::CharType kSingletonCookieFilename[];
extern const base::FilePath::CharType kSingletonLockFilename[];
extern const base::FilePath::CharType kSingletonSocketFilename[];
extern const base::FilePath::CharType kSupervisedUserSettingsFilename[];
extern const base::FilePath::CharType kThemePackFilename[];
extern const base::FilePath::CharType kWebAppDirname[];
extern const base::FilePath::CharType kReportingAndNelStoreFilename[];

#if defined(OS_WIN)
extern const base::FilePath::CharType kJumpListIconDirname[];
#endif

// File name of the Pepper Flash plugin on different platforms.
extern const base::FilePath::CharType kPepperFlashPluginFilename[];

// directory names
#if defined(OS_WIN)
extern const wchar_t kUserDataDirname[];
#endif

// Fraction of the total number of processes to be used for hosting
// extensions. If we have more extensions than this percentage, we will start
// combining extensions in existing processes. This allows web pages to have
// enough render processes and not be starved when a lot of extensions are
// installed.
extern const float kMaxShareOfExtensionProcesses;

#if defined(OS_LINUX)
// The highest and lowest assigned OOM score adjustment
// (oom_score_adj) used by the OomPriority Manager.
extern const int kLowestRendererOomScore;
extern const int kHighestRendererOomScore;
#endif

#if defined(OS_CHROMEOS)
// Chrome OS profile directories have custom prefix.
// Profile path format: [user_data_dir]/u-[$hash]
// Ex.: /home/chronos/u-0123456789
extern const char kProfileDirPrefix[];

// Legacy profile dir that was used when only one cryptohome has been mounted.
extern const char kLegacyProfileDir[];

// This must be kept in sync with TestingProfile::kTestUserProfileDir.
extern const char kTestUserProfileDir[];

extern const char kLockScreenAppProfile[];
#endif

// Used to identify the application to the system AV function in Windows.
extern const char kApplicationClientIDStringForAVScanning[];

// The largest reasonable length we'd assume for a meta tag attribute.
extern const size_t kMaxMetaTagAttributeLength;

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
