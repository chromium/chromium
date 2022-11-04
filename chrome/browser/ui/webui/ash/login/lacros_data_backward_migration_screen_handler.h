// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class LacrosDataBackwardMigrationScreen;

// Interface for dependency injection between LacrosDataBackwardMigrationScreen
// and its WebUI representation.
class LacrosDataBackwardMigrationScreenView
    : public base::SupportsWeakPtr<LacrosDataBackwardMigrationScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "lacros-data-backward-migration", "LacrosDataBackwardMigrationScreen"};

  virtual ~LacrosDataBackwardMigrationScreenView() = default;

  virtual void Show() = 0;

  // Updates the progress bar.
  // progress is a percentage.
  virtual void SetProgressValue(int progress) = 0;

  // Show an error message.
  virtual void SetFailureStatus() = 0;
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
  void SetProgressValue(int progress) override;
  void SetFailureStatus() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_HANDLER_H_
