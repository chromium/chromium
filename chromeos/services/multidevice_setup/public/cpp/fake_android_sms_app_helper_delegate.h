// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"

namespace chromeos {

namespace multidevice_setup {

class FakeAndroidSmsAppHelperDelegate
    : virtual public AndroidSmsAppHelperDelegate {
 public:
  FakeAndroidSmsAppHelperDelegate();
  ~FakeAndroidSmsAppHelperDelegate() override;

  bool has_installed_app() const { return has_installed_app_; }
  bool has_launched_app() const { return has_launched_app_; }
  bool is_default_to_persist_cookie_set() const {
    return is_default_to_persist_cookie_set_;
  }

  void set_has_app_been_manually_uninstalled(
      bool has_app_been_manually_uninstalled) {
    has_app_been_manually_uninstalled_ = has_app_been_manually_uninstalled;
  }

  // Sets all booleans representing recorded actions to false.
  void Reset();

 private:
  // AndroidSmsAppHelperDelegate:
  void SetUpAndroidSmsApp() override;
  void SetUpAndLaunchAndroidSmsApp() override;
  void TearDownAndroidSmsApp() override;
  bool HasAppBeenManuallyUninstalledByUser() override;

  bool has_installed_app_ = false;
  bool has_launched_app_ = false;
  bool is_default_to_persist_cookie_set_ = false;
  bool has_app_been_manually_uninstalled_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsAppHelperDelegate);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_
