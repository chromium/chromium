// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class WrongHWIDScreen;

// Interface between wrong HWID screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class WrongHWIDScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"wrong-hwid"};

  virtual ~WrongHWIDScreenView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Binds `screen` to the view.
  virtual void Bind(WrongHWIDScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
};

// WebUI implementation of WrongHWIDScreenActor.
class WrongHWIDScreenHandler : public WrongHWIDScreenView,
                               public BaseScreenHandler {
 public:
  using TView = WrongHWIDScreenView;

  explicit WrongHWIDScreenHandler(JSCallsContainer* js_calls_container);
  ~WrongHWIDScreenHandler() override;

 private:
  // WrongHWIDScreenActor implementation:
  void Show() override;
  void Hide() override;
  void Bind(WrongHWIDScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  WrongHWIDScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(WrongHWIDScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_
