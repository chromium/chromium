// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_MEDIATOR_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_MEDIATOR_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {

struct ShortcutInfo;
class AddToHomescreenInstaller;

using AppType = AddToHomescreenParams::AppType;

// AddToHomescreenMediator is the C++ counterpart of
// org.chromium.components.webapps.addtohomescreen.AddToHomescreenMediator
// in Java. It uses AddToHomescreenInstaller for installing the current app.
// This class is owned, constructed, and destroyed by its Java counter-part.
class AddToHomescreenMediator : public AddToHomescreenDataFetcher::Observer {
 public:
  // Initializes the mediator for a given reference to the Java side object.
  // After initialization, either StartForAppBanner or StartForAppMenu should be
  // called to update the UI accordingly.
  explicit AddToHomescreenMediator(
      const base::android::JavaParamRef<jobject>& java_ref);

  void StartForAppBanner(
      base::WeakPtr<AppBannerManager> weak_manager,
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  void StartForAppMenu(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_web_contents,
      int title_id);

  // Called from the Java side when the user accepts app installation from the
  // dialog.
  void AddToHomescreen(JNIEnv* env,
                       const base::android::JavaParamRef<jstring>& j_user_title,
                       jint j_app_type);

  // Called from the Java side when the installation UI is dismissed.
  void OnUiDismissed(JNIEnv* env);

  // Called from the Java side when details for a native app are shown.
  void OnNativeDetailsShown(JNIEnv* env);

  // Called from the Java side and destructs this object.
  void Destroy(JNIEnv* env);

 private:
  ~AddToHomescreenMediator() override;

  // Called immediatedly after |params_| is available. Displays |display_icon|
  // in the installation UI.
  void SetIcon(const SkBitmap& display_icon);

  // Sends the Web App info to the Java side.
  void SetWebAppInfo(const std::u16string& user_title,
                     const GURL& url,
                     AddToHomescreenParams::AppType app_type);

  // AddToHomescreenDataFetcher::Observer:
  void OnUserTitleAvailable(const std::u16string& user_title,
                            const GURL& url,
                            AddToHomescreenParams::AppType app_type) override;

  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& display_icon,
                       AddToHomescreenParams::AppType app_type,
                       InstallableStatusCode status_code) override;

  void RecordEventForAppMenu(AddToHomescreenInstaller::Event event,
                             const AddToHomescreenParams& a2hs_params);

  content::WebContents* GetWebContents();

  // Points to the Java reference.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  base::WeakPtr<AppBannerManager> weak_app_banner_manager_;

  // Fetches data required to add a shortcut.
  std::unique_ptr<AddToHomescreenDataFetcher> data_fetcher_;

  std::unique_ptr<AddToHomescreenParams> params_;

  base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                               const AddToHomescreenParams&)>
      event_callback_;

  int app_menu_type_ = AppBannerSettingsHelper::APP_MENU_OPTION_UNKNOWN;

  AddToHomescreenMediator(const AddToHomescreenMediator&) = delete;
  AddToHomescreenMediator& operator=(const AddToHomescreenMediator&) = delete;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_MEDIATOR_H_
