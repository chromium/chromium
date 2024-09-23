// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ARC_VM_DATA_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ARC_VM_DATA_MIGRATION_SCREEN_HANDLER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace ash {

// Interface for dependency injection between ArcVmDataMigrationScreen and its
// WebUI representation.
class ArcVmDataMigrationScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "arc-vm-data-migration", "ArcVmDataMigrationScreen"};

  virtual ~ArcVmDataMigrationScreenView() = default;

  virtual void Show() = 0;
  virtual base::WeakPtr<ArcVmDataMigrationScreenView> AsWeakPtr() = 0;
};

class ArcVmDataMigrationScreenHandler final
    : public BaseScreenHandler,
      public ArcVmDataMigrationScreenView {
 public:
  using TView = ArcVmDataMigrationScreenView;

  ArcVmDataMigrationScreenHandler();
  ~ArcVmDataMigrationScreenHandler() override;
  ArcVmDataMigrationScreenHandler(const ArcVmDataMigrationScreenHandler&) =
      delete;
  ArcVmDataMigrationScreenHandler& operator=(
      const ArcVmDataMigrationScreenHandler&) = delete;

 private:
  // BaseScreenHandler override:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // ArcVmDataMigrationScreenView overrides:
  void Show() override;

  base::WeakPtr<ArcVmDataMigrationScreenView> AsWeakPtr() override;

  base::WeakPtrFactory<ArcVmDataMigrationScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ARC_VM_DATA_MIGRATION_SCREEN_HANDLER_H_
