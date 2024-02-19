// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_dialog.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
// The dialog content area size. Note that the height is less than the design
// spec to compensate for title bar height.
constexpr int kDialogWidth = 768;
constexpr int kDialogHeight = 608;

GURL GetUrl() {
  return GURL{chrome::kChromeUICrostiniUpgraderUrl};
}
}  // namespace

namespace ash {

void CrostiniUpgraderDialog::Show(Profile* profile,
                                  base::OnceClosure launch_closure,
                                  bool only_run_launch_closure_on_restart) {
  if (Reshow()) {
    return;
  }

  auto* instance = new CrostiniUpgraderDialog(
      profile, std::move(launch_closure), only_run_launch_closure_on_restart);
  instance->ShowSystemDialog();
  EmitUpgradeDialogEventHistogram(crostini::UpgradeDialogEvent::kDialogShown);
}

bool CrostiniUpgraderDialog::Reshow() {
  auto* instance = SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    instance->Focus();
    return true;
  }
  return false;
}

CrostiniUpgraderDialog::CrostiniUpgraderDialog(
    Profile* profile,
    base::OnceClosure launch_closure,
    bool only_run_launch_closure_on_restart)
    : SystemWebDialogDelegate(GetUrl(), /*title=*/{}),
      profile_(profile),
      only_run_launch_closure_on_restart_(only_run_launch_closure_on_restart),
      launch_closure_{std::move(launch_closure)} {
  DCHECK(profile_);
  crostini::CrostiniManager::GetForProfile(profile_)->SetCrostiniDialogStatus(
      crostini::DialogType::UPGRADER, true);
  set_can_minimize(true);
  set_can_resize(false);
}

CrostiniUpgraderDialog::~CrostiniUpgraderDialog() {
  if (deletion_closure_for_testing_) {
    std::move(deletion_closure_for_testing_).Run();
  }
  crostini::CrostiniManager::GetForProfile(profile_)->SetCrostiniDialogStatus(
      crostini::DialogType::UPGRADER, false);
}

void CrostiniUpgraderDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

bool CrostiniUpgraderDialog::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniUpgraderDialog::ShouldShowDialogTitle() const {
  return true;
}

bool CrostiniUpgraderDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

void CrostiniUpgraderDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;

  const ShelfID shelf_id(Id());
  params->init_properties_container.SetProperty(kShelfIDKey,
                                                shelf_id.Serialize());
  params->init_properties_container.SetProperty<int>(kShelfItemTypeKey,
                                                     TYPE_DIALOG);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  params->init_properties_container.SetProperty(
      aura::client::kAppIconKey,
      rb.GetImageNamed(IDR_LOGO_CROSTINI_DEFAULT).AsImageSkia());
}

void CrostiniUpgraderDialog::SetDeletionClosureForTesting(
    base::OnceClosure deletion_closure_for_testing) {
  deletion_closure_for_testing_ = std::move(deletion_closure_for_testing);
}

bool CrostiniUpgraderDialog::OnDialogCloseRequested() {
  if (deletion_closure_for_testing_) {
    // Running in a test.
    return true;
  }
  return !upgrader_ui_ || upgrader_ui_->RequestClosePage();
}

namespace {
void RunLaunchClosure(base::WeakPtr<crostini::CrostiniManager> crostini_manager,
                      base::OnceClosure launch_closure,
                      bool only_run_launch_closure_on_restart,
                      bool restart_required) {
  if (!crostini_manager) {
    return;
  }
  if (!restart_required) {
    if (!only_run_launch_closure_on_restart) {
      std::move(launch_closure).Run();
    }
    return;
  }
  crostini_manager->RestartCrostini(
      crostini::DefaultContainerId(),
      base::BindOnce(
          [](base::OnceClosure launch_closure,
             crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              LOG(ERROR)
                  << "Failed to restart crostini after upgrade. Error code: "
                  << static_cast<int>(result);
              return;
            }
            if (launch_closure.is_null() || launch_closure.IsCancelled()) {
              return;
            }
            std::move(launch_closure).Run();
          },
          std::move(launch_closure)));
}
}  // namespace

void CrostiniUpgraderDialog::OnDialogShown(content::WebUI* webui) {
  upgrader_ui_ =
      static_cast<CrostiniUpgraderUI*>(webui->GetController())->GetWeakPtr();
  upgrader_ui_->set_launch_callback(base::BindOnce(
      &RunLaunchClosure,
      crostini::CrostiniManager::GetForProfile(profile_)->GetWeakPtr(),
      std::move(launch_closure_), only_run_launch_closure_on_restart_));
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
    crostini::UpgradeDialogEvent event) {
  base::UmaHistogramEnumeration("Crostini.UpgradeDialogEvent", event);
}

void CrostiniUpgraderDialog::OnWebContentsFinishedLoad() {
  DCHECK(dialog_window());
  dialog_window()->SetTitle(
      l10n_util::GetStringUTF16(IDS_CROSTINI_UPGRADER_TITLE));
}

}  // namespace ash
