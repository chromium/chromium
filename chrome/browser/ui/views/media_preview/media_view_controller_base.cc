// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"

namespace {

constexpr int kComboboxBorderThickness = 1;
constexpr int kRoundedRadius = 12;

void FormatDeviceNameLabel(const raw_ref<views::Label> device_name_label) {
  auto* provider = ChromeLayoutProvider::Get();
  const float label_radius = provider->GetCornerRadiusMetric(
      views::ShapeContextTokens::kComboboxRadius);
  const int horizontal_padding = provider->GetDistanceMetric(
      views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);
  const int vertical_padding =
      provider->GetDistanceMetric(
          DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING) /
          2 -
      kComboboxBorderThickness;
  const auto background_color_id = features::IsChromeRefresh2023()
                                       ? ui::kColorComboboxBackground
                                       : ui::kColorTextfieldBackground;
  const auto border_color_id = features::IsChromeRefresh2023()
                                   ? ui::kColorComboboxContainerOutline
                                   : ui::kColorFocusableBorderUnfocused;

  device_name_label->SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id, label_radius));
  device_name_label->SetBorder(views::CreatePaddedBorder(
      views::CreateThemedRoundedRectBorder(kComboboxBorderThickness,
                                           label_radius, border_color_id),
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
}

}  // namespace

MediaViewControllerBase::MediaViewControllerBase(
    MediaView& base_view,
    bool needs_borders,
    ui::ComboboxModel* model,
    SourceChangeCallback source_change_callback,
    const std::u16string& combobox_accessible_name,
    const std::u16string& no_devices_found_combobox_text,
    const std::u16string& no_devices_found_label_text,
    media_preview_metrics::Context metrics_context)
    : base_view_(base_view),
      live_feed_container_(raw_ref<MediaView>::from_ptr(
          base_view_->AddChildView(std::make_unique<MediaView>()))),
      no_devices_found_label_(raw_ref<views::Label>::from_ptr(
          base_view_->AddChildView(std::make_unique<views::Label>()))),
      device_name_label_(raw_ref<views::Label>::from_ptr(
          base_view_->AddChildView(std::make_unique<views::Label>()))),
      device_selector_combobox_(raw_ref<views::Combobox>::from_ptr(
          base_view_->AddChildView(std::make_unique<views::Combobox>(model)))),
      no_devices_found_combobox_text_(no_devices_found_combobox_text),
      source_change_callback_(std::move(source_change_callback)),
      metrics_context_(metrics_context) {
  CHECK(source_change_callback_);

  auto* provider = ChromeLayoutProvider::Get();
  base_view_->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  if (needs_borders) {
    const int kBorderThickness =
        provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

    base_view_->SetBorder(views::CreateThemedRoundedRectBorder(
        kBorderThickness, kRoundedRadius, ui::kColorMenuBackground));
    base_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorMenuBackground, kRoundedRadius));
  }

  live_feed_container_->SetVisible(false);

  no_devices_found_label_->SetText(no_devices_found_label_text);
  no_devices_found_label_->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  no_devices_found_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  device_name_label_->SetText(no_devices_found_combobox_text_);
  device_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  FormatDeviceNameLabel(device_name_label_);

  device_selector_combobox_->SetAccessibleName(combobox_accessible_name);
  device_selector_combobox_->SetSizeToLargestLabel(false);
  device_selector_combobox_->SetVisible(false);

  // Unretained is safe, because `this` outlives `device_selector_combobox_`.
  device_selector_combobox_->SetCallback(base::BindRepeating(
      &MediaViewControllerBase::OnComboboxSelection, base::Unretained(this)));
}

MediaViewControllerBase::~MediaViewControllerBase() {
  device_selector_combobox_->SetCallback({});
}

void MediaViewControllerBase::OnDeviceListChanged(size_t device_count) {
  bool has_devices = device_count > 0;
  live_feed_container_->SetVisible(has_devices);
  no_devices_found_label_->SetVisible(!has_devices);
  UpdateDeviceNameLabel(has_devices);
  // `device_name_label_` is shown instead of `device_selector_combobox_` when
  // device count is less or equal to 1. We went with a label instead of a
  // disabled combobox, for that case, because a disabled combobox is not
  // accessible by screen reader on Windows.
  device_name_label_->SetVisible(device_count <= 1);
  device_selector_combobox_->SetVisible(device_count > 1);
  if (has_devices) {
    OnComboboxSelection();
  }
  base_view_->RefreshSize();
}

void MediaViewControllerBase::OnComboboxSelection() {
  source_change_callback_.Run(device_selector_combobox_->GetSelectedIndex());
}

void MediaViewControllerBase::UpdateDeviceNameLabel(bool has_devices) {
  if (has_devices) {
    auto index = device_selector_combobox_->GetSelectedIndex();
    CHECK(index);
    device_name_label_->SetText(
        device_selector_combobox_->GetModel()->GetItemAt(index.value()));
  } else {
    device_name_label_->SetText(no_devices_found_combobox_text_);
  }
}
