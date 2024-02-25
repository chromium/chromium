// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"

#include "url/gurl.h"

namespace ash {

namespace multidevice_setup {

FakeAndroidSmsAppHelperDelegate::FakeAndroidSmsAppHelperDelegate() {}

FakeAndroidSmsAppHelperDelegate::~FakeAndroidSmsAppHelperDelegate() = default;

void FakeAndroidSmsAppHelperDelegate::SetUpAndroidSmsApp() {
  // TODO(jlklein): Add error simulation when error codes are added to the API.
  has_installed_app_ = true;
  is_default_to_persist_cookie_set_ = true;
}

void FakeAndroidSmsAppHelperDelegate::SetUpAndLaunchAndroidSmsApp() {
  SetUpAndroidSmsApp();
  has_launched_app_ = true;
}

void FakeAndroidSmsAppHelperDelegate::Reset() {
  has_installed_app_ = false;
  has_launched_app_ = false;
  is_default_to_persist_cookie_set_ = false;
}

void FakeAndroidSmsAppHelperDelegate::TearDownAndroidSmsApp() {
  is_default_to_persist_cookie_set_ = false;
}

bool FakeAndroidSmsAppHelperDelegate::IsAppInstalled() {
  return has_installed_app_;
}

bool FakeAndroidSmsAppHelperDelegate::IsAppRegistryReady() {
  return is_app_registry_ready_;
}

void FakeAndroidSmsAppHelperDelegate::ExecuteOnAppRegistryReady(
    base::OnceClosure task) {
  std::move(task).Run();
}

}  // namespace multidevice_setup

}  // namespace ash
