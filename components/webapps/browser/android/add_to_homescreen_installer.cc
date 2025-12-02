// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_installer.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/AddToHomescreenInstaller_jni.h"

namespace webapps {

// static
void AddToHomescreenInstaller::Install(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params,
    const AddToHomescreenEventCallback& event_callback) {
  if (!web_contents) {
    event_callback.Run(AddToHomescreenEvent::INSTALL_FAILED, params);
    return;
  }

  event_callback.Run(AddToHomescreenEvent::INSTALL_STARTED, params);
  switch (params.app_type) {
    case AddToHomescreenParams::AppType::NATIVE:
      InstallOrOpenNativeApp(web_contents, params, event_callback);
      break;
    case AddToHomescreenParams::AppType::WEBAPK:
    case AddToHomescreenParams::AppType::WEBAPK_DIY:
      WebappsClient::Get()->InstallWebApk(web_contents, params);
      break;
    case AddToHomescreenParams::AppType::SHORTCUT:
      WebappsClient::Get()->InstallShortcut(web_contents, params);
      break;
    case AddToHomescreenParams::AppType::TWA:
      // Auto-minted TWAs will be handled separately, as the install UX is shown
      // by the WebApp service on the Android side.
      NOTREACHED();
  }
  event_callback.Run(AddToHomescreenEvent::INSTALL_REQUEST_FINISHED, params);
}

// static
void AddToHomescreenInstaller::InstallOrOpenNativeApp(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params,
    const AddToHomescreenEventCallback& event_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  bool was_successful = Java_AddToHomescreenInstaller_installOrOpenNativeApp(
      env, web_contents->GetJavaWebContents(), params.native_app_data);
  event_callback.Run(
      was_successful ? AddToHomescreenEvent::NATIVE_INSTALL_OR_OPEN_SUCCEEDED
                     : AddToHomescreenEvent::NATIVE_INSTALL_OR_OPEN_FAILED,
      params);
}

}  // namespace webapps

DEFINE_JNI(AddToHomescreenInstaller)
