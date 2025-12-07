// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ACCOUNT_MANAGER_ACCOUNT_MIGRATION_WELCOME_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ACCOUNT_MANAGER_ACCOUNT_MIGRATION_WELCOME_DIALOG_H_

#include <string>

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "url/gurl.h"

namespace ash {

class AccountMigrationWelcomeDialog : public SystemWebDialogDelegate {
 public:
  AccountMigrationWelcomeDialog(const AccountMigrationWelcomeDialog&) = delete;
  AccountMigrationWelcomeDialog& operator=(
      const AccountMigrationWelcomeDialog&) = delete;

  // Displays the migration dialog for the |email|.
  static AccountMigrationWelcomeDialog* Show(const std::string& email);

 protected:
  AccountMigrationWelcomeDialog(const GURL gurl, const std::string& email);

  ~AccountMigrationWelcomeDialog() override;

  // ui::SystemWebDialogDelegate overrides.
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldShowCloseButton() const override;
  std::string Id() override;

  std::string GetUserEmail() const;

 private:
  const std::string email_;
  const std::string id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ACCOUNT_MANAGER_ACCOUNT_MIGRATION_WELCOME_DIALOG_H_
