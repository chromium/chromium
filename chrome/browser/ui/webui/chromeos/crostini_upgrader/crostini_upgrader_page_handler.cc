// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_page_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

CrostiniUpgraderPageHandler::CrostiniUpgraderPageHandler(
    content::WebContents* web_contents,
    crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate,
    mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page> pending_page,
    base::OnceClosure on_page_closed,
    base::OnceCallback<void(bool)> launch_callback)
    : web_contents_{web_contents},
      upgrader_ui_delegate_{upgrader_ui_delegate},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      on_page_closed_{std::move(on_page_closed)},
      launch_callback_{std::move(launch_callback)} {
  upgrader_ui_delegate_->AddObserver(this);
}

CrostiniUpgraderPageHandler::~CrostiniUpgraderPageHandler() {
  upgrader_ui_delegate_->RemoveObserver(this);
}

namespace {

void Redisplay() {
  CrostiniUpgraderDialog::Reshow();
}

}  // namespace

void CrostiniUpgraderPageHandler::OnBackupMaybeStarted(bool did_start) {
  Redisplay();
}

// Send a close request to the web page.
void CrostiniUpgraderPageHandler::RequestClosePage() {
  page_->RequestClose();
}

void CrostiniUpgraderPageHandler::Backup(bool show_file_chooser) {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kDidBackup);
  upgrader_ui_delegate_->Backup(crostini::ContainerId::GetDefault(),
                                show_file_chooser, web_contents_);
}

void CrostiniUpgraderPageHandler::StartPrechecks() {
  upgrader_ui_delegate_->StartPrechecks();
}

void CrostiniUpgraderPageHandler::Upgrade() {
  Redisplay();
  upgrader_ui_delegate_->Upgrade(crostini::ContainerId::GetDefault());
}

void CrostiniUpgraderPageHandler::Restore() {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kDidRestore);
  upgrader_ui_delegate_->Restore(crostini::ContainerId::GetDefault(),
                                 web_contents_);
}

void CrostiniUpgraderPageHandler::Cancel() {
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kUpgradeCanceled);
  upgrader_ui_delegate_->Cancel();
}

void CrostiniUpgraderPageHandler::Launch() {
  std::move(launch_callback_).Run(restart_required_);
}

void CrostiniUpgraderPageHandler::CancelBeforeStart() {
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kNotStarted);
  restart_required_ = false;
  upgrader_ui_delegate_->CancelBeforeStart();
  if (launch_callback_) {
    // Running launch closure - no upgrade wanted, no need to restart crostini.
    Launch();
  }
}

void CrostiniUpgraderPageHandler::OnPageClosed() {
  if (launch_callback_) {
    Launch();
  }
  if (on_page_closed_) {
    std::move(on_page_closed_).Run();
  }
}

void CrostiniUpgraderPageHandler::OnUpgradeProgress(
    const std::vector<std::string>& messages) {
  page_->OnUpgradeProgress(messages);
}

void CrostiniUpgraderPageHandler::OnUpgradeSucceeded() {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kUpgradeSuccess);
  page_->OnUpgradeSucceeded();
}

void CrostiniUpgraderPageHandler::OnUpgradeFailed() {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kUpgradeFailed);
  page_->OnUpgradeFailed();
}

void CrostiniUpgraderPageHandler::OnBackupProgress(int percent) {
  page_->OnBackupProgress(percent);
}

void CrostiniUpgraderPageHandler::OnBackupSucceeded(bool was_cancelled) {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kBackupSucceeded);
  page_->OnBackupSucceeded(was_cancelled);
}

void CrostiniUpgraderPageHandler::OnBackupFailed() {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kBackupFailed);
  page_->OnBackupFailed();
}

void CrostiniUpgraderPageHandler::PrecheckStatus(
    chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus status) {
  page_->PrecheckStatus(status);
}

void CrostiniUpgraderPageHandler::OnRestoreProgress(int percent) {
  page_->OnRestoreProgress(percent);
}

void CrostiniUpgraderPageHandler::OnRestoreSucceeded() {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kRestoreSucceeded);
  page_->OnRestoreSucceeded();
}

void CrostiniUpgraderPageHandler::OnRestoreFailed() {
  Redisplay();
  CrostiniUpgraderDialog::EmitUpgradeDialogEventHistogram(
      crostini::UpgradeDialogEvent::kRestoreFailed);
  page_->OnRestoreFailed();
}

void CrostiniUpgraderPageHandler::OnCanceled() {
  page_->OnCanceled();
}

}  // namespace chromeos
