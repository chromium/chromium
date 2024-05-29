// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_coordinator.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_mediator.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/AddToHomescreenCoordinator_jni.h"

namespace webapps {

// static
bool AddToHomescreenCoordinator::ShowForAppBanner(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        event_callback) {
  // Don't start if app info is not available.
  if ((params->app_type == AddToHomescreenParams::AppType::NATIVE &&
       params->native_app_data.is_null()) ||
      (params->app_type == AddToHomescreenParams::AppType::WEBAPK &&
       !params->shortcut_info)) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  AddToHomescreenMediator* mediator = (AddToHomescreenMediator*)
      Java_AddToHomescreenCoordinator_initMvcAndReturnMediator(
          env, weak_manager->web_contents()->GetJavaWebContents());
  if (!mediator)
    return false;

  mediator->StartForAppBanner(weak_manager, std::move(params),
                              std::move(event_callback));
  return true;
}

}  // namespace webapps
