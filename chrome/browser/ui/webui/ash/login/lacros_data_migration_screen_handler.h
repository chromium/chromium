// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Interface for dependency injection between LacrosDataMigrationScreen and its
// WebUI representation.
class LacrosDataMigrationScreenView
    : public base::SupportsWeakPtr<LacrosDataMigrationScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "lacros-data-migration", "LacrosDataMigrationScreen"};

  virtual ~LacrosDataMigrationScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Updates the progress bar.
  virtual void SetProgressValue(int progress) = 0;

  // Displays the skip button.
  virtual void ShowSkipButton() = 0;

  // Notifies the UI about low battery.
  virtual void SetLowBatteryStatus(bool low_battery) = 0;

  // Displays the error page. If |required_size| is non nullopt, the error
  // message is to navigate users to make some space on their disk to run
  // migration.
  // |show_goto_files| can control
  virtual void SetFailureStatus(const absl::optional<uint64_t>& required_size,
                                bool show_goto_files) = 0;
};

class LacrosDataMigrationScreenHandler : public BaseScreenHandler,
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
  void SetProgressValue(int progress) override;
  void ShowSkipButton() override;
  void SetLowBatteryStatus(bool low_battery) override;
  void SetFailureStatus(const absl::optional<uint64_t>& required_size,
                        bool show_goto_files) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LACROS_DATA_MIGRATION_SCREEN_HANDLER_H_
