// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class AppDownloadingScreen;

class AppDownloadingScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"app-downloading"};

  virtual ~AppDownloadingScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(AppDownloadingScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;
};

// The sole implementation of the AppDownloadingScreenView, using WebUI.
class AppDownloadingScreenHandler : public BaseScreenHandler,
                                    public AppDownloadingScreenView {
 public:
  using TView = AppDownloadingScreenView;

  explicit AppDownloadingScreenHandler(JSCallsContainer* js_calls_container);
  ~AppDownloadingScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // AppDownloadingScreenView:
  void Bind(AppDownloadingScreen* screen) override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  AppDownloadingScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppDownloadingScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_DOWNLOADING_SCREEN_HANDLER_H_
