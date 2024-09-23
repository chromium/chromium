// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GUEST_TOS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GUEST_TOS_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between GuestTosScreen and its
// WebUI representation.
class GuestTosScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"guest-tos",
                                                       "GuestTosScreen"};

  virtual ~GuestTosScreenView() = default;

  virtual void Show(const std::string& google_eula_url,
                    const std::string& cros_eula_url) = 0;
  virtual base::WeakPtr<GuestTosScreenView> AsWeakPtr() = 0;
};

class GuestTosScreenHandler final : public GuestTosScreenView,
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
  base::WeakPtr<GuestTosScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<GuestTosScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GUEST_TOS_SCREEN_HANDLER_H_
