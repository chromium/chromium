// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_view_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/mic_selector_combobox_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

const ui::ImageModel GetMicImageModel() {
  const int icon_size = features::IsChromeRefresh2023() ? 20 : 18;
  const auto& icon = features::IsChromeRefresh2023()
                         ? vector_icons::kMicChromeRefreshIcon
                         : vector_icons::kMicIcon;
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, icon_size);
}

}  // namespace

MicViewController::MicViewController(
    MediaView& base_view,
    bool needs_borders,
    MicSelectorComboboxModel& combobox_model,
    MediaViewControllerBase::SourceChangeCallback callback)
    : combobox_model_(combobox_model) {
  const auto& combobox_accessible_name =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_MIC_ACCESSIBLE_NAME);
  const auto& no_device_connected_label_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_MICS_FOUND);

  base_controller_ = std::make_unique<MediaViewControllerBase>(
      base_view, needs_borders, &combobox_model, std::move(callback),
      combobox_accessible_name, no_device_connected_label_text);

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
    std::vector<AudioSourceInfo> audio_source_infos) {
  bool has_devices = !audio_source_infos.empty();
  combobox_model_->UpdateDeviceList(std::move(audio_source_infos));
  base_controller_->AdjustComboboxEnabledState(has_devices);
}
