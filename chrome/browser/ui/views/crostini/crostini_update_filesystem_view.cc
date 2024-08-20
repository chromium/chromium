// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_update_filesystem_view.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/crostini/crostini_update_filesystem_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

bool g_crostini_update_filesystem_should_skip_delay_for_testing = false;

CrostiniUpdateFilesystemView* g_crostini_update_filesystem_view_dialog =
    nullptr;
bool g_crostini_update_filesystem_should_show = false;
// The time to delay before showing the upgrade container dialog (to decrease
// flashiness).
constexpr base::TimeDelta kDelayBeforeUpgradeContainerDialog =
    base::Milliseconds(400);

constexpr char kCrostiniUpgradeContainerSourceHistogram[] =
    "Crostini.UpgradeContainerSource";

}  // namespace

namespace crostini {
void SetCrostiniUpdateFilesystemSkipDelayForTesting(bool should_skip) {
  g_crostini_update_filesystem_should_skip_delay_for_testing = should_skip;
}

void PrepareShowCrostiniUpdateFilesystemView(
    Profile* profile,
    crostini::CrostiniUISurface ui_surface) {
  g_crostini_update_filesystem_should_show = true;

  base::TimeDelta delay =
      g_crostini_update_filesystem_should_skip_delay_for_testing
          ? base::Milliseconds(0)
          : kDelayBeforeUpgradeContainerDialog;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShowCrostiniUpdateFilesystemView, profile, ui_surface),
      delay);
}

void ShowCrostiniUpdateFilesystemView(Profile* profile,
                                      crostini::CrostiniUISurface ui_surface) {
  if (g_crostini_update_filesystem_should_show) {
    base::UmaHistogramEnumeration(kCrostiniUpgradeContainerSourceHistogram,
                                  ui_surface,
                                  crostini::CrostiniUISurface::kCount);
    CrostiniUpdateFilesystemView::Show(profile);
  }
}

void CloseCrostiniUpdateFilesystemView() {
  if (g_crostini_update_filesystem_view_dialog) {
    g_crostini_update_filesystem_view_dialog->GetWidget()->Close();
  }
  g_crostini_update_filesystem_should_show = false;
}
}  // namespace crostini

void CrostiniUpdateFilesystemView::Show(Profile* profile) {
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    return;
  }

  if (!g_crostini_update_filesystem_view_dialog) {
    g_crostini_update_filesystem_view_dialog =
        new CrostiniUpdateFilesystemView();
    CreateDialogWidget(g_crostini_update_filesystem_view_dialog, nullptr,
                       nullptr);
  }
  g_crostini_update_filesystem_view_dialog->GetWidget()->Show();
}

// static
CrostiniUpdateFilesystemView*
CrostiniUpdateFilesystemView::GetActiveViewForTesting() {
  return g_crostini_update_filesystem_view_dialog;
}

CrostiniUpdateFilesystemView::CrostiniUpdateFilesystemView() {
  constexpr int kDialogSpacingVertical = 32;

  SetShowCloseButton(false);
  SetTitle(IDS_CROSTINI_UPGRADING_LABEL);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));

  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      kDialogSpacingVertical));

  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_UPGRADING_SUBTEXT);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);
}

CrostiniUpdateFilesystemView::~CrostiniUpdateFilesystemView() {
  g_crostini_update_filesystem_view_dialog = nullptr;
}

BEGIN_METADATA(CrostiniUpdateFilesystemView)
END_METADATA
