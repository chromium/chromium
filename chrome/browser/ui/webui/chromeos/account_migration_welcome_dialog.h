// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MIGRATION_WELCOME_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MIGRATION_WELCOME_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "url/gurl.h"

namespace chromeos {

class AccountMigrationWelcomeDialog : public SystemWebDialogDelegate {
 public:
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
  const std::string& Id() override;

  std::string GetUserEmail() const;

 private:
  const std::string email_;
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(AccountMigrationWelcomeDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MIGRATION_WELCOME_DIALOG_H_
