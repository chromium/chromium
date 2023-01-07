// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_PROVIDER_H_

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"

// Provides Chrome's NewWindowDelegate implementation.
// Specifically, this handles when Lacros is used as the primary web browser.
class ChromeNewWindowDelegateProvider : public ash::NewWindowDelegateProvider {
 public:
  ChromeNewWindowDelegateProvider(
      std::unique_ptr<ash::NewWindowDelegate> ash_new_window_delegate,
      std::unique_ptr<ash::NewWindowDelegate> crosapi_new_window_delegate);
  ChromeNewWindowDelegateProvider(const ChromeNewWindowDelegateProvider&) =
      delete;
  ChromeNewWindowDelegateProvider& operator=(
      const ChromeNewWindowDelegateProvider&) = delete;
  ~ChromeNewWindowDelegateProvider() override;

  // ash::NewWindowDelegateProvider:
  ash::NewWindowDelegate* GetInstance() override;
  ash::NewWindowDelegate* GetPrimary() override;

 private:
  std::unique_ptr<ash::NewWindowDelegate> ash_new_window_delegate_;
  std::unique_ptr<ash::NewWindowDelegate> crosapi_new_window_delegate_;
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_PROVIDER_H_
