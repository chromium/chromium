// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface of the demo mode preferences screen view.
class DemoPreferencesScreenView
    : public base::SupportsWeakPtr<DemoPreferencesScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"demo-preferences",
                                                       "DemoPreferencesScreen"};

  virtual ~DemoPreferencesScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;
};

// WebUI implementation of DemoPreferencesScreenView.
class DemoPreferencesScreenHandler : public BaseScreenHandler,
                                     public DemoPreferencesScreenView {
 public:
  using TView = DemoPreferencesScreenView;

  DemoPreferencesScreenHandler();

  DemoPreferencesScreenHandler(const DemoPreferencesScreenHandler&) = delete;
  DemoPreferencesScreenHandler& operator=(const DemoPreferencesScreenHandler&) =
      delete;

  ~DemoPreferencesScreenHandler() override;

  // DemoPreferencesScreenView:
  void Show() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEMO_PREFERENCES_SCREEN_HANDLER_H_
