// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_ICON_UTILS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_ICON_UTILS_H_

#include "base/android/scoped_java_ref.h"

class SkBitmap;
class GURL;

namespace webapps {

// Contains utilities that query Java code for information about web
// app/shortcut icons.
class WebappsIconUtils {
 public:
  WebappsIconUtils() = delete;
  WebappsIconUtils& operator=(const WebappsIconUtils&) = delete;
  WebappsIconUtils(const WebappsIconUtils&) = delete;

  // Returns the ideal size for an icon representing a web app or a WebAPK.
  static int GetIdealHomescreenIconSizeInPx();

  // Returns the minimum size for an icon representing a web app or a WebAPK.
  static int GetMinimumHomescreenIconSizeInPx();

  // Returns the ideal size for an image displayed on a web app's splash
  // screen.
  static int GetIdealSplashImageSizeInPx();

  // Returns the minimum size for an image displayed on a web app's splash
  // screen.
  static int GetMinimumSplashImageSizeInPx();

  // Returns the ideal size for an adaptive launcher icon of a WebAPK
  static int GetIdealAdaptiveLauncherIconSizeInPx();

  // Returns the ideal size for a shortcut icon of a WebAPK.
  static int GetIdealShortcutIconSizeInPx();

  // Returns if the Android version supports Adaptive Icon (i.e. API level >=
  // 26)
  static bool DoesAndroidSupportMaskableIcons();

  // Returns the given icon, modified to match the launcher requirements.
  // This method may generate an entirely new icon; if this is the case,
  // |is_generated| will be set to |true|.
  // Must be called on a background worker thread.
  static SkBitmap FinalizeLauncherIconInBackground(const SkBitmap& icon,
                                                   bool is_icon_maskable,
                                                   const GURL& url,
                                                   bool* is_generated);

  // Generate an adaptive icon for given maskable icon bitmap.
  static SkBitmap GenerateAdaptiveIconBitmap(const SkBitmap& icon);

  static int GetIdealIconCornerRadiusPxForPromptUI();

  static void SetIdealShortcutSizeForTesting(int size);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_ICON_UTILS_H_
