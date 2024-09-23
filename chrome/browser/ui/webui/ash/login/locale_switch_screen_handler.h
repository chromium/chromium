// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"

namespace ash {

class LocaleSwitchView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"locale-switch",
                                                       "LocaleSwitchScreen"};

  LocaleSwitchView() = default;
  virtual ~LocaleSwitchView() = default;

  LocaleSwitchView(const LocaleSwitchView&) = delete;
  LocaleSwitchView& operator=(const LocaleSwitchView&) = delete;

  virtual void UpdateStrings() = 0;
  virtual base::WeakPtr<LocaleSwitchView> AsWeakPtr() = 0;
};

// A class that updates localized strings in Oobe WebUI.
class LocaleSwitchScreenHandler final : public BaseScreenHandler,
                                        public LocaleSwitchView {
 public:
  using TView = LocaleSwitchView;

  LocaleSwitchScreenHandler();
  ~LocaleSwitchScreenHandler() override;

  // LocaleSwitchView:
  void UpdateStrings() override;
  base::WeakPtr<LocaleSwitchView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<LocaleSwitchView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_
