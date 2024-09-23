// Copyright 2012 The Chromium Authors
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
#if BUILDFLAG(IS_MAC)
extern const base::FilePath::CharType
    kGoogleChromeForTestingBrowserProcessExecutablePath[];
extern const base::FilePath::CharType
    kGoogleChromeBrowserProcessExecutablePath[];
extern const base::FilePath::CharType kChromiumBrowserProcessExecutablePath[];
// NOTE: if you change the value of kFrameworkName, please don't forget to
// update components/test/run_all_unittests.cc as well.
// TODO(tfarina): Remove the comment above, when you fix components to use plist
// on Mac.
extern const base::FilePath::CharType kFrameworkName[];
extern const base::FilePath::CharType kFrameworkExecutableName[];
// Suffix added to the helper app name to display alert notifications. Must be
// kept in sync with the value in alert_helper_params (//chrome/BUILD.gn).
extern const char kMacHelperSuffixAlerts[];
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
extern const base::FilePath::CharType kBrowserResourcesDll[];
extern const base::FilePath::CharType kElfDll[];
extern const base::FilePath::CharType kStatusTrayWindowClass[];
#endif  // BUILDFLAG(IS_WIN)

extern const char kInitialProfile[];
extern const char kMultiProfileDirPrefix[];
extern const base::FilePath::CharType kGuestProfileDir[];
extern const base::FilePath::CharType kSystemProfileDir[];

// filenames
extern const base::FilePath::CharType kAccountPreferencesFilename[];
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
extern const base::FilePath::CharType kLocalStateFilename[];
extern const base::FilePath::CharType kMediaCacheDirname[];
extern const base::FilePath::CharType kNetworkPersistentStateFilename[];
extern const base::FilePath::CharType kNetworkDataDirname[];
extern const base::FilePath::CharType kNotificationSchedulerStorageDirname[];
extern const base::FilePath::CharType kOfflinePageArchivesDirname[];
extern const base::FilePath::CharType kOfflinePageMetadataDirname[];
extern const base::FilePath::CharType kOfflinePagePrefetchStoreDirname[];
extern const base::FilePath::CharType kOfflinePageRequestQueueDirname[];
extern const base::FilePath::CharType kPreferencesFilename[];
extern const base::FilePath::CharType kPreviewsOptOutDBFilename[];
extern const base::FilePath::CharType kQueryTileStorageDirname[];
extern const base::FilePath::CharType kReadmeFilename[];
extern const base::FilePath::CharType kReportingAndNelStoreFilename[];
extern const base::FilePath::CharType kSCTAuditingPendingReportsFileName[];
extern const base::FilePath::CharType kSecurePreferencesFilename[];
extern const base::FilePath::CharType kSegmentationPlatformStorageDirName[];
extern const base::FilePath::CharType kServiceStateFileName[];
extern const base::FilePath::CharType kSingletonCookieFilename[];
extern const base::FilePath::CharType kSingletonLockFilename[];
extern const base::FilePath::CharType kSingletonSocketFilename[];
extern const base::FilePath::CharType kThemePackFilename[];
extern const base::FilePath::CharType kTransportSecurityPersisterFilename[];
extern const base::FilePath::CharType kTrustTokenFilename[];
extern const base::FilePath::CharType kVideoTutorialsStorageDirname[];
extern const base::FilePath::CharType kWebAppDirname[];

#if BUILDFLAG(IS_WIN)
extern const base::FilePath::CharType kJumpListIconDirname[];
#endif

// directory names
#if BUILDFLAG(IS_WIN)
extern const wchar_t kUserDataDirname[];
#elif BUILDFLAG(IS_ANDROID)
extern const base::FilePath::CharType kOTRTempStateDirname[];
#endif

// Fraction of the soft process limit that can be consumed by extensions, before
// additional extension processes are ignored. By allowing this many extension
// processes to count toward the limit, Chrome takes steps to limit the process
// count (e.g., using same-site process sharing) when there are many tabs and
// extensions. By ignoring extensions beyond this fraction, Chrome ensures that
// a very large number of extensions cannot immediately force the user into a
// one-process-per-site mode for all tabs (with poor responsiveness), while
// still securely isolating each extension in its own process.
extern const float kMaxShareOfExtensionProcesses;

// Used to identify the application to the system AV function in Windows.
extern const char kApplicationClientIDStringForAVScanning[];

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
