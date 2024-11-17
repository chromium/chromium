// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_audio_output/select_audio_output_dialog.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/widget/widget.h"

SelectAudioOutputDialog::SelectAudioOutputDialog(
    const std::vector<content::AudioOutputDeviceInfo>& audio_output_devices,
    content::SelectAudioOutputCallback callback)
    : audio_output_devices_(audio_output_devices),
      callback_(std::move(callback)) {
  SetTitle(l10n_util::GetStringUTF16(IDS_SELECT_AUDIO_OUTPUT_DIALOG_TITLE));
  SetModalType(ui::mojom::ModalType::kChild);

  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  auto description_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SELECT_AUDIO_OUTPUT_DIALOG_CHOOSE_DEVICE));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetFontList(views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  AddChildView(std::move(description_label));
  SetShowCloseButton(false);
  std::vector<ui::SimpleComboboxModel::Item> combobox_items;
  for (const auto& device : audio_output_devices_) {
    combobox_items.emplace_back(base::UTF8ToUTF16(device.label));
  }
  combobox_model_ =
      std::make_unique<ui::SimpleComboboxModel>(std::move(combobox_items));

  combobox_ =
      AddChildView(std::make_unique<views::Combobox>(combobox_model_.get()));

  combobox_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_SELECT_AUDIO_OUTPUT_DIALOG_TITLE));

  SetAcceptCallback(base::BindOnce(&SelectAudioOutputDialog::OnAccept,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&SelectAudioOutputDialog::OnCancel,
                                   base::Unretained(this)));
}

void SelectAudioOutputDialog::OnAccept() {
  std::optional<size_t> selected_index = combobox_->GetSelectedRow();

  if (!selected_index.has_value()) {
    OnDeviceSelected(
        base::unexpected(content::SelectAudioOutputError::kUserCancelled));
    return;
  }

  if (*selected_index >= audio_output_devices_.size()) {
    OnDeviceSelected(
        base::unexpected(content::SelectAudioOutputError::kOtherError));
    return;
  }

  OnDeviceSelected(audio_output_devices_[*selected_index].device_id);
}

void SelectAudioOutputDialog::OnDeviceSelected(
    base::expected<std::string, content::SelectAudioOutputError> result) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result)));
  GetWidget()->Close();
}

SelectAudioOutputDialog::~SelectAudioOutputDialog() {}

void SelectAudioOutputDialog::OnCancel() {
  OnDeviceSelected(
      base::unexpected(content::SelectAudioOutputError::kUserCancelled));
}
