// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_view_controller.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

const ui::ImageModel GetMicImageModel() {
  const int icon_size = 20;
  const auto& icon = vector_icons::kMicChromeRefreshIcon;
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, icon_size);
}

std::vector<ui::SimpleComboboxModel::Item> GetComboboxItems(
    const std::vector<media::AudioDeviceDescription>& audio_source_infos) {
  std::vector<ui::SimpleComboboxModel::Item> items;
  items.reserve(audio_source_infos.size());
  for (const auto& info : audio_source_infos) {
    auto device_name = base::UTF8ToUTF16(info.device_name);
    const auto is_virtual_default_device =
        media::AudioDeviceDescription::IsDefaultDevice(info.unique_id);
    if (is_virtual_default_device) {
      device_name =
          l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC);
    }
    const auto secondary_text =
        info.is_system_default && !is_virtual_default_device
            ? l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC)
            : std::u16string();
    items.emplace_back(
        /*text=*/device_name,
        /*dropdown_secondary_text=*/secondary_text, /*icon=*/ui::ImageModel{});
  }
  return items;
}

}  // namespace

MicViewController::MicViewController(
    MediaView& base_view,
    bool needs_borders,
    ui::SimpleComboboxModel& combobox_model,
    bool allow_device_selection,
    MediaViewControllerBase::SourceChangeCallback callback,
    media_preview_metrics::Context metrics_context)
    : combobox_model_(combobox_model) {
  const auto& combobox_accessible_name =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_MIC_ACCESSIBLE_NAME);
  const auto& no_devices_found_combobox_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_MICS_FOUND_COMBOBOX);
  const auto& no_devices_found_label_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_MICS_FOUND);

  base_controller_ = std::make_unique<MediaViewControllerBase>(
      base_view, needs_borders, &combobox_model, std::move(callback),
      combobox_accessible_name, no_devices_found_combobox_text,
      no_devices_found_label_text, allow_device_selection, metrics_context);

  auto& container = GetLiveFeedContainer();
  container.SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container.SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  container.SetDefaultFlex(1);

  auto* icon_view = container.AddChildView(
      std::make_unique<views::ImageView>(GetMicImageModel()));
  icon_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  container.SetFlexForView(icon_view, 0);
}

MicViewController::~MicViewController() = default;

MediaView& MicViewController::GetLiveFeedContainer() {
  return base_controller_->GetLiveFeedContainer();
}

void MicViewController::UpdateAudioSourceInfos(
    const std::vector<media::AudioDeviceDescription>& audio_source_infos) {
  auto audio_source_info_count = audio_source_infos.size();
  combobox_model_->UpdateItemList(GetComboboxItems(audio_source_infos));
  base_controller_->OnDeviceListChanged(audio_source_info_count);
}
