// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_UNINSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_UNINSTALLER_VIEW_H_

#include <string>

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

// The Crostini application uninstaller. Displays a confirmation prompt,
// and kicks off the uninstall if the user confirms that they want the app
// uninstalled. Subsequent notifications are handled by CrostiniPackageService.
class CrostiniAppUninstallerView : public views::BubbleDialogDelegateView {
 public:
  // Show the "are you sure?"-style confirmation prompt. |app_id| should be an
  // ID understood by CrostiniRegistryService::GetRegistration().
  static void Show(Profile* profile, const std::string& app_id);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  CrostiniAppUninstallerView(Profile* profile, const std::string& app_id);
  ~CrostiniAppUninstallerView() override;

  Profile* profile_;
  std::string app_id_;

  base::WeakPtrFactory<CrostiniAppUninstallerView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppUninstallerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_UNINSTALLER_VIEW_H_
