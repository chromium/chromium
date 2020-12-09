// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"

namespace chromeos {

class LocaleSwitchScreen;

class LocaleSwitchView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"locale-switch"};

  LocaleSwitchView() = default;
  virtual ~LocaleSwitchView() = default;

  LocaleSwitchView(const LocaleSwitchView&) = delete;
  LocaleSwitchView& operator=(const LocaleSwitchView&) = delete;

  virtual void Bind(LocaleSwitchScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void UpdateStrings() = 0;
};

// A class that updates localized strings in Oobe WebUI.
class LocaleSwitchScreenHandler : public BaseScreenHandler,
                                  public LocaleSwitchView {
 public:
  using TView = LocaleSwitchView;

  LocaleSwitchScreenHandler(JSCallsContainer* js_calls_container,
                            CoreOobeView* core_oobe_view);
  ~LocaleSwitchScreenHandler() override;

  // LocaleSwitchView:
  void Bind(LocaleSwitchScreen* screen) override;
  void Unbind() override;
  void UpdateStrings() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  LocaleSwitchScreen* screen_ = nullptr;
  CoreOobeView* core_oobe_view_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_
