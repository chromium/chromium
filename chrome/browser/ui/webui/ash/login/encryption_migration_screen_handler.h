// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class EncryptionMigrationScreenView
    : public base::SupportsWeakPtr<EncryptionMigrationScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "encryption-migration", "EncryptionMigrationScreen"};

  // Enumeration for migration UI state. These values must be kept in sync with
  // EncryptionMigrationUIState in JS code, and match the numbering for
  // MigrationUIScreen in histograms/enums.xml. Do not reorder or remove items,
  // only add new items before COUNT.
  enum UIState {
    INITIAL = 0,
    READY = 1,
    MIGRATING = 2,
    MIGRATION_FAILED = 3,
    NOT_ENOUGH_STORAGE = 4,
    COUNT
  };

  virtual ~EncryptionMigrationScreenView() = default;

  virtual void Show() = 0;
  virtual void SetBatteryState(double batteryPercent,
                               bool isEnoughBattery,
                               bool isCharging) = 0;
  virtual void SetIsResuming(bool isResuming) = 0;
  virtual void SetUIState(UIState state) = 0;
  virtual void SetSpaceInfoInString(int64_t availableSpaceSize,
                                    int64_t necessarySpaceSize) = 0;
  virtual void SetNecessaryBatteryPercent(double batteryPercent) = 0;
  virtual void SetMigrationProgress(double progress) = 0;
};

// WebUI implementation of EncryptionMigrationScreenView
class EncryptionMigrationScreenHandler : public EncryptionMigrationScreenView,
                                         public BaseScreenHandler {
 public:
  using TView = EncryptionMigrationScreenView;

  EncryptionMigrationScreenHandler();

  EncryptionMigrationScreenHandler(const EncryptionMigrationScreenHandler&) =
      delete;
  EncryptionMigrationScreenHandler& operator=(
      const EncryptionMigrationScreenHandler&) = delete;

  ~EncryptionMigrationScreenHandler() override;

  // EncryptionMigrationScreenView implementation:
  void Show() override;
  void SetBatteryState(double batteryPercent,
                       bool isEnoughBattery,
                       bool isCharging) override;
  void SetIsResuming(bool isResuming) override;
  void SetUIState(UIState state) override;
  void SetSpaceInfoInString(int64_t availableSpaceSize,
                            int64_t necessarySpaceSize) override;
  void SetNecessaryBatteryPercent(double batteryPercent) override;
  void SetMigrationProgress(double progress) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
