// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"
#include "components/services/app_service/public/cpp/app_types.h"
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
                         apps::AppType app_type,
                         const std::string& app_id,
                         const std::string& app_name,
                         gfx::ImageSkia image,
                         apps::UninstallDialog* uninstall_dialog);

  AppUninstallDialogView(const AppUninstallDialogView&) = delete;
  AppUninstallDialogView& operator=(const AppUninstallDialogView&) = delete;

  ~AppUninstallDialogView() override;

  static AppUninstallDialogView* GetActiveViewForTesting();

 private:
  void InitializeView(Profile* profile,
                      apps::AppType app_type,
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

  void OnWidgetInitialized() override;

  raw_ptr<Profile> profile_;

  raw_ptr<views::Checkbox> report_abuse_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> clear_site_data_checkbox_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
