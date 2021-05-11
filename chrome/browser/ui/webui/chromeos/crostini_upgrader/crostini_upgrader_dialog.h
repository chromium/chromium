// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_

#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

class Profile;

namespace chromeos {

class CrostiniUpgraderUI;

class CrostiniUpgraderDialog : public SystemWebDialogDelegate {
 public:
  // If a restart of Crostini is not required, the launch closure can be
  // optionally dropped. e.g. when running the upgrader from Settings, if the
  // user cancels before starting the upgrade, the launching of a Terminal is
  // not desired. This contrasts to being propmpted for container upgrade from
  // The app launcher, in which case the user could opt not to upgrade and the
  // app should still be launched.
  static void Show(Profile* profile,
                   base::OnceClosure launch_closure,
                   bool only_run_launch_closure_on_restart = false);

  // Returns true if there was an existing instance to reshow.
  static bool Reshow();

  void SetDeletionClosureForTesting(
      base::OnceClosure deletion_closure_for_testing);

  static void EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent event);

 private:
  explicit CrostiniUpgraderDialog(Profile* profile,
                                  base::OnceClosure launch_closure,
                                  bool only_run_launch_closure_on_restart);
  ~CrostiniUpgraderDialog() override;

  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCloseDialogOnEscape() const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  bool OnDialogCloseRequested() override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  void OnWebContentsFinishedLoad() override;

  CrostiniUpgraderUI* upgrader_ui_ = nullptr;  // Not owned.
  Profile* profile_;                           // Not owned
  const bool only_run_launch_closure_on_restart_;
  base::OnceClosure launch_closure_;
  base::OnceClosure deletion_closure_for_testing_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_
