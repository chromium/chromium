// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/styled_label.h"

namespace {

constexpr int kDeviceIconSize = 20;
constexpr auto kDeviceIconBorder = gfx::Insets(6);
constexpr gfx::Size kDeviceEntryViewSize{400, 30};
constexpr int kEntryHighlightOpacity = 45;

void ChangeEntryColor(views::ImageView* image_view,
                      views::StyledLabel* title_view,
                      views::Label* subtitle_view,
                      const gfx::VectorIcon* icon,
                      SkColor foreground_color,
                      SkColor background_color) {
  if (image_view) {
    image_view->SetImage(
        gfx::CreateVectorIcon(*icon, kDeviceIconSize, foreground_color));
  }

  title_view->SetDisplayedOnBackgroundColor(background_color);
  if (!title_view->GetText().empty()) {
    views::StyledLabel::RangeStyleInfo style_info;
    style_info.text_style = views::style::STYLE_PRIMARY;
    style_info.override_color = foreground_color;
    title_view->ClearStyleRanges();
    title_view->AddStyleRange(gfx::Range(0, title_view->GetText().length()),
                              style_info);
    title_view->SizeToFit(0);
  }

  if (subtitle_view) {
    subtitle_view->SetEnabledColor(foreground_color);
    subtitle_view->SetBackgroundColor(background_color);
  }
}

std::unique_ptr<views::ImageView> GetAudioDeviceIcon() {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(gfx::CreateVectorIcon(
      vector_icons::kHeadsetIcon, kDeviceIconSize, gfx::kPlaceholderColor));
  icon_view->SetBorder(views::CreateEmptyBorder(kDeviceIconBorder));
  return icon_view;
}

}  // namespace

DeviceEntryUI::DeviceEntryUI(const std::string& raw_device_id,
                             const std::string& device_name,
                             const gfx::VectorIcon* icon,
                             const std::string& subtext)
    : raw_device_id_(raw_device_id), device_name_(device_name), icon_(icon) {}

AudioDeviceEntryView::AudioDeviceEntryView(
    views::ButtonListener* button_listener,
    SkColor foreground_color,
    SkColor background_color,
    const std::string& raw_device_id,
    const std::string& device_name,
    const std::string& subtext)
    : DeviceEntryUI(raw_device_id, device_name, &vector_icons::kHeadsetIcon),
      HoverButton(button_listener,
                  GetAudioDeviceIcon(),
                  base::UTF8ToUTF16(device_name),
                  base::UTF8ToUTF16(subtext)) {
  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), icon_, foreground_color, background_color);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetInkDropMode(Button::InkDropMode::ON);
  SetInkDropBaseColor(foreground_color);
  SetHasInkDropActionOnClick(true);
  SetPreferredSize(kDeviceEntryViewSize);
}

void AudioDeviceEntryView::SetHighlighted(bool highlighted) {
  is_highlighted_ = highlighted;
  if (highlighted) {
    SetInkDropMode(Button::InkDropMode::OFF);
    SetHasInkDropActionOnClick(false);
    SetBackground(views::CreateSolidBackground(
        SkColorSetA(GetInkDropBaseColor(), kEntryHighlightOpacity)));
  } else {
    SetInkDropMode(Button::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    SetBackground(nullptr);
  }
}

void AudioDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                           SkColor background_color) {
  SetInkDropBaseColor(foreground_color);

  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), icon_, foreground_color, background_color);

  // Reapply highlight formatting as some effects rely on these colors.
  SetHighlighted(is_highlighted_);
}

SkColor AudioDeviceEntryView::GetInkDropBaseColor() const {
  return views::Button::GetInkDropBaseColor();
}

DeviceEntryUIType AudioDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kAudio;
}

CastDeviceEntryView::CastDeviceEntryView(views::ButtonListener* button_listener,
                                         SkColor foreground_color,
                                         SkColor background_color,
                                         const media_router::UIMediaSink& sink)
    : DeviceEntryUI(sink.id,
                    base::UTF16ToUTF8(sink.friendly_name),
                    CastDialogSinkButton::GetVectorIcon(sink.icon_type)),
      CastDialogSinkButton(button_listener, sink) {
  switch (sink.state) {
    // If the sink state is CONNECTING or DISCONNECTING, a throbber icon will
    // show up. The icon's color remains unchanged.
    case media_router::UIMediaSinkState::CONNECTING:
    case media_router::UIMediaSinkState::DISCONNECTING:
      ChangeEntryColor(nullptr, title(), subtitle(), nullptr, foreground_color,
                       background_color);
      break;
    case media_router::UIMediaSinkState::CONNECTED:
    case media_router::UIMediaSinkState::AVAILABLE:
    case media_router::UIMediaSinkState::UNAVAILABLE:
      ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                       subtitle(), icon_, foreground_color, background_color);
      break;
    default:
      NOTREACHED();
  }

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetInkDropMode(Button::InkDropMode::ON);
  SetInkDropBaseColor(foreground_color);
  SetHasInkDropActionOnClick(true);
  SetPreferredSize(kDeviceEntryViewSize);
}

void CastDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                          SkColor background_color) {
  SetInkDropBaseColor(foreground_color);
  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), icon_, foreground_color, background_color);
}

DeviceEntryUIType CastDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kCast;
}

SkColor CastDeviceEntryView::GetInkDropBaseColor() const {
  return views::Button::GetInkDropBaseColor();
}
