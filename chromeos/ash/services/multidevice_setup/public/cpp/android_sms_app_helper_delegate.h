// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_HELPER_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_HELPER_DELEGATE_H_

#include "base/functional/callback.h"

namespace ash {
namespace multidevice_setup {

// A delegate class used to install the Messages for Web PWA.
class AndroidSmsAppHelperDelegate {
 public:
  AndroidSmsAppHelperDelegate(const AndroidSmsAppHelperDelegate&) = delete;
  AndroidSmsAppHelperDelegate& operator=(const AndroidSmsAppHelperDelegate&) =
      delete;

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
  // Returns whether the PWA is currently installed.
  virtual bool IsAppInstalled() = 0;
  // Returns true when details about installed PWAs is available to query.
  virtual bool IsAppRegistryReady() = 0;
  // Takes a task to run when the app registry is available.  If already
  // available it will execute asynchronously.
  virtual void ExecuteOnAppRegistryReady(base::OnceClosure task) = 0;

 protected:
  AndroidSmsAppHelperDelegate() = default;
};

}  // namespace multidevice_setup
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_HELPER_DELEGATE_H_
