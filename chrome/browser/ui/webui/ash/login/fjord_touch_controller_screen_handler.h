// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_TOUCH_CONTROLLER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_TOUCH_CONTROLLER_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class FjordTouchControllerScreen;

// Interface for dependency injection between FjordTouchControllerScreen and its
// WebUI representation.
class FjordTouchControllerScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "fjord-touch-controller", "FjordTouchControllerScreen"};

  virtual ~FjordTouchControllerScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FjordTouchControllerScreenView> AsWeakPtr() = 0;
};

class FjordTouchControllerScreenHandler final
    : public BaseScreenHandler,
      public FjordTouchControllerScreenView {
 public:
  using TView = FjordTouchControllerScreenView;

  FjordTouchControllerScreenHandler();
  FjordTouchControllerScreenHandler(const FjordTouchControllerScreenHandler&) =
      delete;
  FjordTouchControllerScreenHandler& operator=(
      const FjordTouchControllerScreenHandler&) = delete;
  ~FjordTouchControllerScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override {}

  // FjordTouchControllerScreenView:
  void Show() override;
  base::WeakPtr<FjordTouchControllerScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<FjordTouchControllerScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_TOUCH_CONTROLLER_SCREEN_HANDLER_H_
