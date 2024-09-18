// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/new_window/chrome_new_window_delegate_provider.h"

#include <utility>

ChromeNewWindowDelegateProvider::ChromeNewWindowDelegateProvider(
    std::unique_ptr<ash::NewWindowDelegate> ash_new_window_delegate)
    : ash_new_window_delegate_(std::move(ash_new_window_delegate)) {}

ChromeNewWindowDelegateProvider::~ChromeNewWindowDelegateProvider() = default;

ash::NewWindowDelegate* ChromeNewWindowDelegateProvider::GetInstance() {
  return ash_new_window_delegate_.get();
}

ash::NewWindowDelegate* ChromeNewWindowDelegateProvider::GetPrimary() {
  return ash_new_window_delegate_.get();
}
