// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_MEDIATOR_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_MEDIATOR_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {

class AddToHomescreenInstaller;

using AppType = AddToHomescreenParams::AppType;

// AddToHomescreenMediator is the C++ counterpart of
// org.chromium.components.webapps.addtohomescreen.AddToHomescreenMediator
// in Java. It uses AddToHomescreenInstaller for installing the current app.
// This class is owned, constructed, and destroyed by its Java counter-part.
class AddToHomescreenMediator {
 public:
  // Initializes the mediator for a given reference to the Java side object and
  // for a given WebContent.
  // After initialization, SetWebAppInfo and SetIcon should be called to update
  // the UI accordingly.
  AddToHomescreenMediator(
      const base::android::JavaParamRef<jobject>& java_ref,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  void StartForAppBanner(
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  // These 2 methods are called from the coordinator when the current flow
  // started with startForAppMenu.
  void OnAppMetadataAvailable(const std::u16string& user_title,
                              const GURL& url,
                              AddToHomescreenParams::AppType app_type);
  void OnFullAppDataAvailable(std::unique_ptr<AddToHomescreenParams> params);

  // Called from the Java side when the user accepts app installation from the
  // dialog.
  void AddToHomescreen(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_user_title);

  // Called from the Java side when the installation UI is dismissed.
  void OnUiDismissed(JNIEnv* env);

  // Called from the Java side when details for a native app are shown.
  void OnNativeDetailsShown(JNIEnv* env);

  // Called from the Java side and destructs this object.
  void Destroy(JNIEnv* env);

 private:
  ~AddToHomescreenMediator();

  // Called immediatedly after |params_| is available. Displays |display_icon|
  // in the installation UI.
  void SetIcon(const SkBitmap& display_icon);

  // Sends the Web App info to the Java side.
  void SetWebAppInfo(const std::u16string& user_title,
                     const GURL& url,
                     AddToHomescreenParams::AppType app_type);

  void RecordEventForAppMenu(AddToHomescreenInstaller::Event event,
                             const AddToHomescreenParams& a2hs_params);

  // Points to the Java reference.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  base::WeakPtr<content::WebContents> web_contents_;

  std::unique_ptr<AddToHomescreenParams> params_;

  base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                               const AddToHomescreenParams&)>
      event_callback_;

  AddToHomescreenMediator(const AddToHomescreenMediator&) = delete;
  AddToHomescreenMediator& operator=(const AddToHomescreenMediator&) = delete;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_MEDIATOR_H_
