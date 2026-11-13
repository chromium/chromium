// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_TWA_INSTALLER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_TWA_INSTALLER_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"

namespace webapps {

// Helper class for installing a web app as auto-minted TWA by calling the
// API of Android-side WebApp service.
//
// `Install()` passes ownership to the Java TwaInstaller through
// `jni_zero::JniUniquePtr`. Java destroys this object in `onFlowCompleted()`
// after it reports the last event.
class TwaInstaller {
 public:
  static bool Install(std::unique_ptr<AddToHomescreenParams> params,
                      const AddToHomescreenEventCallback& event_callback);

  ~TwaInstaller();

  // Called from the Java side to run `event_callback_`. Must only be called
  // before Java calls `destroy()` on its `JniUniquePtr`.
  void OnInstallEvent(JNIEnv* env, int event);

  TwaInstaller(const TwaInstaller&) = delete;
  TwaInstaller& operator=(const TwaInstaller&) = delete;

 private:
  TwaInstaller(std::unique_ptr<AddToHomescreenParams> params,
               AddToHomescreenEventCallback event_callback);

  std::unique_ptr<AddToHomescreenParams> params_;
  AddToHomescreenEventCallback event_callback_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_TWA_INSTALLER_H_
