// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

class Profile;

namespace ash {

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
  void OnWebContentsFinishedLoad() override;

  base::WeakPtr<CrostiniUpgraderUI> upgrader_ui_ = nullptr;  // Not owned.
  raw_ptr<Profile> profile_;                                 // Not owned
  const bool only_run_launch_closure_on_restart_;
  base::OnceClosure launch_closure_;
  base::OnceClosure deletion_closure_for_testing_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_UPGRADER_CROSTINI_UPGRADER_DIALOG_H_
