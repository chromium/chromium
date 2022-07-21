// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"

using extensions::mojom::APIPermissionID;

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
    {APIPermissionID::kBackground, "background",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kDeclarativeContent, "declarativeContent"},
    {APIPermissionID::kDesktopCapture, "desktopCapture"},
    {APIPermissionID::kDesktopCapturePrivate, "desktopCapturePrivate"},
    {APIPermissionID::kDocumentScan, "documentScan"},
    {APIPermissionID::kDownloads, "downloads"},
    {APIPermissionID::kDownloadsOpen, "downloads.open"},
    {APIPermissionID::kDownloadsShelf, "downloads.shelf"},
    {APIPermissionID::kDownloadsUi, "downloads.ui"},
    {APIPermissionID::kIdentity, "identity"},
    {APIPermissionID::kIdentityEmail, "identity.email"},
    {APIPermissionID::kExperimental, "experimental",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kGeolocation, "geolocation",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kNotifications, "notifications",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kGcm, "gcm",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // Register extension permissions.
    {APIPermissionID::kAccessibilityFeaturesModify,
     "accessibilityFeatures.modify",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kAccessibilityFeaturesRead, "accessibilityFeatures.read"},
    {APIPermissionID::kAccessibilityPrivate, "accessibilityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kActiveTab, "activeTab"},
    {APIPermissionID::kBookmark, "bookmarks"},
    {APIPermissionID::kBrailleDisplayPrivate, "brailleDisplayPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kBrowsingData, "browsingData",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kCertificateProvider, "certificateProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kContentSettings, "contentSettings"},
    {APIPermissionID::kContextMenus, "contextMenus",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kCookie, "cookies"},
    {APIPermissionID::kCryptotokenPrivate, "cryptotokenPrivate"},
    {APIPermissionID::kEnterpriseDeviceAttributes,
     "enterprise.deviceAttributes",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kEnterpriseHardwarePlatform,
     "enterprise.hardwarePlatform",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kEnterpriseNetworkingAttributes,
     "enterprise.networkingAttributes",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kEnterprisePlatformKeys, "enterprise.platformKeys",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFileBrowserHandler, "fileBrowserHandler",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFontSettings, "fontSettings",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kHistory, "history",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kIdltest, "idltest"},
    {APIPermissionID::kInput, "input"},
    {APIPermissionID::kManagement, "management"},
    {APIPermissionID::kMDns, "mdns",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kPlatformKeys, "platformKeys",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kPrivacy, "privacy"},
    {APIPermissionID::kProcesses, "processes",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kSearch, "search",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFavicon, "favicon"},
    {APIPermissionID::kScripting, "scripting",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kSessions, "sessions"},
    {APIPermissionID::kSidePanel, "sidePanel"},
    {APIPermissionID::kTabGroups, "tabGroups",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kTab, "tabs",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kTopSites, "topSites",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kTransientBackground, "transientBackground",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kTts, "tts", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kTtsEngine, "ttsEngine",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWallpaper, "wallpaper",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kWebAuthenticationProxy, "webAuthenticationProxy",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebNavigation, "webNavigation",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},

    // Register private permissions.
    {APIPermissionID::kActivityLogPrivate, "activityLogPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kAutoTestPrivate, "autotestPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kChromeosInfoPrivate, "chromeosInfoPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kCommandsAccessibility, "commands.accessibility",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kCommandLinePrivate, "commandLinePrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kDeveloperPrivate, "developerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kDownloadsInternal, "downloadsInternal"},
    {APIPermissionID::kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kFileManagerPrivate, "fileManagerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kIdentityPrivate, "identityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebcamPrivate, "webcamPrivate"},
    {APIPermissionID::kMediaPlayerPrivate, "mediaPlayerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kMediaRouterPrivate, "mediaRouterPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kNetworkingCastPrivate, "networking.castPrivate"},
    {APIPermissionID::kSystemPrivate, "systemPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kInputMethodPrivate, "inputMethodPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kEchoPrivate, "echoPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kImageWriterPrivate, "imageWriterPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kRtcPrivate, "rtcPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kTerminalPrivate, "terminalPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kVirtualKeyboardPrivate, "virtualKeyboardPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebstorePrivate, "webstorePrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kEnterprisePlatformKeysPrivate,
     "enterprise.platformKeysPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kEnterpriseReportingPrivate,
     "enterprise.reportingPrivate", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebrtcAudioPrivate, "webrtcAudioPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebrtcDesktopCapturePrivate,
     "webrtcDesktopCapturePrivate", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebrtcLoggingPrivate, "webrtcLoggingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWebrtcLoggingPrivateAudioDebug,
     "webrtcLoggingPrivate.audioDebug",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kSettingsPrivate, "settingsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kAutofillPrivate, "autofillPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kPasswordsPrivate, "passwordsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kUsersPrivate, "usersPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kLanguageSettingsPrivate, "languageSettingsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kResourcesPrivate, "resourcesPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kSafeBrowsingPrivate, "safeBrowsingPrivate"},

    // Full url access permissions.
    {APIPermissionID::kDebugger, "debugger",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kDevtools, "devtools",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal},
    {APIPermissionID::kPageCapture, "pageCapture",
     APIPermissionInfo::kFlagImpliesFullURLAccess},
    {APIPermissionID::kTabCapture, "tabCapture",
     APIPermissionInfo::kFlagImpliesFullURLAccess},
    {APIPermissionID::kTabCaptureForTab, "tabCaptureForTab",
     APIPermissionInfo::kFlagInternal},
    {APIPermissionID::kProxy, "proxy",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional},

    // Platform-app permissions.
    {APIPermissionID::kFileSystemProvider, "fileSystemProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // Settings override permissions.
    {APIPermissionID::kHomepage, "homepage",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
    {APIPermissionID::kSearchProvider, "searchProvider",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
    {APIPermissionID::kStartupPages, "startupPages",
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
