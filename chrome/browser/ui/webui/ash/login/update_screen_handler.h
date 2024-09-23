// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class UpdateScreen;

// Interface for dependency injection between WelcomeScreen and its actual
// representation. Owned by UpdateScreen.
class UpdateView {
 public:
  // The screen name must never change. It's stored into local state as a
  // pending screen during OOBE update. So the value should be the same between
  // versions.
  inline constexpr static StaticOobeScreenId kScreenId{"oobe-update",
                                                       "UpdateScreen"};

  enum class UIState {
    kCheckingForUpdate = 0,
    kUpdateInProgress = 1,
    kRestartInProgress = 2,
    kManualReboot = 3,
    kCellularPermission = 4,
    kOptOutInfo = 5,
  };

  virtual ~UpdateView() {}

  // Shows the contents of the screen.
  virtual void Show(bool is_opt_out_enabled) = 0;

  virtual void SetUpdateState(UIState value) = 0;
  virtual void SetUpdateStatus(int percent,
                               const std::u16string& percent_message,
                               const std::u16string& timeleft_message) = 0;
  virtual void ShowLowBatteryWarningMessage(bool value) = 0;
  virtual void SetAutoTransition(bool value) = 0;
  virtual void SetCancelUpdateShortcutEnabled(bool value) = 0;
  virtual base::WeakPtr<UpdateView> AsWeakPtr() = 0;
};

class UpdateScreenHandler final : public UpdateView, public BaseScreenHandler {
 public:
  using TView = UpdateView;

  UpdateScreenHandler();

  UpdateScreenHandler(const UpdateScreenHandler&) = delete;
  UpdateScreenHandler& operator=(const UpdateScreenHandler&) = delete;

  ~UpdateScreenHandler() override;

 private:
  // UpdateView:
  void Show(bool is_opt_out_enabled) override;

  void SetUpdateState(UpdateView::UIState value) override;
  void SetUpdateStatus(int percent,
                       const std::u16string& percent_message,
                       const std::u16string& timeleft_message) override;
  void ShowLowBatteryWarningMessage(bool value) override;
  void SetAutoTransition(bool value) override;
  void SetCancelUpdateShortcutEnabled(bool value) override;
  base::WeakPtr<UpdateView> AsWeakPtr() override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<UpdateView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_UPDATE_SCREEN_HANDLER_H_
