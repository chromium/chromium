// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_

#include "chromeos/ash/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"

namespace ash {

namespace multidevice_setup {

class FakeAndroidSmsAppHelperDelegate
    : virtual public AndroidSmsAppHelperDelegate {
 public:
  FakeAndroidSmsAppHelperDelegate();

  FakeAndroidSmsAppHelperDelegate(const FakeAndroidSmsAppHelperDelegate&) =
      delete;
  FakeAndroidSmsAppHelperDelegate& operator=(
      const FakeAndroidSmsAppHelperDelegate&) = delete;

  ~FakeAndroidSmsAppHelperDelegate() override;

  bool has_installed_app() const { return has_installed_app_; }
  void set_has_installed_app(bool has_installed_app) {
    has_installed_app_ = has_installed_app;
  }

  bool has_launched_app() const { return has_launched_app_; }
  bool is_default_to_persist_cookie_set() const {
    return is_default_to_persist_cookie_set_;
  }

  void set_is_app_registry_ready(bool is_app_registry_ready) {
    is_app_registry_ready_ = is_app_registry_ready;
  }

  // Sets all booleans representing recorded actions to false.
  void Reset();

 private:
  // AndroidSmsAppHelperDelegate:
  void SetUpAndroidSmsApp() override;
  void SetUpAndLaunchAndroidSmsApp() override;
  void TearDownAndroidSmsApp() override;
  bool IsAppInstalled() override;
  bool IsAppRegistryReady() override;
  void ExecuteOnAppRegistryReady(base::OnceClosure task) override;

  bool has_installed_app_ = false;
  bool has_launched_app_ = false;
  bool is_default_to_persist_cookie_set_ = false;
  bool is_app_registry_ready_ = false;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_
