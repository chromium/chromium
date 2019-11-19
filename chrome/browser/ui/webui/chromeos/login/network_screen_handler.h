// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class CoreOobeView;
class NetworkScreen;

// Interface of network screen. Owned by NetworkScreen.
class NetworkScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"network-selection"};

  virtual ~NetworkScreenView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(NetworkScreen* screen) = 0;

  // Unbinds model from the view.
  virtual void Unbind() = 0;

  // Shows error message in a bubble.
  virtual void ShowError(const base::string16& message) = 0;

  // Hides error messages showing no error state.
  virtual void ClearErrors() = 0;

  // Shows network connecting status or network selection otherwise.
  virtual void ShowConnectingStatus(bool connecting,
                                    const base::string16& network_id) = 0;

  // Enables or disables offline Demo Mode during Demo Mode network selection.
  virtual void SetOfflineDemoModeEnabled(bool enabled) = 0;
};

// WebUI implementation of NetworkScreenView. It is used to interact with
// the OOBE network selection screen.
class NetworkScreenHandler : public NetworkScreenView,
                             public BaseScreenHandler {
 public:
  using TView = NetworkScreenView;

  NetworkScreenHandler(JSCallsContainer* js_calls_container,
                       CoreOobeView* core_oobe_view);
  ~NetworkScreenHandler() override;

 private:
  // NetworkScreenView:
  void Show() override;
  void Hide() override;
  void Bind(NetworkScreen* screen) override;
  void Unbind() override;
  void ShowError(const base::string16& message) override;
  void ClearErrors() override;
  void ShowConnectingStatus(bool connecting,
                            const base::string16& network_id) override;
  void SetOfflineDemoModeEnabled(bool enabled) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

  CoreOobeView* core_oobe_view_ = nullptr;
  NetworkScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
