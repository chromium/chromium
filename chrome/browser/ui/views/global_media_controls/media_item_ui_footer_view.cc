// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_footer_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace {

constexpr int kEntryMaxWidth = 150;
constexpr int kDeviceIconSize = 12;

// Label button with custom hover effect.
class DeviceEntryButton : public views::LabelButton {
  METADATA_HEADER(DeviceEntryButton, views::LabelButton)

 public:
  explicit DeviceEntryButton(PressedCallback callback,
                             const gfx::VectorIcon* icon = nullptr,
                             const std::u16string& text = std::u16string());
  ~DeviceEntryButton() override = default;

  void UpdateColor(SkColor foreground_color);
  void SetIcon(const gfx::VectorIcon* icon);
  SkColor GetForegroundColor() const;

 private:
  void UpdateImage();

  SkColor foreground_color_ = gfx::kPlaceholderColor;

  raw_ptr<const gfx::VectorIcon> icon_;
};

DeviceEntryButton::DeviceEntryButton(PressedCallback callback,
                                     const gfx::VectorIcon* icon,
                                     const std::u16string& text)
    : LabelButton(std::move(callback), text), icon_(icon) {
  ConfigureInkDropForToolbar(this);
  views::InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      &DeviceEntryButton::GetForegroundColor, base::Unretained(this)));
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_LABEL_BUTTON)));
  SetMaxSize(gfx::Size(kEntryMaxWidth, 0));
}

void DeviceEntryButton::UpdateColor(SkColor foreground_color) {
  foreground_color_ = foreground_color;

  SetEnabledTextColors(foreground_color_);
  UpdateImage();
}

void DeviceEntryButton::SetIcon(const gfx::VectorIcon* icon) {
  icon_ = icon;
  UpdateImage();
}

SkColor DeviceEntryButton::GetForegroundColor() const {
  return foreground_color_;
}

void DeviceEntryButton::UpdateImage() {
  if (!icon_)
    return;

  SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon_, foreground_color_,
                                               kDeviceIconSize));
}

BEGIN_METADATA(DeviceEntryButton)
END_METADATA

}  // anonymous namespace

MediaItemUIFooterView::MediaItemUIFooterView(
    base::RepeatingClosure stop_casting_callback) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  if (stop_casting_callback.is_null())
    return;

  // |this| owns the DeviceEntryButton, so base::Unretained is safe here.
  AddChildView(std::make_unique<DeviceEntryButton>(
      stop_casting_callback, nullptr,
      l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_STOP_CASTING_BUTTON_LABEL)));
}

MediaItemUIFooterView::~MediaItemUIFooterView() = default;

void MediaItemUIFooterView::OnMediaItemUIDeviceSelectorUpdated(
    const std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>>&
        device_entries_map) {
  RemoveAllChildViews();

  for (const auto& entry : device_entries_map) {
    int tag = entry.first;
    DeviceEntryUI* device_entry = entry.second;

    auto* device_entry_button =
        AddChildView(std::make_unique<DeviceEntryButton>(
            base::BindRepeating(&MediaItemUIFooterView::OnDeviceSelected,
                                base::Unretained(this), tag),
            &device_entry->icon(),
            base::UTF8ToUTF16(device_entry->device_name())));
    device_entry_button->set_tag(tag);
    device_entry_button->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            views::MinimumFlexSizeRule::kPreferredSnapToZero)
            .WithOrder(1));
  }

  overflow_button_ = AddChildView(std::make_unique<DeviceEntryButton>(
      base::BindRepeating(&MediaItemUIFooterView::OnOverflowButtonClicked,
                          base::Unretained(this))));
  overflow_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred)
          .WithOrder(2));
  if (delegate_) {
    overflow_button_->SetIcon(delegate_->IsDeviceSelectorExpanded()
                                  ? &kMediaControlsArrowDropUpIcon
                                  : &kMediaControlsArrowDropDownIcon);
  }

  UpdateButtonsColor();
}

void MediaItemUIFooterView::Layout(PassKey) {
  if (!overflow_button_) {
    LayoutSuperclass<views::View>(this);
    return;
  }

  overflow_button_->SetVisible(false);
  if (GetPreferredSize().width() > GetContentsBounds().width())
    overflow_button_->SetVisible(true);
  LayoutSuperclass<views::View>(this);
}

void MediaItemUIFooterView::OnColorsChanged(SkColor foreground,
                                            SkColor background) {
  foreground_color_ = foreground;
  UpdateButtonsColor();
}

void MediaItemUIFooterView::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void MediaItemUIFooterView::UpdateButtonsColor() {
  for (views::View* view : children()) {
    static_cast<DeviceEntryButton*>(view)->UpdateColor(foreground_color_);
  }
}

void MediaItemUIFooterView::OnDeviceSelected(int tag) {
  if (delegate_)
    delegate_->OnDeviceSelected(tag);
}

void MediaItemUIFooterView::OnOverflowButtonClicked() {
  if (!delegate_)
    return;

  delegate_->OnDropdownButtonClicked();
  overflow_button_->SetIcon(delegate_->IsDeviceSelectorExpanded()
                                ? &kMediaControlsArrowDropUpIcon
                                : &kMediaControlsArrowDropDownIcon);
}

BEGIN_METADATA(MediaItemUIFooterView)
END_METADATA
