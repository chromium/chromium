// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

class PrefRegistrySimple;

namespace chromeos {

class EnableDebuggingScreen;

// Interface between enable debugging screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class EnableDebuggingScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"debugging"};

  enum UIState {
    UI_STATE_ERROR = -1,
    UI_STATE_REMOVE_PROTECTION = 1,
    UI_STATE_SETUP = 2,
    UI_STATE_WAIT = 3,
    UI_STATE_DONE = 4,
  };

  virtual ~EnableDebuggingScreenView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetDelegate(EnableDebuggingScreen* screen) = 0;
  virtual void UpdateUIState(UIState state) = 0;
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
  void UpdateUIState(UIState state) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // JS messages handlers.
  void HandleOnSetup(const std::string& password);

  EnableDebuggingScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(EnableDebuggingScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
