// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_STATION_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_STATION_SETUP_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class FjordStationSetupScreen;

// Interface for dependency injection between FjordStationSetupScreen and its
// WebUI representation.
class FjordStationSetupScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "fjord-station-setup", "FjordStationSetupScreen"};

  virtual ~FjordStationSetupScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FjordStationSetupScreenView> AsWeakPtr() = 0;
};

class FjordStationSetupScreenHandler final
    : public BaseScreenHandler,
      public FjordStationSetupScreenView {
 public:
  using TView = FjordStationSetupScreenView;

  FjordStationSetupScreenHandler();
  FjordStationSetupScreenHandler(const FjordStationSetupScreenHandler&) =
      delete;
  FjordStationSetupScreenHandler& operator=(
      const FjordStationSetupScreenHandler&) = delete;
  ~FjordStationSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // FjordStationSetupScreenView:
  void Show() override;
  base::WeakPtr<FjordStationSetupScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<FjordStationSetupScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_STATION_SETUP_SCREEN_HANDLER_H_
