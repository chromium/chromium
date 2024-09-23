// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_ICON_UTILS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_ICON_UTILS_H_

#include "base/android/scoped_java_ref.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webapk/webapk.pb.h"

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

  // Returns the ideal size for a WebAPK icon of specific type
  static int GetIdealIconSizeForIconType(webapk::Image::Usage usage,
                                         webapk::Image::Purpose purpose);

  // Returns if the Android version supports Adaptive Icon (i.e. API level >=
  // 26)
  static bool DoesAndroidSupportMaskableIcons();

  // Finalize the launcher icon from |icon|. |start_url| is used to generate the
  // icon if |icon| is empty or is not large enough. When complete, posts
  // |callback| on |ui_thread_task_runner| binding:
  // - the generated icon
  // - whether |icon| was used in generating the launcher icon
  static void FinalizeLauncherIconInBackground(
      const SkBitmap& bitmap,
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
      base::OnceCallback<void(const SkBitmap&, bool)> callback);

  // Generates a home screen icon for the web page at `page_url`. The icon is
  // a single letter on a grey background.
  static SkBitmap GenerateHomeScreenIconInBackground(const GURL& page_url);

  // Generate an adaptive icon for given maskable icon bitmap.
  static SkBitmap GenerateAdaptiveIconBitmap(const SkBitmap& icon);

  static int GetIdealIconCornerRadiusPxForPromptUI();

  static void SetIdealShortcutSizeForTesting(int size);
  static void SetIconSizesForTesting(std::vector<int> sizes);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_ICON_UTILS_H_
