// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_HELPER_DELEGATE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_HELPER_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"

namespace chromeos {
namespace multidevice_setup {

// A delegate class used to install the Messages for Web PWA.
class AndroidSmsAppHelperDelegate {
 public:
  virtual ~AndroidSmsAppHelperDelegate() = default;

  // Sets up the Messages for Web PWA. Handles retries and errors internally.
  virtual void SetUpAndroidSmsApp() = 0;
  // Attempts to setup the Messages for Web PWA (if needed) and then launches it
  // if the installation succeeds. If installation fails, retries will continue,
  // but the app will not be launched if the first installation attempt failed.
  virtual void SetUpAndLaunchAndroidSmsApp() = 0;
  // Cleans up previously setup Messages for Web PWA. This does not uninstall
  // the PWA but only clears state that was setup for the PWA.
  virtual void TearDownAndroidSmsApp() = 0;
  // Returns true if the app was ever installed successfully since the feature
  // was enabled and then been manually uninstalled by the user.
  virtual bool HasAppBeenManuallyUninstalledByUser() = 0;

 protected:
  AndroidSmsAppHelperDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegate);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_HELPER_DELEGATE_H_
