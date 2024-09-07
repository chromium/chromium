// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/arc/arc_vm_data_migration_confirmation_dialog.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace arc {

namespace {

constexpr char kInternalName[] = "ArcVmDataMigrationConfirmationDialog";

constexpr int kDialogCornerRadius = 12;

void ReportConfirmationDialogShown(int days_until_deadline) {
  base::UmaHistogramExactLinear(
      "Arc.VmDataMigration.RemainingDays.ConfirmationDialogShown",
      days_until_deadline, kArcVmDataMigrationNumberOfDismissibleDays);
}

void ReportConfirmationDialogButtonClicked(int days_until_deadline,
                                           bool accepted) {
  base::UmaHistogramExactLinear(
      base::StringPrintf("Arc.VmDataMigration.RemainingDays.%s",
                         accepted ? "ConfirmationDialogAccepted"
                                  : "ConfirmationDialogCanceled"),
      days_until_deadline, kArcVmDataMigrationNumberOfDismissibleDays);
}

}  // namespace

ArcVmDataMigrationConfirmationDialog::ArcVmDataMigrationConfirmationDialog(
    PrefService* prefs,
    ArcVmDataMigrationConfirmationCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(prefs);
  set_internal_name(kInternalName);

  const int days_until_deadline = GetDaysUntilArcVmDataMigrationDeadline(prefs);
  ReportConfirmationDialogShown(days_until_deadline);

  if (ArcVmDataMigrationShouldBeDismissible(days_until_deadline)) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
               static_cast<int>(ui::mojom::DialogButton::kCancel));
    SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(
            IDS_ARC_VM_DATA_MIGRATION_DIALOG_UPDATE_NOW_BUTTON_LABEL));
    SetButtonLabel(ui::mojom::DialogButton::kCancel,
                   l10n_util::GetStringUTF16(
                       IDS_ARC_VM_DATA_MIGRATION_DIALOG_SKIP_BUTTON_LABEL));
    SetAcceptCallback(
        base::BindOnce(&ArcVmDataMigrationConfirmationDialog::OnButtonClicked,
                       weak_ptr_factory_.GetWeakPtr(), days_until_deadline,
                       true /* accepted */));
    SetCancelCallback(
        base::BindOnce(&ArcVmDataMigrationConfirmationDialog::OnButtonClicked,
                       weak_ptr_factory_.GetWeakPtr(), days_until_deadline,
                       false /* accepted */));
  } else {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(
                       IDS_ARC_VM_DATA_MIGRATION_DIALOG_UPDATE_BUTTON_LABEL));
    SetAcceptCallback(
        base::BindOnce(&ArcVmDataMigrationConfirmationDialog::OnButtonClicked,
                       weak_ptr_factory_.GetWeakPtr(), days_until_deadline,
                       true /* accepted */));
  }

  InitializeView(days_until_deadline);

  // Not system modal so that the user can interact with apps before restart.
  SetModalType(ui::mojom::ModalType::kNone);
  SetOwnedByWidget(true);
  SetShowCloseButton(false);

  const auto* layout_provider = ChromeLayoutProvider::Get();
  DCHECK(layout_provider);
  set_fixed_width(layout_provider->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_use_round_corners(true);
  set_corner_radius(kDialogCornerRadius);
  set_margins(layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
}

ArcVmDataMigrationConfirmationDialog::~ArcVmDataMigrationConfirmationDialog() =
    default;

void ArcVmDataMigrationConfirmationDialog::InitializeView(
    int days_until_deadline) {
  auto view = std::make_unique<views::View>();

  const auto* layout_provider = ChromeLayoutProvider::Get();
  DCHECK(layout_provider);
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG));
  view->SetLayoutManager(std::move(layout));

  view->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              kSaveIcon, ash::AshColorProvider::Get()->GetContentLayerColor(
                             ash::AshColorProvider::ContentLayerType::
                                 kIconColorProminent)))
          .SetHorizontalAlignment(views::ImageView::Alignment::kLeading)
          .Build());

  view->AddChildView(
      views::Builder<views::Label>()
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(layout_provider->GetDistanceMetric(
                                    views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                                0, 0, 0))
          .SetText(
              l10n_util::GetStringUTF16(IDS_ARC_VM_DATA_MIGRATION_DIALOG_TITLE))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetFontList(views::TypographyProvider::Get()
                           .GetFont(views::style::CONTEXT_DIALOG_TITLE,
                                    views::style::STYLE_PRIMARY)
                           .DeriveWithWeight(gfx::Font::Weight::BOLD))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());

  view->AddChildView(
      views::Builder<views::Label>()
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(layout_provider->GetDistanceMetric(
                                    views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                                0, 0, 0))
          .SetText(l10n_util::GetStringUTF16(
              IDS_ARC_VM_DATA_MIGRATION_DIALOG_SAVE_WORK_MESSAGE))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());

  view->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetPluralStringFUTF16(
              IDS_ARC_VM_DATA_MIGRATION_DIALOG_DAYS_UNTIL_DEADLINE,
              days_until_deadline))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(ChromeTextStyle::STYLE_RED)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());

  SetContentsView(std::move(view));
}

void ArcVmDataMigrationConfirmationDialog::OnButtonClicked(
    int days_until_deadline,
    bool accepted) {
  DCHECK(!callback_.is_null());
  ReportConfirmationDialogButtonClicked(days_until_deadline, accepted);
  std::move(callback_).Run(accepted);
}

void ShowArcVmDataMigrationConfirmationDialog(
    PrefService* prefs,
    ArcVmDataMigrationConfirmationCallback callback) {
  views::DialogDelegate::CreateDialogWidget(
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          prefs, std::move(callback)),
      nullptr /* context */, nullptr /* parent */)
      ->Show();
}

}  // namespace arc
