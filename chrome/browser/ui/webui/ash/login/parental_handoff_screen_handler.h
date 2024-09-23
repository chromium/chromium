// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PARENTAL_HANDOFF_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PARENTAL_HANDOFF_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace ash {

// Interface for dependency injection between ParentalHandoffScreen and its
// WebUI representation.
class ParentalHandoffScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"parental-handoff",
                                                       "ParentalHandoffScreen"};

  virtual ~ParentalHandoffScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::u16string& username) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<ParentalHandoffScreenView> AsWeakPtr() = 0;
};

class ParentalHandoffScreenHandler final : public BaseScreenHandler,
                                           public ParentalHandoffScreenView {
 public:
  using TView = ParentalHandoffScreenView;

  ParentalHandoffScreenHandler();
  ParentalHandoffScreenHandler(const ParentalHandoffScreenHandler&) = delete;
  ParentalHandoffScreenHandler& operator=(const ParentalHandoffScreenHandler&) =
      delete;
  ~ParentalHandoffScreenHandler() override;

 private:
  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // Shows the contents of the screen.
  void Show(const std::u16string& username) override;
  base::WeakPtr<ParentalHandoffScreenView> AsWeakPtr() override;

  base::WeakPtrFactory<ParentalHandoffScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PARENTAL_HANDOFF_SCREEN_HANDLER_H_
