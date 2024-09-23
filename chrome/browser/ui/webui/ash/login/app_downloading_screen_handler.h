// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class AppDownloadingScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"app-downloading",
                                                       "AppDownloadingScreen"};

  virtual ~AppDownloadingScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<AppDownloadingScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the AppDownloadingScreenView, using WebUI.
class AppDownloadingScreenHandler final : public BaseScreenHandler,
                                          public AppDownloadingScreenView {
 public:
  using TView = AppDownloadingScreenView;

  AppDownloadingScreenHandler();

  AppDownloadingScreenHandler(const AppDownloadingScreenHandler&) = delete;
  AppDownloadingScreenHandler& operator=(const AppDownloadingScreenHandler&) =
      delete;

  ~AppDownloadingScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // AppDownloadingScreenView:
  void Show() override;
  base::WeakPtr<AppDownloadingScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<AppDownloadingScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_
