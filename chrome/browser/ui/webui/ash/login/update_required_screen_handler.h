// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_UPDATE_REQUIRED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_UPDATE_REQUIRED_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class UpdateRequiredScreen;

// Interface for dependency injection between UpdateRequiredScreen and its
// WebUI representation.

class UpdateRequiredView {
 public:
  enum UIState {
    UPDATE_REQUIRED_MESSAGE = 0,   // 'System update required' message.
    UPDATE_PROCESS,                // Update is going on.
    UPDATE_NEED_PERMISSION,        // Need user's permission to proceed.
    UPDATE_COMPLETED_NEED_REBOOT,  // Update successful, manual reboot is
                                   // needed.
    UPDATE_ERROR,                  // An error has occurred.
    EOL_REACHED,                   // End of Life reached message.
    UPDATE_NO_NETWORK              // No network available to update
  };

  inline constexpr static StaticOobeScreenId kScreenId{"update-required",
                                                       "UpdateRequiredScreen"};

  virtual ~UpdateRequiredView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Is device connected to some network?
  virtual void SetIsConnected(bool connected) = 0;
  // Is progress unavailable (e.g. we are checking for updates)?
  virtual void SetUpdateProgressUnavailable(bool unavailable) = 0;
  // Set progress percentage.
  virtual void SetUpdateProgressValue(int progress) = 0;
  // Set progress message (like "Verifying").
  virtual void SetUpdateProgressMessage(const std::u16string& message) = 0;
  // Set the visibility of the estimated time left.
  virtual void SetEstimatedTimeLeftVisible(bool visible) = 0;
  // Set the estimated time left, in seconds.
  virtual void SetEstimatedTimeLeft(int seconds_left) = 0;
  // Set the UI state of the screen.
  virtual void SetUIState(UpdateRequiredView::UIState ui_state) = 0;
  // Set enterprise and device name to be used in strings in the UI.
  virtual void SetEnterpriseAndDeviceName(const std::string& enterpriseDomain,
                                          const std::u16string& deviceName) = 0;
  virtual void SetEolMessage(const std::string& eolMessage) = 0;
  virtual void SetIsUserDataPresent(bool deleted) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<UpdateRequiredView> AsWeakPtr() = 0;
};

class UpdateRequiredScreenHandler final : public UpdateRequiredView,
                                          public BaseScreenHandler {
 public:
  using TView = UpdateRequiredView;

  UpdateRequiredScreenHandler();

  UpdateRequiredScreenHandler(const UpdateRequiredScreenHandler&) = delete;
  UpdateRequiredScreenHandler& operator=(const UpdateRequiredScreenHandler&) =
      delete;

  ~UpdateRequiredScreenHandler() override;

 private:
  void Show() override;

  void SetIsConnected(bool connected) override;
  void SetUpdateProgressUnavailable(bool unavailable) override;
  void SetUpdateProgressValue(int progress) override;
  void SetUpdateProgressMessage(const std::u16string& message) override;
  void SetEstimatedTimeLeftVisible(bool visible) override;
  void SetEstimatedTimeLeft(int seconds_left) override;
  void SetUIState(UpdateRequiredView::UIState ui_state) override;
  void SetEnterpriseAndDeviceName(const std::string& enterpriseDomain,
                                  const std::u16string& deviceName) override;
  void SetEolMessage(const std::string& eolMessage) override;
  void SetIsUserDataPresent(bool data_present) override;
  base::WeakPtr<UpdateRequiredView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // The domain name for which update required screen is being shown.
  std::string domain_;

 private:
  base::WeakPtrFactory<UpdateRequiredView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_UPDATE_REQUIRED_SCREEN_HANDLER_H_
