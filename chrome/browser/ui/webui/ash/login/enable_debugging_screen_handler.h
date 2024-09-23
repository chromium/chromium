// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

class PrefRegistrySimple;

namespace ash {

// Interface between enable debugging screen and its representation.
class EnableDebuggingScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"debugging",
                                                       "EnableDebuggingScreen"};

  enum UIState {
    kUIStateError = -1,
    kUIStateRemoveProtection = 1,
    kUIStateSetup = 2,
    kUIStateWait = 3,
    kUIStateDone = 4,
  };

  virtual ~EnableDebuggingScreenView() = default;

  virtual void Show() = 0;
  virtual void UpdateUIState(UIState state) = 0;
  virtual base::WeakPtr<EnableDebuggingScreenView> AsWeakPtr() = 0;
};

// WebUI implementation of EnableDebuggingScreenView.
class EnableDebuggingScreenHandler final : public EnableDebuggingScreenView,
                                           public BaseScreenHandler {
 public:
  using TView = EnableDebuggingScreenView;

  EnableDebuggingScreenHandler();

  EnableDebuggingScreenHandler(const EnableDebuggingScreenHandler&) = delete;
  EnableDebuggingScreenHandler& operator=(const EnableDebuggingScreenHandler&) =
      delete;

  ~EnableDebuggingScreenHandler() override;

  // EnableDebuggingScreenView implementation:
  void Show() override;
  void UpdateUIState(UIState state) override;
  base::WeakPtr<EnableDebuggingScreenView> AsWeakPtr() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  base::WeakPtrFactory<EnableDebuggingScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENABLE_DEBUGGING_SCREEN_HANDLER_H_
