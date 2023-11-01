// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"

MediaViewControllerBase::MediaViewControllerBase(
    MediaView& base_view,
    bool needs_borders,
    ui::ComboboxModel* model,
    base::RepeatingClosure on_selection_changed,
    const std::u16string& combobox_accessible_name,
    const std::u16string& no_device_connected_label_text)
    : base_view_(base_view),
      no_device_connected_label_(raw_ref<views::Label>::from_ptr(
          base_view_->AddChildView(std::make_unique<views::Label>()))),
      device_selector_combobox_(raw_ref<views::Combobox>::from_ptr(
          base_view_->AddChildView(std::make_unique<views::Combobox>(model)))),
      combobox_selection_change_callback_(std::move(on_selection_changed)) {
  CHECK(combobox_selection_change_callback_);

  if (needs_borders) {
    auto* provider = ChromeLayoutProvider::Get();
    const int kRoundedRadius = provider->GetCornerRadiusMetric(
        views::ShapeContextTokens::kOmniboxExpandedRadius);
    const int kBorderThickness =
        provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

    base_view_->SetBorder(views::CreateThemedRoundedRectBorder(
        kBorderThickness, kRoundedRadius, ui::kColorMenuBackground));
    base_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorMenuBackground, kRoundedRadius));
  }

  no_device_connected_label_->SetText(no_device_connected_label_text);
  no_device_connected_label_->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  no_device_connected_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  device_selector_combobox_->SetAccessibleName(combobox_accessible_name);
  device_selector_combobox_->SetEnabled(false);
  device_selector_combobox_->SetCallback(combobox_selection_change_callback_);
}

MediaViewControllerBase::~MediaViewControllerBase() {
  device_selector_combobox_->SetCallback({});
}

MediaView& MediaViewControllerBase::GetLiveFeedContainer() {
  return *base_view_;
}

void MediaViewControllerBase::AdjustComboboxEnabledState(bool has_devices) {
  no_device_connected_label_->SetVisible(!has_devices);
  device_selector_combobox_->SetEnabled(has_devices);

  if (has_devices) {
    combobox_selection_change_callback_.Run();
  } else {
    active_device_id_.clear();
  }

  base_view_->RefreshSize();
}

absl::optional<size_t> MediaViewControllerBase::GetComboboxSelectedIndex()
    const {
  return device_selector_combobox_->GetSelectedIndex();
}

bool MediaViewControllerBase::UpdateActiveDeviceId(
    const std::string& device_id) {
  if (active_device_id_ == device_id) {
    return false;
  }
  active_device_id_ = device_id;
  return true;
}
