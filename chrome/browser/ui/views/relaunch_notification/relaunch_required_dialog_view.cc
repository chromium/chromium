// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
views::Widget* RelaunchRequiredDialogView::Show(
    Browser* browser,
    base::Time deadline,
    base::RepeatingClosure on_accept) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      new RelaunchRequiredDialogView(deadline, std::move(on_accept)),
      browser->window()->GetNativeWindow());
  widget->Show();
  return widget;
}

RelaunchRequiredDialogView::~RelaunchRequiredDialogView() = default;

// static
RelaunchRequiredDialogView* RelaunchRequiredDialogView::FromWidget(
    views::Widget* widget) {
  return static_cast<RelaunchRequiredDialogView*>(
      widget->widget_delegate()->AsDialogDelegate());
}

void RelaunchRequiredDialogView::SetDeadline(base::Time deadline) {
  relaunch_required_timer_.SetDeadline(deadline);
}

std::u16string RelaunchRequiredDialogView::GetWindowTitle() const {
  // Round the time-to-relaunch to the nearest "boundary", which may be a day,
  // hour, minute, or second. For example, two days and eighteen hours will be
  // rounded up to three days, while two days and one hour will be rounded down
  // to two days. This rounding is significant for only the initial showing of
  // the dialog. Each refresh of the title thereafter will take place at the
  // moment when the boundary value changes. For example, the title will be
  // refreshed from three days to two days when there are exactly two days
  // remaining. This scales nicely to the final seconds, when one would expect a
  // "3..2..1.." countdown to change precisely on the per-second boundaries.
  const base::TimeDelta rounded_offset =
      relaunch_required_timer_.GetRoundedDeadlineDelta();
  DCHECK_GE(rounded_offset, base::TimeDelta());
  int amount = rounded_offset.InSeconds();
  int message_id = IDS_RELAUNCH_REQUIRED_TITLE_SECONDS;
  if (rounded_offset.InDays() >= 2) {
    amount = rounded_offset.InDays();
    message_id = IDS_RELAUNCH_REQUIRED_TITLE_DAYS;
  } else if (rounded_offset.InHours() >= 1) {
    amount = rounded_offset.InHours();
    message_id = IDS_RELAUNCH_REQUIRED_TITLE_HOURS;
  } else if (rounded_offset.InMinutes() >= 1) {
    amount = rounded_offset.InMinutes();
    message_id = IDS_RELAUNCH_REQUIRED_TITLE_MINUTES;
  }

  return l10n_util::GetPluralStringFUTF16(message_id, amount);
}

ui::ImageModel RelaunchRequiredDialogView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

// |relaunch_required_timer_| automatically starts for the next time the title
// needs to be updated (e.g., from "2 days" to "3 days").
RelaunchRequiredDialogView::RelaunchRequiredDialogView(
    base::Time deadline,
    base::RepeatingClosure on_accept)
    : relaunch_required_timer_(
          deadline,
          base::BindRepeating(&RelaunchRequiredDialogView::UpdateWindowTitle,
                              base::Unretained(this))) {
  set_internal_name("RelaunchRequiredDialog");
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_RELAUNCH_ACCEPT_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_RELAUNCH_REQUIRED_CANCEL_BUTTON));
  SetShowIcon(true);
  SetAcceptCallback(base::BindOnce(
      [](base::RepeatingClosure callback) {
        base::RecordAction(base::UserMetricsAction("RelaunchRequired_Accept"));
        callback.Run();
      },
      on_accept));
  SetCancelCallback(base::BindOnce(
      base::RecordAction, base::UserMetricsAction("RelaunchRequired_Close")));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  auto label = std::make_unique<views::Label>(
      l10n_util::GetPluralStringFUTF16(IDS_RELAUNCH_REQUIRED_BODY,
                                       BrowserList::GetIncognitoBrowserCount()),
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Align the body label with the left edge of the dialog's title.
  // TODO(bsep): Remove this when fixing https://crbug.com/810970.
  const int title_offset =
      2 * provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left() +
      provider->GetDistanceMetric(DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE);
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, title_offset - margins().left(), 0, 0)));

  AddChildView(std::move(label));

  base::RecordAction(base::UserMetricsAction("RelaunchRequiredShown"));
}

void RelaunchRequiredDialogView::UpdateWindowTitle() {
  GetWidget()->UpdateWindowTitle();
}
