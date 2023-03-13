// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"

namespace {

constexpr int kDeviceIconSize = 20;
constexpr auto kDeviceIconBorder = gfx::Insets(6);
constexpr gfx::Size kDeviceEntryViewSize{400, 30};

void ChangeEntryColor(views::ImageView* image_view,
                      views::StyledLabel* title_view,
                      views::Label* subtitle_view,
                      const gfx::VectorIcon* icon,
                      SkColor foreground_color,
                      SkColor background_color) {
  if (image_view) {
    image_view->SetImage(ui::ImageModel::FromVectorIcon(*icon, foreground_color,
                                                        kDeviceIconSize));
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

const gfx::VectorIcon* GetVectorIcon(
    global_media_controls::mojom::IconType icon) {
  switch (icon) {
    case global_media_controls::mojom::IconType::kInfo:
      return &vector_icons::kInfoOutlineIcon;
    case global_media_controls::mojom::IconType::kSpeaker:
      return &kSpeakerIcon;
    case global_media_controls::mojom::IconType::kSpeakerGroup:
      return &kSpeakerGroupIcon;
    case global_media_controls::mojom::IconType::kInput:
      return &kInputIcon;
    case global_media_controls::mojom::IconType::kTv:
      return &kTvIcon;
    // In these cases the icon is a placeholder and doesn't actually get shown.
    case global_media_controls::mojom::IconType::kThrobber:
    case global_media_controls::mojom::IconType::kUnknown:
      return &kTvIcon;
  }
}

std::unique_ptr<views::ImageView> CreateIconView(const gfx::VectorIcon* icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      *icon, gfx::kPlaceholderColor, kDeviceIconSize));
  icon_view->SetBorder(views::CreateEmptyBorder(kDeviceIconBorder));
  return icon_view;
}

std::unique_ptr<views::View> CreateIconView(
    global_media_controls::mojom::IconType icon) {
  if (icon == global_media_controls::mojom::IconType::kThrobber) {
    return media_router::CreateThrobber();
  }
  return CreateIconView(GetVectorIcon(icon));
}

std::unique_ptr<views::ImageView> GetAudioDeviceIcon() {
  return CreateIconView(&vector_icons::kHeadsetIcon);
}

}  // namespace

DeviceEntryUI::DeviceEntryUI(const std::string& raw_device_id,
                             const std::string& device_name,
                             const gfx::VectorIcon* icon,
                             const std::string& subtext)
    : raw_device_id_(raw_device_id), device_name_(device_name), icon_(icon) {}

AudioDeviceEntryView::AudioDeviceEntryView(PressedCallback callback,
                                           SkColor foreground_color,
                                           SkColor background_color,
                                           const std::string& raw_device_id,
                                           const std::string& device_name)
    : DeviceEntryUI(raw_device_id, device_name, &vector_icons::kHeadsetIcon),
      HoverButton(std::move(callback),
                  GetAudioDeviceIcon(),
                  base::UTF8ToUTF16(device_name)) {
  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), icon_, foreground_color, background_color);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);
  SetHasInkDropActionOnClick(true);
  SetPreferredSize(kDeviceEntryViewSize);
}

void AudioDeviceEntryView::SetHighlighted(bool highlighted) {
  if (is_highlighted_ == highlighted) {
    return;
  }
  is_highlighted_ = highlighted;
  if (highlighted) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    SetHasInkDropActionOnClick(false);
    SetBackground(views::CreateSolidBackground(
        SkColorSetA(views::InkDrop::Get(this)->GetBaseColor(), 0x2D)));
  } else {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    SetBackground(nullptr);
  }
  OnPropertyChanged(&is_highlighted_, views::kPropertyEffectsPaint);
}

bool AudioDeviceEntryView::GetHighlighted() const {
  return is_highlighted_;
}

void AudioDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                           SkColor background_color) {
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);

  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), icon_, foreground_color, background_color);

  // Reapply highlight formatting as some effects rely on these colors.
  SetHighlighted(is_highlighted_);
}

DeviceEntryUIType AudioDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kAudio;
}

CastDeviceEntryView::CastDeviceEntryView(
    base::RepeatingClosure callback,
    SkColor foreground_color,
    SkColor background_color,
    const global_media_controls::mojom::DevicePtr& device)
    : DeviceEntryUI(device->id, device->name, GetVectorIcon(device->icon)),
      HoverButton(std::move(callback),
                  CreateIconView(device->icon),
                  base::UTF8ToUTF16(device->name),
                  base::UTF8ToUTF16(device->status_text)),
      device_(device->Clone()) {
  ChangeCastEntryColor(foreground_color, background_color);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);
  SetHasInkDropActionOnClick(true);
  SetPreferredSize(kDeviceEntryViewSize);
}

CastDeviceEntryView::~CastDeviceEntryView() = default;

void CastDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                          SkColor background_color) {
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);
  ChangeCastEntryColor(foreground_color, background_color);
}

DeviceEntryUIType CastDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kCast;
}

void CastDeviceEntryView::ChangeCastEntryColor(SkColor foreground_color,
                                               SkColor background_color) {
  if (device_->icon == global_media_controls::mojom::IconType::kThrobber) {
    // Do not pass in `icon_view()` here as we want to keep the throbber view
    // with its color unchanged.
    ChangeEntryColor(nullptr, title(), subtitle(), nullptr, foreground_color,
                     background_color);
  } else {
    ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                     subtitle(), icon_, foreground_color, background_color);
  }
}

std::string CastDeviceEntryView::GetStatusTextForTest() const {
  return device_->status_text;
}

BEGIN_METADATA(AudioDeviceEntryView, HoverButton)
ADD_PROPERTY_METADATA(bool, Highlighted)
END_METADATA

BEGIN_METADATA(CastDeviceEntryView, HoverButton)
END_METADATA
