// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
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

// foreground_color_id is only set for CastDeviceEntryViewAsh.
std::unique_ptr<views::ImageView> CreateIconView(
    const gfx::VectorIcon& icon,
    std::optional<ui::ColorId> foreground_color_id = std::nullopt) {
  auto icon_view = std::make_unique<views::ImageView>();
  if (foreground_color_id.has_value()) {
    icon_view->SetImage(ui::ImageModel::FromVectorIcon(
        icon, foreground_color_id.value(), kDeviceIconSize));
  } else {
    icon_view->SetImage(ui::ImageModel::FromVectorIcon(
        icon, gfx::kPlaceholderColor, kDeviceIconSize));
  }
  icon_view->SetBorder(views::CreateEmptyBorder(kDeviceIconBorder));
  return icon_view;
}

// foreground_color_id is only set for CastDeviceEntryViewAsh.
std::unique_ptr<views::View> CreateIconView(
    global_media_controls::mojom::IconType icon,
    std::optional<ui::ColorId> foreground_color_id = std::nullopt) {
  if (icon == global_media_controls::mojom::IconType::kThrobber) {
    return media_router::CreateThrobber();
  }
  return CreateIconView(GetVectorIcon(icon), foreground_color_id);
}

std::unique_ptr<views::ImageView> GetAudioDeviceIcon() {
  return CreateIconView(vector_icons::kHeadsetIcon);
}

}  // namespace

DeviceEntryUI::DeviceEntryUI(const std::string& raw_device_id,
                             const std::string& device_name,
                             const gfx::VectorIcon& icon,
                             const std::string& subtext)
    : raw_device_id_(raw_device_id), device_name_(device_name), icon_(icon) {}

///////////////////////////////////////////////////////////////////////////////
// AudioDeviceEntryView:

AudioDeviceEntryView::AudioDeviceEntryView(PressedCallback callback,
                                           SkColor foreground_color,
                                           SkColor background_color,
                                           const std::string& raw_device_id,
                                           const std::string& device_name)
    : DeviceEntryUI(raw_device_id, device_name, vector_icons::kHeadsetIcon),
      HoverButton(std::move(callback),
                  GetAudioDeviceIcon(),
                  base::UTF8ToUTF16(device_name)) {
  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), &icon(), foreground_color, background_color);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
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
  ChangeEntryColor(static_cast<views::ImageView*>(icon_view()), title(),
                   subtitle(), &icon(), foreground_color, background_color);

  // Reapply highlight formatting as some effects rely on these colors.
  SetHighlighted(is_highlighted_);
}

DeviceEntryUIType AudioDeviceEntryView::GetType() const {
  return DeviceEntryUIType::kAudio;
}

///////////////////////////////////////////////////////////////////////////////
// CastDeviceEntryView:

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
  SetHasInkDropActionOnClick(true);
}

CastDeviceEntryView::~CastDeviceEntryView() = default;

void CastDeviceEntryView::OnColorsChanged(SkColor foreground_color,
                                          SkColor background_color) {
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
                     subtitle(), &icon(), foreground_color, background_color);
  }
}

std::string CastDeviceEntryView::GetStatusTextForTest() const {
  return device_->status_text;
}

///////////////////////////////////////////////////////////////////////////////
// CastDeviceEntryViewAsh:

CastDeviceEntryViewAsh::CastDeviceEntryViewAsh(
    PressedCallback callback,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    const global_media_controls::mojom::DevicePtr& device)
    : DeviceEntryUI(device->id, device->name, GetVectorIcon(device->icon)),
      HoverButton(std::move(callback),
                  CreateIconView(device->icon, foreground_color_id),
                  base::UTF8ToUTF16(device->name)),
      device_(device->Clone()) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
}

CastDeviceEntryViewAsh::~CastDeviceEntryViewAsh() = default;

DeviceEntryUIType CastDeviceEntryViewAsh::GetType() const {
  return DeviceEntryUIType::kCast;
}

BEGIN_METADATA(AudioDeviceEntryView)
ADD_PROPERTY_METADATA(bool, Highlighted)
END_METADATA

BEGIN_METADATA(CastDeviceEntryView)
END_METADATA

BEGIN_METADATA(CastDeviceEntryViewAsh)
END_METADATA
