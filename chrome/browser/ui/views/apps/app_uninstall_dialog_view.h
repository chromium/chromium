// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_UNINSTALL_DIALOG_VIEW_H_

#include <memory>
#include "base/macros.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace views {
class Checkbox;
}  // namespace views

namespace gfx {
class ImageSkia;
}

// Currently, app uninstallation on Chrome OS invokes a specific dialog per app
// type:
// - Chrome Apps / PWAs:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/app_list/extension_uninstaller.h?q=extensionuninstaller&sq=package:chromium&l=17
// - ARC apps:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/app_list/arc/arc_app_dialog.h?q=arcappunin&sq=package:chromium&l=21
// - Crostini:
// https://cs.chromium.org/chromium/src/chrome/browser/chromeos/crostini/crostini_util.h?type=cs&q=crostiniuninstall&sq=package:chromium&g=0&l=131
//
// There are 3 separate views for app uninstalling, which are subtly different
// from each other.
//
// This class combines the above three specific dialogs, and generates the
// correct UI based on the app type. Once the user has confirmed the uninstall,
// this class calls the parent class apps::UninstallDialog::UiBase to notify
// AppService, which transfers control to the publisher to uninstall the app.
//
// TODO(crbug.com/1009248):
// 1. Add an interface to the uninstall, like what is done by
// extension_uninstall_dialog_->ConfirmUninstallByExtension.
class AppUninstallDialogView : public apps::UninstallDialog::UiBase,
                               public views::BubbleDialogDelegateView {
 public:
  AppUninstallDialogView(Profile* profile,
                         apps::mojom::AppType app_type,
                         const std::string& app_id,
                         const std::string& app_name,
                         gfx::ImageSkia image,
                         apps::UninstallDialog* uninstall_dialog);
  ~AppUninstallDialogView() override = default;

  // views::BubbleDialogDelegateView:
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  gfx::ImageSkia GetWindowIcon() override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowIcon() const override;

 private:
  void AddMultiLineLabel(views::View* parent, const base::string16& label_text);
  void InitializeViewForExtension(Profile* profile, const std::string& app_id);
#if defined(OS_CHROMEOS)
  void InitializeViewForArcApp(Profile* profile, const std::string& app_id);
  void InitializeViewForCrostiniApp(Profile* profile,
                                    const std::string& app_id);
#endif
  void InitializeView(Profile* profile,
                      const std::string& app_id);

  // The type of apps, e.g. Extension-backed app, Android app.
  apps::mojom::AppType app_type_;

  // The name of apps, e.g. Camera.
  const std::string app_name_;

  // Whether app represents a shortcut. |shortcut_| is available for the ARC
  // apps only.
  bool shortcut_ = false;

  views::Checkbox* report_abuse_checkbox_ = nullptr;
  views::Checkbox* clear_site_data_checkbox_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppUninstallDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_UNINSTALL_DIALOG_VIEW_H_
