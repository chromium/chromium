// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_recovery_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

CrostiniRecoveryView* g_crostini_recovery_view = nullptr;

constexpr char kCrostiniRecoverySourceHistogram[] = "Crostini.RecoverySource";

}  // namespace

void crostini::ShowCrostiniRecoveryView(
    Profile* profile,
    crostini::CrostiniUISurface ui_surface,
    const std::string& app_id,
    int64_t display_id,
    const std::vector<guest_os::LaunchArg>& args,
    crostini::CrostiniSuccessCallback callback) {
  CrostiniRecoveryView::Show(profile, app_id, display_id, args,
                             std::move(callback));
  base::UmaHistogramEnumeration(kCrostiniRecoverySourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);
}

void CrostiniRecoveryView::Show(Profile* profile,
                                const std::string& app_id,
                                int64_t display_id,
                                const std::vector<guest_os::LaunchArg>& args,
                                crostini::CrostiniSuccessCallback callback) {
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    std::move(callback).Run(false, "crostini is not allowed");
    return;
  }

  // Any new apps launched during recovery are immediately cancelled.
  if (g_crostini_recovery_view) {
    std::move(callback).Run(false, "recovery in progress");
  } else {
    g_crostini_recovery_view = new CrostiniRecoveryView(
        profile, app_id, display_id, args, std::move(callback));
    CreateDialogWidget(g_crostini_recovery_view, nullptr, nullptr);
  }
  // Always call Show to bring the dialog to the front of the screen.
  g_crostini_recovery_view->GetWidget()->Show();
}

bool CrostiniRecoveryView::Accept() {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  SetButtonEnabled(ui::mojom::DialogButton::kCancel, false);
  crostini::CrostiniManager::GetForProfile(profile_)->StopVm(
      crostini::kCrostiniDefaultVmName,
      base::BindOnce(&CrostiniRecoveryView::OnStopVm,
                     weak_ptr_factory_.GetWeakPtr()));
  DialogModelChanged();
  return false;
}

void CrostiniRecoveryView::OnStopVm(crostini::CrostiniResult result) {
  VLOG(1) << "Scheduling app launch " << app_id_;
  if (result != crostini::CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Error stopping VM for recovery: " << (int)result;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&crostini::LaunchCrostiniApp, profile_, app_id_,
                                display_id_, args_, std::move(callback_)));
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

bool CrostiniRecoveryView::Cancel() {
  if (callback_) {
    std::move(callback_).Run(false, "cancelled for recovery");
    guest_os::LaunchTerminal(profile_, display_id_,
                             crostini::DefaultContainerId());
  }
  return true;
}

// static
CrostiniRecoveryView* CrostiniRecoveryView::GetActiveViewForTesting() {
  return g_crostini_recovery_view;
}

CrostiniRecoveryView::CrostiniRecoveryView(
    Profile* profile,
    const std::string& app_id,
    int64_t display_id,
    const std::vector<guest_os::LaunchArg>& args,
    crostini::CrostiniSuccessCallback callback)
    : profile_(profile),
      app_id_(app_id),
      display_id_(display_id),
      args_(args),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_RESTART_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_TERMINAL_BUTTON));
  SetShowCloseButton(false);
  SetTitle(IDS_CROSTINI_RECOVERY_TITLE);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_MESSAGE);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);
}

CrostiniRecoveryView::~CrostiniRecoveryView() {
  g_crostini_recovery_view = nullptr;
}

BEGIN_METADATA(CrostiniRecoveryView)
END_METADATA
