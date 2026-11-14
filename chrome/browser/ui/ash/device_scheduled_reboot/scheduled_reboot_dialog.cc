// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/device_scheduled_reboot/scheduled_reboot_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

ScheduledRebootDialog::ScheduledRebootDialog(const base::Time& reboot_time,
                                             gfx::NativeView native_view,
                                             base::OnceClosure reboot_callback)
    : title_refresh_timer_(
          reboot_time,
          base::BindRepeating(&ScheduledRebootDialog::UpdateWindowTitle,
                              base::Unretained(this))) {
  ShowBubble(reboot_time, native_view, std::move(reboot_callback));
}

ScheduledRebootDialog::~ScheduledRebootDialog() {
  if (widget_) {
    widget_->CloseNow();
  }
}

void ScheduledRebootDialog::ShowBubble(const base::Time& reboot_time,
                                       gfx::NativeView native_view,
                                       base::OnceClosure reboot_callback) {
  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(BuildTitle())
          .AddOkButton(base::DoNothing())
          .AddCancelButton(
              std::move(reboot_callback),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(IDS_POLICY_REBOOT_BUTTON)))
          .AddParagraph(
              ui::DialogModelLabel(
                  l10n_util::GetStringFUTF16(
                      IDS_POLICY_DEVICE_SCHEDULED_REBOOT_DIALOG_MESSAGE,
                      base::TimeFormatTimeOfDay(reboot_time),
                      base::TimeFormatShortDate(reboot_time)))
                  .set_is_secondary())
          .Build();
  // TODO(crbug.com/41493925): Autosizing this bubble once this bubble's anchor
  // point is in the correct position.
  auto bubble = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kSystem,
      /*autosize=*/false);
  bubble->SetOwnedByWidget(views::WidgetDelegate::OwnedByWidgetPassKey());
  bubble->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget_ = base::WrapUnique(constrained_window::CreateBrowserModalDialogViews(
      std::move(bubble), native_view));
  widget_->MakeCloseSynchronous(base::BindOnce(
      &ScheduledRebootDialog::CloseWidget, weak_ptr_factory_.GetWeakPtr()));
  widget_->Show();
}

views::DialogDelegate* ScheduledRebootDialog::GetDialogDelegate() const {
  return widget_ ? widget_->widget_delegate()->AsDialogDelegate() : nullptr;
}

void ScheduledRebootDialog::CloseWidget(views::Widget::ClosedReason reason) {
  widget_.reset();
}

void ScheduledRebootDialog::UpdateWindowTitle() {
  if (widget_) {
    widget_->widget_delegate()->AsDialogDelegate()->SetTitle(BuildTitle());
  }
}

const std::u16string ScheduledRebootDialog::BuildTitle() const {
  const base::TimeDelta rounded_offset =
      title_refresh_timer_.GetRoundedDeadlineDelta();
  int amount = rounded_offset.InSeconds();
  int message_id = IDS_REBOOT_SCHEDULED_TITLE_SECONDS;
  if (rounded_offset.InMinutes() >= 1) {
    amount = rounded_offset.InMinutes();
    message_id = IDS_REBOOT_SCHEDULED_TITLE_MINUTES;
  }
  return l10n_util::GetPluralStringFUTF16(message_id, amount);
}
