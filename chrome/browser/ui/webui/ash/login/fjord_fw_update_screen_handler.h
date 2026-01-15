// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_FW_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_FW_UPDATE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class FjordFwUpdateScreen;

// Interface for dependency injection between FjordFwUpdateScreen and its
// WebUI representation.
class FjordFwUpdateScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"fjord-fw-update",
                                                       "FjordFwUpdateScreen"};

  virtual ~FjordFwUpdateScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FjordFwUpdateScreenView> AsWeakPtr() = 0;
};

class FjordFwUpdateScreenHandler final : public BaseScreenHandler,
                                         public FjordFwUpdateScreenView {
 public:
  using TView = FjordFwUpdateScreenView;

  FjordFwUpdateScreenHandler();
  FjordFwUpdateScreenHandler(const FjordFwUpdateScreenHandler&) = delete;
  FjordFwUpdateScreenHandler& operator=(const FjordFwUpdateScreenHandler&) =
      delete;
  ~FjordFwUpdateScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // FjordFwUpdateScreenView:
  void Show() override;
  base::WeakPtr<FjordFwUpdateScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<FjordFwUpdateScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_FW_UPDATE_SCREEN_HANDLER_H_
