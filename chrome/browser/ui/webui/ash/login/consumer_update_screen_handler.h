// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CONSUMER_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CONSUMER_UPDATE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class ConsumerUpdateScreen;

// Interface for dependency injection between ConsumerUpdateScreen and its
// WebUI representation.
class ConsumerUpdateScreenView
    : public base::SupportsWeakPtr<ConsumerUpdateScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"consumer-update",
                                                       "ConsumerUpdateScreen"};

  enum class UIState {
    kCheckingForUpdate = 0,
    kUpdateInProgress = 1,
    kRestartInProgress = 2,
    kManualReboot = 3,
    kCellularPermission = 4,
  };

  virtual ~ConsumerUpdateScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void SetUpdateState(UIState value) = 0;
  virtual void SetUpdateStatus(int percent,
                               const std::u16string& percent_message,
                               const std::u16string& timeleft_message) = 0;
  virtual void ShowLowBatteryWarningMessage(bool value) = 0;
  virtual void SetAutoTransition(bool value) = 0;
  virtual void SetIsUpdateMandatory(bool value) = 0;
};

class ConsumerUpdateScreenHandler : public BaseScreenHandler,
                                    public ConsumerUpdateScreenView {
 public:
  using TView = ConsumerUpdateScreenView;

  ConsumerUpdateScreenHandler();

  ConsumerUpdateScreenHandler(const ConsumerUpdateScreenHandler&) = delete;
  ConsumerUpdateScreenHandler& operator=(const ConsumerUpdateScreenHandler&) =
      delete;

  ~ConsumerUpdateScreenHandler() override;

  // ConsumerUpdateScreenView:
  void Show() override;

  void SetUpdateState(ConsumerUpdateScreenView::UIState value) override;
  void SetUpdateStatus(int percent,
                       const std::u16string& percent_message,
                       const std::u16string& timeleft_message) override;
  void ShowLowBatteryWarningMessage(bool value) override;
  void SetAutoTransition(bool value) override;
  void SetIsUpdateMandatory(bool value) override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CONSUMER_UPDATE_SCREEN_HANDLER_H_
