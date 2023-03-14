// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ARC_VM_DATA_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ARC_VM_DATA_MIGRATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Interface for dependency injection between ArcVmDataMigrationScreen and its
// WebUI representation.
class ArcVmDataMigrationScreenView
    : public base::SupportsWeakPtr<ArcVmDataMigrationScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "arc-vm-data-migration", "ArcVmDataMigrationScreen"};

  enum class UIState {
    kLoading = 0,
    kWelcome = 1,
    kResume = 2,
    kProgress = 3,
    kSuccess = 4,
    kFailure = 5,
  };

  virtual ~ArcVmDataMigrationScreenView() = default;

  virtual void Show() = 0;
  virtual void SetUIState(UIState state) = 0;
  virtual void SetRequiredFreeDiskSpace(uint64_t required_free_disk_space) = 0;
  virtual void SetMinimumBatteryPercent(double percent) = 0;
  virtual void SetBatteryState(bool enough, bool connected) = 0;
  virtual void SetMigrationProgress(double progress) = 0;
  virtual void SetEstimatedRemainingTime(const base::TimeDelta& delta) = 0;
};

class ArcVmDataMigrationScreenHandler : public BaseScreenHandler,
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
  void SetUIState(UIState state) override;
  void SetRequiredFreeDiskSpace(uint64_t required_free_disk_space) override;
  void SetMinimumBatteryPercent(double percent) override;
  void SetBatteryState(bool enough, bool connected) override;
  void SetMigrationProgress(double progress) override;
  void SetEstimatedRemainingTime(const base::TimeDelta& delta) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ARC_VM_DATA_MIGRATION_SCREEN_HANDLER_H_
