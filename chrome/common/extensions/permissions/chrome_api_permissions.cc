// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"

namespace extensions {
namespace chrome_api_permissions {

namespace {

template <typename T>
std::unique_ptr<APIPermission> CreateAPIPermission(
    const APIPermissionInfo* permission) {
  return std::make_unique<T>(permission);
}

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr APIPermissionInfo::InitInfo permissions_to_register[] = {
    // Register permissions for all extension types.
    {APIPermission::kBackground, "background",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDeclarativeContent, "declarativeContent"},
    {APIPermission::kDesktopCapture, "desktopCapture"},
    {APIPermission::kDesktopCapturePrivate, "desktopCapturePrivate"},
    {APIPermission::kDocumentScan, "documentScan"},
    {APIPermission::kDownloads, "downloads"},
    {APIPermission::kDownloadsOpen, "downloads.open"},
    {APIPermission::kDownloadsShelf, "downloads.shelf"},
    {APIPermission::kIdentity, "identity"},
    {APIPermission::kIdentityEmail, "identity.email"},
    {APIPermission::kExperimental, "experimental",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kGeolocation, "geolocation",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kNotifications, "notifications",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kGcm, "gcm",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // Register extension permissions.
    {APIPermission::kAccessibilityFeaturesModify,
     "accessibilityFeatures.modify",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAccessibilityFeaturesRead, "accessibilityFeatures.read"},
    {APIPermission::kAccessibilityPrivate, "accessibilityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kActiveTab, "activeTab"},
    {APIPermission::kBookmark, "bookmarks"},
    {APIPermission::kBrailleDisplayPrivate, "brailleDisplayPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kBrowsingData, "browsingData",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCertificateProvider, "certificateProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kContentSettings, "contentSettings"},
    {APIPermission::kContextMenus, "contextMenus",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCookie, "cookies"},
    {APIPermission::kCryptotokenPrivate, "cryptotokenPrivate"},
    {APIPermission::kDataReductionProxy, "dataReductionProxy",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kEnterpriseDeviceAttributes, "enterprise.deviceAttributes",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterpriseHardwarePlatform, "enterprise.hardwarePlatform",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterpriseNetworkingAttributes,
     "enterprise.networkingAttributes",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterprisePlatformKeys, "enterprise.platformKeys",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileBrowserHandler, "fileBrowserHandler",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFontSettings, "fontSettings",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kHistory, "history",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kIdltest, "idltest"},
    {APIPermission::kInput, "input"},
    {APIPermission::kManagement, "management"},
    {APIPermission::kMDns, "mdns",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPlatformKeys, "platformKeys",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrivacy, "privacy"},
    {APIPermission::kProcesses, "processes",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kSearch, "search",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSessions, "sessions"},
    {APIPermission::kSignedInDevices, "signedInDevices"},
    {APIPermission::kTab, "tabs",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kTopSites, "topSites",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kTransientBackground, "transientBackground",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kTts, "tts", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kTtsEngine, "ttsEngine",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWallpaper, "wallpaper",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebNavigation, "webNavigation",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},

    // Register private permissions.
    {APIPermission::kActivityLogPrivate, "activityLogPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kAutoTestPrivate, "autotestPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kCast, "cast", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kChromeosInfoPrivate, "chromeosInfoPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kCommandsAccessibility, "commands.accessibility",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kCommandLinePrivate, "commandLinePrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kDeveloperPrivate, "developerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kDownloadsInternal, "downloadsInternal"},
    {APIPermission::kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kFileManagerPrivate, "fileManagerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kIdentityPrivate, "identityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebcamPrivate, "webcamPrivate"},
    {APIPermission::kMediaPlayerPrivate, "mediaPlayerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kMediaRouterPrivate, "mediaRouterPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kNetworkingCastPrivate, "networking.castPrivate"},
    {APIPermission::kSystemPrivate, "systemPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kCloudPrintPrivate, "cloudPrintPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kInputMethodPrivate, "inputMethodPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kEchoPrivate, "echoPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kImageWriterPrivate, "imageWriterPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kRtcPrivate, "rtcPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kTerminalPrivate, "terminalPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kVirtualKeyboardPrivate, "virtualKeyboardPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWallpaperPrivate, "wallpaperPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebstorePrivate, "webstorePrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kEnterprisePlatformKeysPrivate,
     "enterprise.platformKeysPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kEnterpriseReportingPrivate, "enterprise.reportingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebrtcAudioPrivate, "webrtcAudioPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebrtcDesktopCapturePrivate, "webrtcDesktopCapturePrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebrtcLoggingPrivate, "webrtcLoggingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebrtcLoggingPrivateAudioDebug,
     "webrtcLoggingPrivate.audioDebug",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kSettingsPrivate, "settingsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kAutofillAssistantPrivate, "autofillAssistantPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kAutofillPrivate, "autofillPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kPasswordsPrivate, "passwordsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kUsersPrivate, "usersPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kLanguageSettingsPrivate, "languageSettingsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kResourcesPrivate, "resourcesPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kSafeBrowsingPrivate, "safeBrowsingPrivate"},

    // Full url access permissions.
    {APIPermission::kDebugger, "debugger",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kDevtools, "devtools",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal},
    {APIPermission::kPageCapture, "pageCapture",
     APIPermissionInfo::kFlagImpliesFullURLAccess},
    {APIPermission::kTabCapture, "tabCapture",
     APIPermissionInfo::kFlagImpliesFullURLAccess},
    {APIPermission::kTabCaptureForTab, "tabCaptureForTab",
     APIPermissionInfo::kFlagInternal},
    {APIPermission::kProxy, "proxy",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional},

    // Platform-app permissions.
    {APIPermission::kFileSystemProvider, "fileSystemProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLauncherSearchProvider, "launcherSearchProvider"},

    // Settings override permissions.
    {APIPermission::kHomepage, "homepage",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
    {APIPermission::kSearchProvider, "searchProvider",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
    {APIPermission::kStartupPages, "startupPages",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

base::span<const Alias> GetPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  static constexpr Alias aliases[] = {Alias("windows", "tabs")};

  return base::make_span(aliases);
}

}  // namespace chrome_api_permissions
}  // namespace extensions
