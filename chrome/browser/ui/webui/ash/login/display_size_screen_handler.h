// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DISPLAY_SIZE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DISPLAY_SIZE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between DisplaySizeScreen and its
// WebUI representation.
class DisplaySizeScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"display-size",
                                                       "DisplaySizeScreen"};

  virtual ~DisplaySizeScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict data) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<DisplaySizeScreenView> AsWeakPtr() = 0;
};

class DisplaySizeScreenHandler final : public BaseScreenHandler,
                                       public DisplaySizeScreenView {
 public:
  using TView = DisplaySizeScreenView;

  DisplaySizeScreenHandler();

  DisplaySizeScreenHandler(const DisplaySizeScreenHandler&) = delete;
  DisplaySizeScreenHandler& operator=(const DisplaySizeScreenHandler&) = delete;

  ~DisplaySizeScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // DisplaySizeScreenView:
  void Show(base::Value::Dict data) override;
  base::WeakPtr<DisplaySizeScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<DisplaySizeScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DISPLAY_SIZE_SCREEN_HANDLER_H_
