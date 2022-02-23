// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SMART_PRIVACY_PROTECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SMART_PRIVACY_PROTECTION_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class SmartPrivacyProtectionScreen;
}

namespace chromeos {

// Interface between SmartPrivacyProtection screen and its representation,
// either WebUI or Views one. Note, do not forget to call OnViewDestroyed in the
// dtor.
class SmartPrivacyProtectionView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"smart-privacy-protection"};

  virtual ~SmartPrivacyProtectionView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Bind(ash::SmartPrivacyProtectionScreen* screen) = 0;
  virtual void Unbind() = 0;
};

// WebUI implementation of SmartPrivacyProtectionView. It is used to interact
// with the SmartPrivacyProtection part of the JS page.
class SmartPrivacyProtectionScreenHandler : public SmartPrivacyProtectionView,
                                            public BaseScreenHandler {
 public:
  using TView = SmartPrivacyProtectionView;

  explicit SmartPrivacyProtectionScreenHandler(
      JSCallsContainer* js_calls_container);

  SmartPrivacyProtectionScreenHandler(
      const SmartPrivacyProtectionScreenHandler&) = delete;
  SmartPrivacyProtectionScreenHandler& operator=(
      const SmartPrivacyProtectionScreenHandler&) = delete;

  ~SmartPrivacyProtectionScreenHandler() override;

  // SmartPrivacyProtectionView implementation:
  void Show() override;
  void Hide() override;
  void Bind(ash::SmartPrivacyProtectionScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

 private:
  ash::SmartPrivacyProtectionScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SmartPrivacyProtectionScreenHandler;
using ::chromeos::SmartPrivacyProtectionView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SMART_PRIVACY_PROTECTION_SCREEN_HANDLER_H_
