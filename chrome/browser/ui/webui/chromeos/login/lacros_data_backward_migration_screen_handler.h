// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class LacrosDataBackwardMigrationScreen;
}

namespace chromeos {

// Interface for dependency injection between LacrosDataBackwardMigrationScreen
// and its WebUI representation.
class LacrosDataBackwardMigrationScreenView
    : public base::SupportsWeakPtr<LacrosDataBackwardMigrationScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "lacros-data-backward-migration"};

  virtual ~LacrosDataBackwardMigrationScreenView() = default;

  virtual void Show() = 0;
};

class LacrosDataBackwardMigrationScreenHandler
    : public BaseScreenHandler,
      public LacrosDataBackwardMigrationScreenView {
 public:
  using TView = LacrosDataBackwardMigrationScreenView;

  LacrosDataBackwardMigrationScreenHandler();
  ~LacrosDataBackwardMigrationScreenHandler() override;
  LacrosDataBackwardMigrationScreenHandler(
      const LacrosDataBackwardMigrationScreenHandler&) = delete;
  LacrosDataBackwardMigrationScreenHandler& operator=(
      const LacrosDataBackwardMigrationScreenHandler&) = delete;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // LacrosDataBackwardMigrationScreenView:
  void Show() override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LacrosDataBackwardMigrationScreenHandler;
using ::chromeos::LacrosDataBackwardMigrationScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_HANDLER_H_
