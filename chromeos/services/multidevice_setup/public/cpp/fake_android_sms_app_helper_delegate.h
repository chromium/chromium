// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"

namespace chromeos {
namespace multidevice_setup {

class FakeAndroidSmsAppHelperDelegate : public AndroidSmsAppHelperDelegate {
 public:
  FakeAndroidSmsAppHelperDelegate();
  ~FakeAndroidSmsAppHelperDelegate() override;
  bool HasInstalledApp();
  bool HasLaunchedApp();
  void Reset();

  // AndroidSmsAppHelperDelegate:
  void InstallAndroidSmsApp() override;
  void InstallAndLaunchAndroidSmsApp() override;

 private:
  bool has_installed_ = false;
  bool has_launched_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsAppHelperDelegate);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_HELPER_DELEGATE_H_
