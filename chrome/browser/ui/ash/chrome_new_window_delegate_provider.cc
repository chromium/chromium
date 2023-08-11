// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_delegate_provider.h"

#include <utility>

#include "chrome/browser/ash/crosapi/browser_util.h"

ChromeNewWindowDelegateProvider::ChromeNewWindowDelegateProvider(
    std::unique_ptr<ash::NewWindowDelegate> ash_new_window_delegate,
    std::unique_ptr<ash::NewWindowDelegate> crosapi_new_window_delegate)
    : ash_new_window_delegate_(std::move(ash_new_window_delegate)),
      crosapi_new_window_delegate_(std::move(crosapi_new_window_delegate)) {}

ChromeNewWindowDelegateProvider::~ChromeNewWindowDelegateProvider() = default;

ash::NewWindowDelegate* ChromeNewWindowDelegateProvider::GetInstance() {
  return ash_new_window_delegate_.get();
}

ash::NewWindowDelegate* ChromeNewWindowDelegateProvider::GetPrimary() {
  if (crosapi::browser_util::IsLacrosEnabled()) {
    return crosapi_new_window_delegate_.get();
  }
  return ash_new_window_delegate_.get();
}
