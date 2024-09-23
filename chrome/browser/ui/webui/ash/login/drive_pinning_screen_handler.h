// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DRIVE_PINNING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DRIVE_PINNING_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class DrivePinningScreen;

// Interface for dependency injection between DrivePinningScreen and its
// WebUI representation.
class DrivePinningScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"drive-pinning",
                                                       "DrivePinningScreen"};

  virtual ~DrivePinningScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict data) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<DrivePinningScreenView> AsWeakPtr() = 0;
};

class DrivePinningScreenHandler final : public BaseScreenHandler,
                                        public DrivePinningScreenView {
 public:
  using TView = DrivePinningScreenView;

  DrivePinningScreenHandler();

  DrivePinningScreenHandler(const DrivePinningScreenHandler&) = delete;
  DrivePinningScreenHandler& operator=(const DrivePinningScreenHandler&) =
      delete;

  ~DrivePinningScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  void Show(base::Value::Dict data) override;
  base::WeakPtr<DrivePinningScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<DrivePinningScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DRIVE_PINNING_SCREEN_HANDLER_H_
