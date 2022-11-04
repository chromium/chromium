// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"

namespace ash {

class LocaleSwitchView : public base::SupportsWeakPtr<LocaleSwitchView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"locale-switch",
                                                       "LocaleSwitchScreen"};

  LocaleSwitchView() = default;
  virtual ~LocaleSwitchView() = default;

  LocaleSwitchView(const LocaleSwitchView&) = delete;
  LocaleSwitchView& operator=(const LocaleSwitchView&) = delete;

  virtual void UpdateStrings() = 0;
};

// A class that updates localized strings in Oobe WebUI.
class LocaleSwitchScreenHandler : public BaseScreenHandler,
                                  public LocaleSwitchView {
 public:
  using TView = LocaleSwitchView;

  explicit LocaleSwitchScreenHandler(CoreOobeView* core_oobe_view);
  ~LocaleSwitchScreenHandler() override;

  // LocaleSwitchView:
  void UpdateStrings() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::raw_ptr<CoreOobeView> core_oobe_view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCALE_SWITCH_SCREEN_HANDLER_H_
