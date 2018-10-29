// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_upgrade_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

CrostiniUpgradeView* g_crostini_upgrade_view = nullptr;

constexpr char kCrostiniUpgradeSourceHistogram[] = "Crostini.UpgradeSource";

}  // namespace

void crostini::ShowCrostiniUpgradeView(Profile* profile,
                                       crostini::CrostiniUISurface ui_surface) {
  base::UmaHistogramEnumeration(kCrostiniUpgradeSourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);
  return CrostiniUpgradeView::Show(profile);
}

void CrostiniUpgradeView::Show(Profile* profile) {
  DCHECK(crostini::IsCrostiniUIAllowedForProfile(profile));
  if (!g_crostini_upgrade_view) {
    g_crostini_upgrade_view = new CrostiniUpgradeView;
    CreateDialogWidget(g_crostini_upgrade_view, nullptr, nullptr);
  }
  g_crostini_upgrade_view->GetWidget()->Show();
}

int CrostiniUpgradeView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 CrostiniUpgradeView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CROSTINI_TERMINA_UPDATE_REQUIRED);
}

bool CrostiniUpgradeView::ShouldShowCloseButton() const {
  return false;
}

gfx::Size CrostiniUpgradeView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

// static
CrostiniUpgradeView* CrostiniUpgradeView::GetActiveViewForTesting() {
  return g_crostini_upgrade_view;
}

CrostiniUpgradeView::CrostiniUpgradeView() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_TERMINA_UPDATE_OFFLINE);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_UPGRADE);
}

CrostiniUpgradeView::~CrostiniUpgradeView() {
  g_crostini_upgrade_view = nullptr;
}
