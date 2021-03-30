// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class UpdateScreen;

// Interface for dependency injection between WelcomeScreen and its actual
// representation. Owned by UpdateScreen.
class UpdateView {
 public:
  // The screen name must never change. It's stored into local state as a
  // pending screen during OOBE update. So the value should be the same between
  // versions.
  constexpr static StaticOobeScreenId kScreenId{"oobe-update"};

  enum class UIState {
    kCheckingForUpdate = 0,
    kUpdateInProgress = 1,
    kRestartInProgress = 2,
    kManualReboot = 3,
    kCellularPermission = 4,
  };

  virtual ~UpdateView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds `screen` to the view.
  virtual void Bind(UpdateScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  virtual void SetUpdateState(UIState value) = 0;
  virtual void SetUpdateStatus(int percent,
                               const std::u16string& percent_message,
                               const std::u16string& timeleft_message) = 0;
  virtual void ShowLowBatteryWarningMessage(bool value) = 0;
  virtual void SetAutoTransition(bool value) = 0;
  virtual void SetCancelUpdateShortcutEnabled(bool value) = 0;
};

class UpdateScreenHandler : public UpdateView, public BaseScreenHandler {
 public:
  using TView = UpdateView;

  explicit UpdateScreenHandler(JSCallsContainer* js_calls_container);
  ~UpdateScreenHandler() override;

 private:
  // UpdateView:
  void Show() override;
  void Hide() override;
  void Bind(UpdateScreen* screen) override;
  void Unbind() override;

  void SetUpdateState(UpdateView::UIState value) override;
  void SetUpdateStatus(int percent,
                       const std::u16string& percent_message,
                       const std::u16string& timeleft_message) override;
  void ShowLowBatteryWarningMessage(bool value) override;
  void SetAutoTransition(bool value) override;
  void SetCancelUpdateShortcutEnabled(bool value) override;

  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& details);

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  UpdateScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
