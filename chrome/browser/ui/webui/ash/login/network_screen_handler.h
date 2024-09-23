// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface of network screen. Owned by NetworkScreen.
class NetworkScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"network-selection",
                                                       "NetworkScreen"};

  virtual ~NetworkScreenView() = default;

  // Shows the contents of the screen.
  virtual void ShowScreenWithData(base::Value::Dict data) = 0;

  // Shows error message in a bubble.
  virtual void ShowError(const std::u16string& message) = 0;

  // Hides error messages showing no error state.
  virtual void ClearErrors() = 0;

  virtual void SetQuickStartEntryPointVisibility(bool visible) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<NetworkScreenView> AsWeakPtr() = 0;
};

// WebUI implementation of NetworkScreenView. It is used to interact with
// the OOBE network selection screen.
class NetworkScreenHandler final : public NetworkScreenView,
                                   public BaseScreenHandler {
 public:
  using TView = NetworkScreenView;

  NetworkScreenHandler();

  NetworkScreenHandler(const NetworkScreenHandler&) = delete;
  NetworkScreenHandler& operator=(const NetworkScreenHandler&) = delete;

  ~NetworkScreenHandler() override;

 private:
  // NetworkScreenView:
  void ShowScreenWithData(base::Value::Dict data) override;
  void ShowError(const std::u16string& message) override;
  void ClearErrors() override;
  void SetQuickStartEntryPointVisibility(bool visible) override;
  base::WeakPtr<NetworkScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

 private:
  base::WeakPtrFactory<NetworkScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_SCREEN_HANDLER_H_
