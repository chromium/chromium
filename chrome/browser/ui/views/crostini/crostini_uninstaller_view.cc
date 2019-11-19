// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

CrostiniUninstallerView* g_crostini_uninstaller_view = nullptr;

constexpr char kCrostiniUninstallResultHistogram[] = "Crostini.UninstallResult";
constexpr char kCrostiniUninstallSourceHistogram[] = "Crostini.UninstallSource";

}  // namespace

void crostini::ShowCrostiniUninstallerView(
    Profile* profile,
    crostini::CrostiniUISurface ui_surface) {
  base::UmaHistogramEnumeration(kCrostiniUninstallSourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);
  return CrostiniUninstallerView::Show(profile);
}

void CrostiniUninstallerView::Show(Profile* profile) {
  DCHECK(crostini::CrostiniFeatures::Get()->IsUIAllowed(profile));
  if (!g_crostini_uninstaller_view) {
    g_crostini_uninstaller_view = new CrostiniUninstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_crostini_uninstaller_view,
                                              nullptr, nullptr);
  }
  g_crostini_uninstaller_view->GetWidget()->Show();
}

int CrostiniUninstallerView::GetDialogButtons() const {
  switch (state_) {
    case State::PROMPT:
      return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
    case State::UNINSTALLING:
      return ui::DIALOG_BUTTON_NONE;
    case State::ERROR:
      return ui::DIALOG_BUTTON_CANCEL;
  }
  NOTREACHED();
  return 0;
}

base::string16 CrostiniUninstallerView::GetWindowTitle() const {
  const base::string16 device_type = ui::GetChromeOSDeviceName();
  return l10n_util::GetStringUTF16(IDS_CROSTINI_UNINSTALLER_TITLE);
}

bool CrostiniUninstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniUninstallerView::Accept() {
  state_ = State::UNINSTALLING;
  message_label_->SetText(
      l10n_util::GetStringUTF16(IDS_CROSTINI_UNINSTALLER_UNINSTALLING_MESSAGE));

  // Kick off the Crostini Remove sequence.
  crostini::CrostiniManager::GetForProfile(profile_)->RemoveCrostini(
      crostini::kCrostiniDefaultVmName,
      base::BindOnce(&CrostiniUninstallerView::UninstallCrostiniFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);
  // Setting value to -1 makes the progress bar play the
  // "indeterminate animation".
  progress_bar_->SetValue(-1);
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  return false;  // Should not close the dialog
}

bool CrostiniUninstallerView::Cancel() {
  RecordUninstallResultHistogram(UninstallResult::kCancelled);
  return true;  // Should close the dialog
}

gfx::Size CrostiniUninstallerView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

// static
CrostiniUninstallerView* CrostiniUninstallerView::GetActiveViewForTesting() {
  return g_crostini_uninstaller_view;
}

CrostiniUninstallerView::CrostiniUninstallerView(Profile* profile)
    : profile_(profile) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_CROSTINI_UNINSTALLER_UNINSTALL_BUTTON));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const base::string16 device_type = ui::GetChromeOSDeviceName();
  const base::string16 message =
      l10n_util::GetStringFUTF16(IDS_CROSTINI_UNINSTALLER_BODY, device_type);
  message_label_ = new views::Label(message);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label_);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_UNINSTALLER);
}

CrostiniUninstallerView::~CrostiniUninstallerView() {
  g_crostini_uninstaller_view = nullptr;
}

void CrostiniUninstallerView::HandleError(const base::string16& error_message) {
  state_ = State::ERROR;
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);
  progress_bar_->SetVisible(false);
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  RecordUninstallResultHistogram(UninstallResult::kError);
}

void CrostiniUninstallerView::UninstallCrostiniFinished(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    HandleError(l10n_util::GetStringUTF16(IDS_CROSTINI_UNINSTALLER_ERROR));
    return;
  } else {
    RecordUninstallResultHistogram(UninstallResult::kSuccess);
  }
  GetWidget()->Close();
}

void CrostiniUninstallerView::RecordUninstallResultHistogram(
    UninstallResult result) {
  // Prevent multiple results being logged for a given uninstall flow. This
  // happens because Cancel is always called, either due to the user cancelling
  // or the dialog being closed. The simplest way to prevent metrics being
  // erroneously logged for user cancellation is to only record the first
  // metric.
  if (has_logged_result_)
    return;

  base::UmaHistogramEnumeration(kCrostiniUninstallResultHistogram, result,
                                UninstallResult::kCount);
  has_logged_result_ = true;
}
