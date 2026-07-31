// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H_
#define CHROME_COMMON_CHROME_CONSTANTS_H_

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
inline constexpr base::FilePath::CharType kBrowserResourcesDll[] =
    FILE_PATH_LITERAL("chrome.dll");
inline constexpr base::FilePath::CharType kElfDll[] =
    FILE_PATH_LITERAL("chrome_elf.dll");
inline constexpr base::FilePath::CharType kStatusTrayWindowClass[] =
    FILE_PATH_LITERAL("Chrome_StatusTrayWindow");
#endif  // BUILDFLAG(IS_WIN)

inline constexpr char kInitialProfile[] = "Default";
inline constexpr char kMultiProfileDirPrefix[] = "Profile ";
inline constexpr base::FilePath::CharType kGuestProfileDir[] =
    FILE_PATH_LITERAL("Guest Profile");
inline constexpr base::FilePath::CharType kSystemProfileDir[] =
    FILE_PATH_LITERAL("System Profile");

// filenames
inline constexpr base::FilePath::CharType kAccountPreferencesFilename[] =
    FILE_PATH_LITERAL("AccountPreferences");
inline constexpr base::FilePath::CharType kCacheDirname[] =
    FILE_PATH_LITERAL("Cache");
inline constexpr base::FilePath::CharType kCookieFilename[] =
    FILE_PATH_LITERAL("Cookies");
inline constexpr base::FilePath::CharType kCRLSetFilename[] =
    FILE_PATH_LITERAL("Certificate Revocation Lists");
inline constexpr base::FilePath::CharType kCustomDictionaryFileName[] =
    FILE_PATH_LITERAL("Custom Dictionary.txt");
inline constexpr base::FilePath::CharType kDeviceBoundSessionsFilename[] =
    FILE_PATH_LITERAL("Device Bound Sessions");
inline constexpr base::FilePath::CharType kDownloadServiceStorageDirname[] =
    FILE_PATH_LITERAL("Download Service");
inline constexpr base::FilePath::CharType kExtensionActivityLogFilename[] =
    FILE_PATH_LITERAL("Extension Activity");
inline constexpr base::FilePath::CharType kExtensionsCookieFilename[] =
    FILE_PATH_LITERAL("Extension Cookies");
inline constexpr base::FilePath::CharType
    kFeatureEngagementTrackerStorageDirname[] =
        FILE_PATH_LITERAL("Feature Engagement Tracker");
inline constexpr base::FilePath::CharType kFirstRunSentinel[] =
    FILE_PATH_LITERAL("First Run");
inline constexpr base::FilePath::CharType kGCMStoreDirname[] =
    FILE_PATH_LITERAL("GCM Store");
inline constexpr base::FilePath::CharType kLocalStateFilename[] =
    FILE_PATH_LITERAL("Local State");
inline constexpr base::FilePath::CharType kMediaCacheDirname[] =
    FILE_PATH_LITERAL("Media Cache");
inline constexpr base::FilePath::CharType kNetworkPersistentStateFilename[] =
    FILE_PATH_LITERAL("Network Persistent State");
inline constexpr base::FilePath::CharType kNetworkDataDirname[] =
    FILE_PATH_LITERAL("Network");
inline constexpr base::FilePath::CharType
    kNotificationSchedulerStorageDirname[] =
        FILE_PATH_LITERAL("Notification Scheduler");
inline constexpr base::FilePath::CharType kOfflinePageArchivesDirname[] =
    FILE_PATH_LITERAL("Offline Pages/archives");
inline constexpr base::FilePath::CharType kOfflinePageMetadataDirname[] =
    FILE_PATH_LITERAL("Offline Pages/metadata");
inline constexpr base::FilePath::CharType kOfflinePagePrefetchStoreDirname[] =
    FILE_PATH_LITERAL("Offline Pages/prefech_store");
inline constexpr base::FilePath::CharType kOfflinePageRequestQueueDirname[] =
    FILE_PATH_LITERAL("Offline Pages/request_queue");
