// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_AUTOLAUNCH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_AUTOLAUNCH_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// Interface between reset screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class KioskAutolaunchScreenView
    : public base::SupportsWeakPtr<KioskAutolaunchScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"autolaunch",
                                                       "AutolaunchScreen"};

  virtual ~KioskAutolaunchScreenView() = default;

  virtual void Show() = 0;
  virtual void HandleOnCancel() = 0;
  virtual void HandleOnConfirm() = 0;
  virtual void HandleOnVisible() = 0;
};

// WebUI implementation of KioskAutolaunchScreenActor.
class KioskAutolaunchScreenHandler : public KioskAutolaunchScreenView,
                                     public KioskAppManagerObserver,
                                     public BaseScreenHandler {
 public:
  using TView = KioskAutolaunchScreenView;

  KioskAutolaunchScreenHandler();

  KioskAutolaunchScreenHandler(const KioskAutolaunchScreenHandler&) = delete;
  KioskAutolaunchScreenHandler& operator=(const KioskAutolaunchScreenHandler&) =
      delete;

  ~KioskAutolaunchScreenHandler() override;

  // KioskAutolaunchScreenView:
  void Show() override;
  void HandleOnCancel() override;
  void HandleOnConfirm() override;
  void HandleOnVisible() override;

  // KioskAppManagerObserver:
  void OnKioskAppsSettingsChanged() override;
  void OnKioskAppDataChanged(const std::string& app_id) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  void DeclareJSCallbacks() override;

 private:
  // Updates auto-start UI assets on JS side.
  void UpdateKioskApp();

  bool is_visible_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::KioskAutolaunchScreenHandler;
using ::chromeos::KioskAutolaunchScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_AUTOLAUNCH_SCREEN_HANDLER_H_
