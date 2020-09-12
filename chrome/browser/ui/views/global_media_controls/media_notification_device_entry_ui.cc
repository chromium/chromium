// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr gfx::Insets kIconContainerInsets{10, 15};
constexpr int kDeviceIconSize = 18;
constexpr gfx::Insets kLabelsContainerInsets{18, 0};
constexpr gfx::Size kDeviceEntryViewSize{400, 30};
constexpr int kEntryHighlightOpacity = 45;

}  // namespace

DeviceEntryUI::DeviceEntryUI(SkColor foreground_color,
                             SkColor background_color,
                             const std::string& raw_device_id,
                             const std::string& device_name,
                             const gfx::VectorIcon* icon,
                             const std::string& subtext)
    : foreground_color_(foreground_color),
      background_color_(background_color),
      raw_device_id_(raw_device_id),
      device_name_(device_name),
      icon_(icon) {}

std::string DeviceEntryUI::GetEntryLabelForTesting() {
  return base::UTF16ToUTF8(device_name_label_->GetText());
}

AudioDeviceEntryView::AudioDeviceEntryView(SkColor foreground_color,
                                           SkColor background_color,
                                           const std::string& raw_device_id,
                                           const std::string& device_name,
                                           const std::string& subtext)
    : DeviceEntryUI(foreground_color,
                    background_color,
                    raw_device_id,
                    device_name,
                    &vector_icons::kHeadsetIcon) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  icon_container_ = AddChildView(std::make_unique<views::View>());
  auto* icon_container_layout =
      icon_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kIconContainerInsets));
  icon_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  icon_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  device_icon_ =
      icon_container_->AddChildView(std::make_unique<views::ImageView>());
  device_icon_->SetImage(
      gfx::CreateVectorIcon(*icon_, kDeviceIconSize, foreground_color));

  labels_container_ = AddChildView(std::make_unique<views::View>());
  auto* labels_container_layout =
      labels_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kLabelsContainerInsets));
  labels_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  labels_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::Label::CustomFont device_name_label_font{
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(1)};
  device_name_label_ =
      labels_container_->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(device_name_), device_name_label_font));
  device_name_label_->SetEnabledColor(foreground_color);
  device_name_label_->SetBackgroundColor(background_color);

  if (!subtext.empty()) {
    device_subtext_label_ = labels_container_->AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(subtext)));
    device_subtext_label_->SetTextStyle(
        views::style::TextStyle::STYLE_SECONDARY);
    device_subtext_label_->SetEnabledColor(foreground_color);
    device_subtext_label_->SetBackgroundColor(background_color);
  }

  // Ensures that hovering over these items also hovers this view.
  icon_container_->set_can_process_events_within_subtree(false);
  labels_container_->set_can_process_events_within_subtree(false);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetInkDropMode(Button::InkDropMode::ON);
  set_ink_drop_base_color(foreground_color);
  set_has_ink_drop_action_on_click(true);
  SetPreferredSize(kDeviceEntryViewSize);
}

void AudioDeviceEntryView::SetHighlighted(bool highlighted) {
  is_highlighted_ = highlighted;
  if (highlighted) {
    SetInkDropMode(Button::InkDropMode::OFF);
    set_has_ink_drop_action_on_click(false);
    SetBackground(views::CreateSolidBackground(
        SkColorSetA(GetInkDropBaseColor(), kEntryHighlightOpacity)));
  } else {
    SetInkDropMode(Button::InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
    SetBackground(nullptr);
  }
}

void AudioDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                           SkColor background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;
  set_ink_drop_base_color(foreground_color_);

  device_icon_->SetImage(
      gfx::CreateVectorIcon(*icon_, kDeviceIconSize, foreground_color_));

  device_name_label_->SetEnabledColor(foreground_color_);
  device_name_label_->SetBackgroundColor(background_color_);

  if (device_subtext_label_) {
    device_subtext_label_->SetEnabledColor(foreground_color_);
    device_subtext_label_->SetBackgroundColor(background_color_);
  }

  // Reapply highlight formatting as some effects rely on these colors.
  SetHighlighted(is_highlighted_);
}

DeviceEntryUIType AudioDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kAudio;
}

CastDeviceEntryView::CastDeviceEntryView(SkColor foreground_color,
                                         SkColor background_color,
                                         const media_router::UIMediaSink& sink)
    : DeviceEntryUI(foreground_color,
                    background_color,
                    sink.id,
                    base::UTF16ToUTF8(sink.friendly_name),
                    // TODO(muyaoxu): change device icon
                    &vector_icons::kHeadsetIcon),
      CastDialogSinkButton(nullptr,
                           sink,
                           /* TODO(muyaoxu): change this to button_tag */ 0) {}

// TODO(muyaoxu): Implement this function
void CastDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                          SkColor background_color) {}

DeviceEntryUIType CastDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kCast;
}
