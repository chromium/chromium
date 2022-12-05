// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_LAUNCH_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_LAUNCH_UTIL_H_

#include "base/component_export.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace apps {

// Enumeration of possible app launch sources.
// This should be kept in sync with metadata/apps/histograms.xml,
// LaunchSource in enums.xml, as well as ApplicationLaunchSource in
// //components/services/app_service/public/protos/app_types.proto. Note the
// enumeration is used in UMA histogram so entries should not be re-ordered or
// removed. New entries should be added at the bottom.
enum class LaunchSource {
  kUnknown = 0,
  kFromAppListGrid = 1,              // Grid of apps, not the search box.
  kFromAppListGridContextMenu = 2,   // Grid of apps; context menu.
  kFromAppListQuery = 3,             // Query-dependent results (larger icons).
  kFromAppListQueryContextMenu = 4,  // Query-dependent results; context menu.
  kFromAppListRecommendation = 5,    // Query-less recommendations (smaller
                                     // icons).
  kFromParentalControls = 6,         // Parental Controls Settings Section and
                                     // Per App time notification.
  kFromShelf = 7,                    // Shelf.
  kFromFileManager = 8,              // FileManager.
  kFromLink = 9,                     // Left-clicking on links in the browser.
  kFromOmnibox = 10,                 // Enter URL in the Omnibox in the
                                     // browser.
  kFromChromeInternal = 11,          // Chrome internal call.
  kFromKeyboard = 12,                // Keyboard shortcut for opening app.
  kFromOtherApp = 13,                // Clicking link in another app or webui.
  kFromMenu = 14,                    // Menu.
  kFromInstalledNotification = 15,   // Installed notification
  kFromTest = 16,                    // Test
  kFromArc = 17,                     // Arc.
  kFromSharesheet = 18,              // Sharesheet.
  kFromReleaseNotesNotification = 19,  // Release Notes Notification.
  kFromFullRestore = 20,               // Full restore.
  kFromSmartTextContextMenu = 21,      // Smart text selection context menu.
  kFromDiscoverTabNotification = 22,   // Discover Tab Notification.
  kFromManagementApi = 23,             // Management API.
  kFromKiosk = 24,                     // Kiosk.
  kFromCommandLine = 25,               // Command line.
  kFromBackgroundMode = 26,            // Background mode.
  kFromNewTabPage = 27,                // New tab page.
  kFromIntentUrl = 28,                 // Intent URL.
  kFromOsLogin = 29,                   // Run on OS login.
  kFromProtocolHandler = 30,           // Protocol handler.
  kFromUrlHandler = 31,                // Url handler.
  kFromLockScreen = 32,                // Lock screen app launcher.

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kFromLockScreen,
};

// Don't remove items or change the order of this enum.  It's used in
// histograms and preferences.
enum class LaunchContainer {
  kLaunchContainerWindow = 0,
  kLaunchContainerPanelDeprecated = 1,
  kLaunchContainerTab = 2,
  // For platform apps, which don't actually have a container (they just get a
  // "onLaunched" event).
  kLaunchContainerNone = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kLaunchContainerNone,
};

// The window information to launch an app.
struct COMPONENT_EXPORT(APP_TYPES) WindowInfo {
  WindowInfo() = default;
  explicit WindowInfo(int64_t display_id);

  int32_t window_id = -1;
  int32_t state = 0;
  int64_t display_id = -1;
  absl::optional<gfx::Rect> bounds;
};

using WindowInfoPtr = std::unique_ptr<WindowInfo>;

COMPONENT_EXPORT(APP_TYPES)
ApplicationLaunchSource ConvertLaunchSourceToProtoApplicationLaunchSource(
    LaunchSource launch_source);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_LAUNCH_UTIL_H_