inline constexpr base::FilePath::CharType kPreferencesFilename[] =
    FILE_PATH_LITERAL("Preferences");
inline constexpr base::FilePath::CharType kPreviewsOptOutDBFilename[] =
    FILE_PATH_LITERAL("previews_opt_out.db");
inline constexpr base::FilePath::CharType kQueryTileStorageDirname[] =
    FILE_PATH_LITERAL("Query Tiles");
inline constexpr base::FilePath::CharType kReadmeFilename[] =
    FILE_PATH_LITERAL("README");
inline constexpr base::FilePath::CharType kSCTAuditingPendingReportsFileName[] =
    FILE_PATH_LITERAL("SCT Auditing Pending Reports");
inline constexpr base::FilePath::CharType kSecurePreferencesFilename[] =
    FILE_PATH_LITERAL("Secure Preferences");
inline constexpr base::FilePath::CharType kServiceStateFileName[] =
    FILE_PATH_LITERAL("Service State");
inline constexpr base::FilePath::CharType
    kSegmentationPlatformStorageDirName[] =
        FILE_PATH_LITERAL("Segmentation Platform");
inline constexpr base::FilePath::CharType kSingletonCookieFilename[] =
    FILE_PATH_LITERAL("SingletonCookie");
inline constexpr base::FilePath::CharType kSingletonLockFilename[] =
    FILE_PATH_LITERAL("SingletonLock");
inline constexpr base::FilePath::CharType kSingletonSocketFilename[] =
    FILE_PATH_LITERAL("SingletonSocket");
inline constexpr base::FilePath::CharType kThemePackFilename[] =
    FILE_PATH_LITERAL("Cached Theme.pak");
inline constexpr base::FilePath::CharType
    kTransportSecurityPersisterFilename[] =
        FILE_PATH_LITERAL("TransportSecurity");
inline constexpr base::FilePath::CharType kTrustTokenFilename[] =
    FILE_PATH_LITERAL("Trust Tokens");
inline constexpr base::FilePath::CharType kVideoTutorialsStorageDirname[] =
    FILE_PATH_LITERAL("Video Tutorials");
inline constexpr base::FilePath::CharType kWebAppDirname[] =
    FILE_PATH_LITERAL("Web Applications");
// Only use if the ENABLE_REPORTING build flag is true
inline constexpr base::FilePath::CharType kReportingAndNelStoreFilename[] =
    FILE_PATH_LITERAL("Reporting and NEL");

#if BUILDFLAG(IS_WIN)
inline constexpr base::FilePath::CharType kJumpListIconDirname[] =
    FILE_PATH_LITERAL("JumpListIcons");
#endif

// directory names
#if BUILDFLAG(IS_WIN)
inline constexpr wchar_t kUserDataDirname[] = L"User Data";
#elif BUILDFLAG(IS_ANDROID)
inline constexpr base::FilePath::CharType kOTRTempStateDirname[] =
    FILE_PATH_LITERAL("OTRTempState");
#endif

// Fraction of the soft process limit that can be consumed by extensions, before
// additional extension processes are ignored. By allowing this many extension
// processes to count toward the limit, Chrome takes steps to limit the process
// count (e.g., using same-site process sharing) when there are many tabs and
// extensions. By ignoring extensions beyond this fraction, Chrome ensures that
// a very large number of extensions cannot immediately force the user into a
// one-process-per-site mode for all tabs (with poor responsiveness), while
// still securely isolating each extension in its own process.
inline constexpr float kMaxShareOfExtensionProcesses = 0.30f;

// This GUID is associated with any 'don't ask me again' settings that the
// user can select for different file types.
// {2676A9A2-D919-4FEE-9187-152100393AB2}
inline constexpr char kApplicationClientIDStringForAVScanning[] =
    "2676A9A2-D919-4FEE-9187-152100393AB2";

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
