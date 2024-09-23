// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CHOOBE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CHOOBE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class ChobbeScreen;

// Interface for dependency injection between ChobbeScreen and its
// WebUI representation.
class ChoobeScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"choobe",
                                                       "ChoobeScreen"};

  virtual ~ChoobeScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::vector<ScreenSummary>& screens) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<ChoobeScreenView> AsWeakPtr() = 0;
};

class ChoobeScreenHandler final : public BaseScreenHandler,
                                  public ChoobeScreenView {
 public:
  using TView = ChoobeScreenView;

  ChoobeScreenHandler();

  ChoobeScreenHandler(const ChoobeScreenHandler&) = delete;
  ChoobeScreenHandler& operator=(const ChoobeScreenHandler&) = delete;

  ~ChoobeScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // ChoobeScreenView:
  void Show(const std::vector<ScreenSummary>& screens) override;
  base::WeakPtr<ChoobeScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<ChoobeScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CHOOBE_SCREEN_HANDLER_H_
