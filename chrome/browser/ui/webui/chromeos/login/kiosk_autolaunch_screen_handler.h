// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_AUTOLAUNCH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_AUTOLAUNCH_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

class KioskAutolaunchScreen;

// Interface between reset screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class KioskAutolaunchScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"autolaunch"};

  virtual ~KioskAutolaunchScreenView() {}

  virtual void Show() = 0;
  virtual void SetDelegate(KioskAutolaunchScreen* delegate) = 0;
};

// WebUI implementation of KioskAutolaunchScreenActor.
class KioskAutolaunchScreenHandler : public KioskAutolaunchScreenView,
                                     public KioskAppManagerObserver,
                                     public BaseScreenHandler {
 public:
  using TView = KioskAutolaunchScreenView;

  explicit KioskAutolaunchScreenHandler(JSCallsContainer* js_calls_container);
  ~KioskAutolaunchScreenHandler() override;

  // KioskAutolaunchScreenView:
  void Show() override;
  void SetDelegate(KioskAutolaunchScreen* delegate) override;

  // KioskAppManagerObserver:
  void OnKioskAppsSettingsChanged() override;
  void OnKioskAppDataChanged(const std::string& app_id) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Updates auto-start UI assets on JS side.
  void UpdateKioskApp();

  // JS messages handlers.
  void HandleOnCancel();
  void HandleOnConfirm();
  void HandleOnVisible();

  KioskAutolaunchScreen* delegate_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
  bool is_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(KioskAutolaunchScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_AUTOLAUNCH_SCREEN_HANDLER_H_
