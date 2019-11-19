// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

class PrefRegistrySimple;

namespace chromeos {

class EnableDebuggingScreen;

// Interface between enable debugging screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class EnableDebuggingScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"debugging"};

  virtual ~EnableDebuggingScreenView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetDelegate(EnableDebuggingScreen* screen) = 0;
};

// WebUI implementation of EnableDebuggingScreenView.
class EnableDebuggingScreenHandler : public EnableDebuggingScreenView,
                                     public BaseScreenHandler {
 public:
  using TView = EnableDebuggingScreenView;

  explicit EnableDebuggingScreenHandler(JSCallsContainer* js_calls_container);
  ~EnableDebuggingScreenHandler() override;

  // EnableDebuggingScreenView implementation:
  void Show() override;
  void Hide() override;
  void SetDelegate(EnableDebuggingScreen* screen) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  enum UIState {
    UI_STATE_ERROR = -1,
    UI_STATE_REMOVE_PROTECTION = 1,
    UI_STATE_SETUP = 2,
    UI_STATE_WAIT = 3,
    UI_STATE_DONE = 4,
  };

  // JS messages handlers.
  void HandleOnCancel();
  void HandleOnDone();
  void HandleOnLearnMore();
  void HandleOnRemoveRootFSProtection();
  void HandleOnSetup(const std::string& password);

  void ShowWithParams();

  // Callback for CryptohomeClient::WaitForServiceToBeAvailable
  void OnCryptohomeDaemonAvailabilityChecked(bool service_is_available);

  // Callback for DebugDaemonClient::WaitForServiceToBeAvailable
  void OnDebugDaemonServiceAvailabilityChecked(bool service_is_available);

  // Callback for DebugDaemonClient::EnableDebuggingFeatures().
  void OnEnableDebuggingFeatures(bool success);

  // Callback for DebugDaemonClient::QueryDebuggingFeatures().
  void OnQueryDebuggingFeatures(bool success, int features_flag);

  // Callback for DebugDaemonClient::RemoveRootfsVerification().
  void OnRemoveRootfsVerification(bool success);

  // Updates UI state.
  void UpdateUIState(UIState state);

  EnableDebuggingScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  base::WeakPtrFactory<EnableDebuggingScreenHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EnableDebuggingScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
