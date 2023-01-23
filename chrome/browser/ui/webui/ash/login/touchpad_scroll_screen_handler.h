// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TOUCHPAD_SCROLL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TOUCHPAD_SCROLL_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class TouchpadScrollScreen;

// Interface for dependency injection between TouchpadScrollScreen and its
// WebUI representation.
class TouchpadScrollScreenView
    : public base::SupportsWeakPtr<TouchpadScrollScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"touchpad-scroll",
                                                       "TouchpadScrollScreen"};

  virtual ~TouchpadScrollScreenView() = default;

  // Set the reverse scrolling preferences to the toggle
  virtual void SetReverseScrolling(bool value) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;
};

class TouchpadScrollScreenHandler : public BaseScreenHandler,
                                    public TouchpadScrollScreenView {
 public:
  using TView = TouchpadScrollScreenView;

  TouchpadScrollScreenHandler();

  TouchpadScrollScreenHandler(const TouchpadScrollScreenHandler&) = delete;
  TouchpadScrollScreenHandler& operator=(const TouchpadScrollScreenHandler&) =
      delete;

  ~TouchpadScrollScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  void SetReverseScrolling(bool value) override;

  // TouchpadScrollScreenView:
  void Show() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TOUCHPAD_SCROLL_SCREEN_HANDLER_H_
