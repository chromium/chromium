// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class UpdateScreen;

// Interface for dependency injection between WelcomeScreen and its actual
// representation. Owned by UpdateScreen.
class UpdateView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"update"};

  virtual ~UpdateView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(UpdateScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Set the estimated time left, in seconds.
  virtual void SetEstimatedTimeLeft(int value) = 0;
  virtual void SetShowEstimatedTimeLeft(bool value) = 0;
  virtual void SetUpdateCompleted(bool value) = 0;
  virtual void SetShowCurtain(bool value) = 0;
  virtual void SetProgressMessage(const base::string16& value) = 0;
  virtual void SetProgress(int value) = 0;
  virtual void SetRequiresPermissionForCellular(bool value) = 0;
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
  void SetEstimatedTimeLeft(int value) override;
  void SetShowEstimatedTimeLeft(bool value) override;
  void SetUpdateCompleted(bool value) override;
  void SetShowCurtain(bool value) override;
  void SetProgressMessage(const base::string16& value) override;
  void SetProgress(int value) override;
  void SetRequiresPermissionForCellular(bool value) override;
  void SetCancelUpdateShortcutEnabled(bool value) override;

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
