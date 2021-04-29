// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace views {
class Checkbox;
}  // namespace views

namespace gfx {
class ImageSkia;
}

// This class generates the unified uninstall dialog based on the app type. Once
// the user has confirmed the uninstall, this class calls the parent class
// apps::UninstallDialog::UiBase to notify AppService, which transfers control
// to the publisher to uninstall the app.
class AppUninstallDialogView : public apps::UninstallDialog::UiBase,
                               public AppDialogView {
 public:
  AppUninstallDialogView(Profile* profile,
                         apps::mojom::AppType app_type,
                         const std::string& app_id,
                         const std::string& app_name,
                         gfx::ImageSkia image,
                         apps::UninstallDialog* uninstall_dialog);
  ~AppUninstallDialogView() override;

  static AppUninstallDialogView* GetActiveViewForTesting();

 private:
  void InitializeView(Profile* profile,
                      apps::mojom::AppType app_type,
                      const std::string& app_id,
                      const std::string& app_name);

  void InitializeCheckbox(const GURL& app_start_url);

  void InitializeViewForExtension(Profile* profile, const std::string& app_id);
  void InitializeViewForWebApp(Profile* profile, const std::string& app_id);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitializeViewForArcApp(Profile* profile, const std::string& app_id);
  void InitializeViewWithMessage(const std::u16string& message);
#endif

  void OnDialogCancelled();
  void OnDialogAccepted();

  Profile* profile_;

  views::Checkbox* report_abuse_checkbox_ = nullptr;
  views::Checkbox* clear_site_data_checkbox_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppUninstallDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
