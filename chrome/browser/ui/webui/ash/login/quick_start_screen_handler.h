// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/verification_shapes.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}

namespace ash {

class QuickStartView : public base::SupportsWeakPtr<QuickStartView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"quick-start",
                                                       "QuickStartScreen"};

  virtual ~QuickStartView() = default;

  virtual void Show() = 0;
  virtual void SetShapes(const quick_start::ShapeList& shape_list) = 0;
  virtual void SetQRCode(base::Value::List blob) = 0;
  virtual void ShowConnectingToWifi() = 0;
  virtual void ShowConnectedToWifi(std::string ssid, std::string password) = 0;
};

// WebUI implementation of QuickStartView.
class QuickStartScreenHandler : public QuickStartView,
                                public BaseScreenHandler {
 public:
  using TView = QuickStartView;

  QuickStartScreenHandler();

  QuickStartScreenHandler(const QuickStartScreenHandler&) = delete;
  QuickStartScreenHandler& operator=(const QuickStartScreenHandler&) = delete;

  ~QuickStartScreenHandler() override;

  // QuickStartView:
  void Show() override;
  void SetShapes(const quick_start::ShapeList& shape_list) override;
  void SetQRCode(base::Value::List blob) override;
  void ShowConnectingToWifi() override;
  void ShowConnectedToWifi(std::string ssid, std::string password) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_
