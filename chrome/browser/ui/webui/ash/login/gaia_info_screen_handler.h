// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_INFO_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_INFO_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class GaiaInfoScreen;

// Interface for dependency injection between GaiaInfoScreen and its
// WebUI representation.
class GaiaInfoScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"gaia-info",
                                                       "GaiaInfoScreen"};

  virtual ~GaiaInfoScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<GaiaInfoScreenView> AsWeakPtr() = 0;
};

class GaiaInfoScreenHandler final : public BaseScreenHandler,
                                    public GaiaInfoScreenView {
 public:
  using TView = GaiaInfoScreenView;

  GaiaInfoScreenHandler();

  GaiaInfoScreenHandler(const GaiaInfoScreenHandler&) = delete;
  GaiaInfoScreenHandler& operator=(const GaiaInfoScreenHandler&) = delete;

  ~GaiaInfoScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // GaiaInfoScreenView:
  void Show() override;
  base::WeakPtr<GaiaInfoScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<GaiaInfoScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_INFO_SCREEN_HANDLER_H_
