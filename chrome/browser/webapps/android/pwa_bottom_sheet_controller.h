// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_ANDROID_PWA_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_WEBAPPS_ANDROID_PWA_BOTTOM_SHEET_CONTROLLER_H_

#include <map>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/android/installable/installable_ambient_badge_infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {

// A Controller for the BottomSheet install UI for progressive web apps.
// If successfully created, the lifetime of this object is tied to the lifetime
// of the BottomSheet UI being shown and the object is destroyed from Java when
// the UI is dismissed. This class can be instantiated from both the Java side
// (when the user selects Install App from the App Menu) and from the C++ side,
// when the engagement score for the web site is high enough to promote the
// install of a PWA.
class PwaBottomSheetController {
 public:
  // If possible, shows the PWA Bottom Sheet installer. Otherwise it attempts to
  // delegate to the install infobar UI.
  static void MaybeCreateAndShow(
      base::WeakPtr<InstallableAmbientBadgeInfoBarDelegate::Client> weak_client,
      content::WebContents* web_contents,
      const base::string16& app_name,
      const SkBitmap& primary_icon,
      const bool is_primary_icon_maskable,
      const GURL& start_url,
      const std::map<GURL, SkBitmap>& screenshots,
      const base::string16& description,
      const std::vector<base::string16>& categories,
      bool show_expanded);

  virtual ~PwaBottomSheetController();

  // Called from the Java side and destructs this object.
  void Destroy(JNIEnv* env);

  // Called from the Java side when the user opts to install.
  void OnAddToHomescreen(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);

 private:
  PwaBottomSheetController(const base::string16& app_name,
                           const SkBitmap& primary_icon,
                           const bool is_primary_icon_maskable,
                           const GURL& start_url,
                           const std::map<GURL, SkBitmap>& screenshots,
                           const base::string16& description,
                           const std::vector<base::string16>& categories,
                           bool show_expanded);
  PwaBottomSheetController(const PwaBottomSheetController&) = delete;
  PwaBottomSheetController& operator=(const PwaBottomSheetController&) = delete;

  // Shows the Bottom Sheet installer UI for a given |web_contents|.
  void ShowBottomSheetInstaller(content::WebContents* web_contents);

  // Called for each screenshot available. Updates the Java side with the new
  // image.
  void UpdateScreenshot(const SkBitmap& screenshot,
                        content::WebContents* web_contents);

  const base::string16 app_name_;
  const SkBitmap primary_icon_;
  const bool is_primary_icon_maskable_;
  const GURL& start_url_;
  const std::map<GURL, SkBitmap>& screenshots_;
  const base::string16 description_;
  const std::vector<base::string16>& categories_;
  bool show_expanded_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEBAPPS_ANDROID_PWA_BOTTOM_SHEET_CONTROLLER_H_
