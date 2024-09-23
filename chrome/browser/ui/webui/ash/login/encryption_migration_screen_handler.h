// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class EncryptionMigrationScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "encryption-migration", "EncryptionMigrationScreen"};

  virtual ~EncryptionMigrationScreenView() = default;

  virtual void Show() = 0;
  virtual base::WeakPtr<EncryptionMigrationScreenView> AsWeakPtr() = 0;
};

// WebUI implementation of EncryptionMigrationScreenView
class EncryptionMigrationScreenHandler final
    : public EncryptionMigrationScreenView,
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
  base::WeakPtr<EncryptionMigrationScreenView> AsWeakPtr() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<EncryptionMigrationScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
