// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace ash {

// Interface for dependency injection between LacrosDataMigrationScreen and its
// WebUI representation.
class LacrosDataMigrationScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "lacros-data-migration", "LacrosDataMigrationScreen"};

  virtual ~LacrosDataMigrationScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<LacrosDataMigrationScreenView> AsWeakPtr() = 0;
};

class LacrosDataMigrationScreenHandler final
    : public BaseScreenHandler,
      public LacrosDataMigrationScreenView {
 public:
  using TView = LacrosDataMigrationScreenView;

  LacrosDataMigrationScreenHandler();
  ~LacrosDataMigrationScreenHandler() override;
  LacrosDataMigrationScreenHandler(const LacrosDataMigrationScreenHandler&) =
      delete;
  LacrosDataMigrationScreenHandler& operator=(
      const LacrosDataMigrationScreenHandler&) = delete;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // LacrosDataMigrationScreenView:
  void Show() override;
  base::WeakPtr<LacrosDataMigrationScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<LacrosDataMigrationScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_
