// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kRoundedRadius = 12;

}  // namespace

MediaViewControllerBase::MediaViewControllerBase(
    MediaView& base_view,
    bool needs_borders,
    ui::ComboboxModel* model,
    SourceChangeCallback source_change_callback,
    const std::u16string& combobox_accessible_name,
    const std::u16string& no_devices_found_combobox_text,
    const std::u16string& no_devices_found_label_text,
    bool allow_device_selection,
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
      allow_device_selection_(allow_device_selection),
      source_change_callback_(std::move(source_change_callback)),
      previous_device_name_(no_devices_found_combobox_text_),
      metrics_context_(metrics_context) {
  CHECK(source_change_callback_);

  auto* provider = ChromeLayoutProvider::Get();

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
  no_devices_found_label_->SetTextStyle(
      views::style::TextStyle::STYLE_BODY_3_MEDIUM);
  no_devices_found_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  device_name_label_->SetText(no_devices_found_combobox_text_);
  device_name_label_->SetTextStyle(views::style::TextStyle::STYLE_BODY_4);
  device_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  device_name_label_->SetProperty(
      views::kMarginsKey, gfx::Insets().set_top(provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  device_selector_combobox_->GetViewAccessibility().SetName(
      combobox_accessible_name);
  device_selector_combobox_->SetSizeToLargestLabel(false);
  device_selector_combobox_->SetVisible(false);
  device_selector_combobox_->SetProperty(
      views::kMarginsKey, gfx::Insets().set_top(provider->GetDistanceMetric(
                              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // Unretained is safe, because `this` outlives `device_selector_combobox_`.
  device_selector_combobox_->SetCallback(
      base::BindRepeating(&MediaViewControllerBase::OnComboboxSelection,
                          base::Unretained(this), /*due_to_user_action=*/true));
  on_menu_will_show_subscription_ =
      device_selector_combobox_->AddMenuWillShowCallback(
          base::BindRepeating(&MediaViewControllerBase::OnComboboxMenuWillShow,
                              base::Unretained(this)));
}

MediaViewControllerBase::~MediaViewControllerBase() {
  device_selector_combobox_->SetCallback({});
  if (allow_device_selection_) {
    media_preview_metrics::RecordDeviceSelectionAction(metrics_context_,
                                                       user_action_);
  }
}

void MediaViewControllerBase::OnDeviceListChanged(size_t device_count) {
  const bool has_devices = device_count > 0;
  if (!has_devices) {
    live_feed_container_->SetVisible(false);
    no_devices_found_label_->SetVisible(true);
    device_name_label_->SetText(no_devices_found_combobox_text_);
    device_name_label_->SetEnabledColorId(
        ui::ColorIds::kColorSysOnSurfaceSubtle);
    device_name_label_->SetVisible(true);
    device_selector_combobox_->SetVisible(false);
    AnnounceDynamicChangeIfNeeded(no_devices_found_label_->GetText());
    previous_device_name_ = no_devices_found_combobox_text_;
    base_view_->RefreshSize();
    return;
  }

  live_feed_container_->SetVisible(true);
  no_devices_found_label_->SetVisible(false);
  UpdateDeviceNameLabel();
  device_name_label_->SetVisible(!allow_device_selection_);
  device_selector_combobox_->SetVisible(allow_device_selection_);
  AnnounceDynamicChangeIfNeeded(l10n_util::GetStringFUTF16(
      IDS_MEDIA_PREVIEW_ANNOUNCE_SELECTED_DEVICE_CHANGE,
      device_name_label_->GetText()));
  OnComboboxSelection(/*due_to_user_action=*/false);
  base_view_->RefreshSize();
}

void MediaViewControllerBase::OnComboboxSelection(bool due_to_user_action) {
  auto index = device_selector_combobox_->GetSelectedIndex();
  if (!index) {
    return;
  }

  const auto& newly_selected_device_name =
      device_selector_combobox_->GetModel()->GetItemAt(index.value());
  if (due_to_user_action &&
      newly_selected_device_name != previous_device_name_) {
    user_action_ = media_preview_metrics::
        MediaPreviewDeviceSelectionUserAction::kSelection;
  }

  previous_device_name_ = newly_selected_device_name;
  source_change_callback_.Run(index);
}

void MediaViewControllerBase::UpdateDeviceNameLabel() {
  auto index = device_selector_combobox_->GetSelectedIndex();
  CHECK(index);
  device_name_label_->SetText(
      device_selector_combobox_->GetModel()->GetItemAt(index.value()));
  device_name_label_->SetEnabledColorId(ui::ColorIds::kColorSysOnSurface);
}

void MediaViewControllerBase::AnnounceDynamicChangeIfNeeded(
    std::u16string announcement) {
  if (!has_device_list_changed_before_) {
    has_device_list_changed_before_ = true;
    return;
  }

  if (!allow_device_selection_) {
    return;
  }

  if (previous_device_name_ == device_name_label_->GetText()) {
    return;
  }

  device_name_label_->GetViewAccessibility().AnnouncePolitely(announcement);
}

void MediaViewControllerBase::OnComboboxMenuWillShow() {
  if (user_action_ ==
      media_preview_metrics::MediaPreviewDeviceSelectionUserAction::kNoAction) {
    user_action_ =
        media_preview_metrics::MediaPreviewDeviceSelectionUserAction::kOpened;
  }
}
