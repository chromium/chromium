// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PARENTAL_HANDOFF_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PARENTAL_HANDOFF_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace chromeos {

class ParentalHandoffScreen;
class JSCallsContainer;

// Interface for dependency injection between ParentalHandoffScreen and its
// WebUI representation.
class ParentalHandoffScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"parental-handoff"};

  virtual ~ParentalHandoffScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::u16string& username) = 0;

  // Binds |screen| to the view.
  virtual void Bind(ParentalHandoffScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
};

class ParentalHandoffScreenHandler : public BaseScreenHandler,
                                     public ParentalHandoffScreenView {
 public:
  using TView = ParentalHandoffScreenView;

  explicit ParentalHandoffScreenHandler(JSCallsContainer* js_calls_container);
  ParentalHandoffScreenHandler(const ParentalHandoffScreenHandler&) = delete;
  ParentalHandoffScreenHandler& operator=(const ParentalHandoffScreenHandler&) =
      delete;
  ~ParentalHandoffScreenHandler() override;

 private:
  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // Shows the contents of the screen.
  void Show(const std::u16string& username) override;
  void Bind(ParentalHandoffScreen* screen) override;
  void Unbind() override;

  ParentalHandoffScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PARENTAL_HANDOFF_SCREEN_HANDLER_H_
