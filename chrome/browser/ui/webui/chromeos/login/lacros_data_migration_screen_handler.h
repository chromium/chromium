// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class LacrosDataMigrationScreen;
}

namespace chromeos {

// Interface for dependency injection between LacrosDataMigrationScreen and its
// WebUI representation.
class LacrosDataMigrationScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"lacros-data-migration"};

  virtual ~LacrosDataMigrationScreenView() {}

  // Binds `screen` to the view.
  virtual void Bind(ash::LacrosDataMigrationScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Updates the progress bar.
  virtual void SetProgressValue(int progress) = 0;

  // Displays the skip button.
  virtual void ShowSkipButton() = 0;
};

class LacrosDataMigrationScreenHandler : public BaseScreenHandler,
                                         public LacrosDataMigrationScreenView {
 public:
  using TView = LacrosDataMigrationScreenView;

  explicit LacrosDataMigrationScreenHandler(
      JSCallsContainer* js_calls_container);
  ~LacrosDataMigrationScreenHandler() override;
  LacrosDataMigrationScreenHandler(const LacrosDataMigrationScreenHandler&) =
      delete;
  LacrosDataMigrationScreenHandler& operator=(
      const LacrosDataMigrationScreenHandler&) = delete;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // LacrosDataMigrationScreenView:
  void Bind(ash::LacrosDataMigrationScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void SetProgressValue(int progress) override;
  void ShowSkipButton() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  ash::LacrosDataMigrationScreen* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LacrosDataMigrationScreenHandler;
using ::chromeos::LacrosDataMigrationScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_
