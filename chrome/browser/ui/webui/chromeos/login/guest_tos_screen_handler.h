// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GUEST_TOS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GUEST_TOS_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class GuestTosScreen;
}

namespace chromeos {

// Interface for dependency injection between GuestTosScreen and its
// WebUI representation.
class GuestTosScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"guest-tos"};

  virtual ~GuestTosScreenView() = default;

  virtual void Show(const std::string& google_eula_url,
                    const std::string& cros_eula_url) = 0;
  virtual void Bind(ash::GuestTosScreen* screen) = 0;
  virtual void Unbind() = 0;
};

class GuestTosScreenHandler : public GuestTosScreenView,
                              public BaseScreenHandler {
 public:
  using TView = GuestTosScreenView;

  GuestTosScreenHandler();
  ~GuestTosScreenHandler() override;
  GuestTosScreenHandler(const GuestTosScreenHandler&) = delete;
  GuestTosScreenHandler& operator=(const GuestTosScreenHandler&) = delete;

 private:
  // GuestTosScreenView
  void Show(const std::string& google_eula_url,
            const std::string& cros_eula_url) override;
  void Bind(ash::GuestTosScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;
  void RegisterMessages() override;

  void HandleAccept(bool enable_usage_stats);

  ash::GuestTosScreen* screen_ = nullptr;
  bool show_on_init_ = false;
  std::string google_eula_url_;
  std::string cros_eula_url_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::GuestTosScreenHandler;
using ::chromeos::GuestTosScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GUEST_TOS_SCREEN_HANDLER_H_
