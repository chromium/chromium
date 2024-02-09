// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_BOTTOMSHEET_PWA_BOTTOM_SHEET_CONTROLLER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_BOTTOMSHEET_PWA_BOTTOM_SHEET_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
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
  // If possible, shows/expand the PWA Bottom Sheet installer and returns true.
  // Otherwise does nothing and returns false.
  static bool MaybeShow(
      content::WebContents* web_contents,
      const WebAppBannerData& web_app_banner_data,
      bool expand_sheet,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          a2hs_event_callback,
      std::unique_ptr<AddToHomescreenParams> a2hs_params);

  PwaBottomSheetController(const PwaBottomSheetController&) = delete;
  PwaBottomSheetController& operator=(const PwaBottomSheetController&) = delete;
  virtual ~PwaBottomSheetController();

  // Called from the Java side and destructs this object.
  void Destroy(JNIEnv* env);

  // Called from the Java side when install source needs to be updated (e.g. if
  // the bottom sheet is created as an ambient badge, but then the user uses the
  // menu item to expand it, we will need to update the source from
  // AMBIENT_BADGE to MENU).
  void UpdateInstallSource(JNIEnv* env, int install_source);

  // Called from the Java side when bottom sheet got closed with swipe.
  void OnSheetClosedWithSwipe(JNIEnv* env);

  // Called from the Java side when bottom sheet got expanded.
  void OnSheetExpanded(JNIEnv* env);

  // Called from the Java side when the user opts to install.
  void OnAddToHomescreen(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);

 private:
  PwaBottomSheetController(
      const WebAppBannerData& web_app_banner_data,
      std::unique_ptr<AddToHomescreenParams> a2hs_params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          a2hs_event_callback);

  // Shows the Bottom Sheet installer UI for a given |web_contents|.
  void ShowBottomSheetInstaller(content::WebContents* web_contents,
                                bool expand_sheet);

  // Called for each screenshot available. Updates the Java side with the new
  // image.
  void UpdateScreenshot(const SkBitmap& screenshot,
                        content::WebContents* web_contents);

  const WebAppBannerData web_app_banner_data_;
  // Contains app parameters such as its type and the install source used that
  // will be passed to |a2hs_event_callback_| eventually.
  std::unique_ptr<AddToHomescreenParams> a2hs_params_;
  // Called to provide input into the state of the installation process.
  base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                               const AddToHomescreenParams&)>
      a2hs_event_callback_;
  // Whether the bottom sheet has been expanded.
  bool sheet_expanded_ = false;
  // Whether the bottom sheet has been closed.
  bool sheet_closed_ = false;
  // Whether the install flow was triggered.
  bool install_triggered_ = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_BOTTOMSHEET_PWA_BOTTOM_SHEET_CONTROLLER_H_
