// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/util/ranges/algorithm.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace {

// Constants for the DeviceEntryView
constexpr gfx::Insets kIconContainerInsets{10, 15};
constexpr int kDeviceIconSize = 18;
constexpr gfx::Insets kLabelsContainerInsets{18, 0};
constexpr gfx::Size kDeviceEntryViewSize{400, 30};
constexpr int kEntryHighlightOpacity = 45;
// Constants for the MediaNotificationDeviceSelectorView
constexpr gfx::Insets kExpandButtonStripInsets{6, 15};
constexpr gfx::Size kExpandButtonStripSize{400, 30};
constexpr gfx::Insets kExpandButtonBorderInsets{4, 8};

class DeviceEntryView : public views::Button {
 public:
  DeviceEntryView(const SkColor& foreground_color,
                  const SkColor& background_color,
                  const std::string& raw_device_id,
                  const std::string& name,
                  const std::string& subtext = "");

  const std::string& GetDeviceId() { return raw_device_id_; }
  const std::string& GetDeviceName() { return device_name_; }

  void SetHighlighted(bool highlighted);

  void OnColorsChanged(const SkColor& foreground_color,
                       const SkColor& background_color);

  bool is_highlighted_for_testing() { return is_highlighted_; }

 protected:
  SkColor foreground_color_, background_color_;
  const std::string raw_device_id_, device_name_;
  bool is_highlighted_ = false;
  views::View* icon_container_;
  views::ImageView* device_icon_;
  views::View* labels_container_;
  views::Label* device_name_label_;
  views::Label* device_subtext_label_ = nullptr;
};

}  // anonymous namespace

DeviceEntryView::DeviceEntryView(const SkColor& foreground_color,
                                 const SkColor& background_color,
                                 const std::string& raw_device_id,
                                 const std::string& name,
                                 const std::string& subtext)
    : foreground_color_(foreground_color),
      background_color_(background_color),
      raw_device_id_(raw_device_id),
      device_name_(name) {
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
  device_icon_->SetImage(gfx::CreateVectorIcon(
      vector_icons::kHeadsetIcon, kDeviceIconSize, foreground_color));

  labels_container_ = AddChildView(std::make_unique<views::View>());
  auto* labels_container_layout_ =
      labels_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kLabelsContainerInsets));
  labels_container_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  labels_container_layout_->set_cross_axis_alignment(
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

void DeviceEntryView::SetHighlighted(bool highlighted) {
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

void DeviceEntryView::OnColorsChanged(const SkColor& foreground_color,
                                      const SkColor& background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;
  set_ink_drop_base_color(foreground_color_);

  device_icon_->SetImage(gfx::CreateVectorIcon(
      vector_icons::kHeadsetIcon, kDeviceIconSize, foreground_color_));

  device_name_label_->SetEnabledColor(foreground_color_);
  device_name_label_->SetBackgroundColor(background_color_);

  if (device_subtext_label_) {
    device_subtext_label_->SetEnabledColor(foreground_color_);
    device_subtext_label_->SetBackgroundColor(background_color_);
  }

  // Reapply highlight formatting as some effects rely on these colors.
  SetHighlighted(is_highlighted_);
}

namespace {

class ExpandDeviceSelectorButton : public IconLabelBubbleView {
 public:
  explicit ExpandDeviceSelectorButton(IconLabelBubbleView::Delegate* delegate);
  ~ExpandDeviceSelectorButton() override = default;

  void OnColorsChanged();

 private:
  bool ShouldShowSeparator() const override { return false; }
  IconLabelBubbleView::Delegate* delegate_;
};

}  // anonymous namespace

ExpandDeviceSelectorButton::ExpandDeviceSelectorButton(
    IconLabelBubbleView::Delegate* delegate)
    : IconLabelBubbleView(
          views::style::GetFont(views::style::TextContext::CONTEXT_BUTTON,
                                views::style::TextStyle::STYLE_PRIMARY),
          delegate),
      delegate_(delegate) {
  SetLabel(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_BUTTON_LABEL));
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetBorder(views::CreateRoundedRectBorder(
      1, kExpandButtonStripSize.height() / 2, gfx::Insets(),
      delegate_->GetIconLabelBubbleSurroundingForegroundColor()));

  label()->SetBorder(views::CreateEmptyBorder(kExpandButtonBorderInsets));
  label()->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  auto size = GetPreferredSize();
  size.set_height(kExpandButtonStripSize.height());
  size.set_width(size.width() + kExpandButtonBorderInsets.width());
  SetPreferredSize(size);
}

void ExpandDeviceSelectorButton::OnColorsChanged() {
  UpdateLabelColors();
  SetBorder(views::CreateRoundedRectBorder(
      1, kExpandButtonStripSize.height() / 2, gfx::Insets(),
      delegate_->GetIconLabelBubbleSurroundingForegroundColor()));
}

MediaNotificationDeviceSelectorView::MediaNotificationDeviceSelectorView(
    MediaNotificationDeviceSelectorViewDelegate* delegate,
    const std::string& current_device_id,
    const SkColor& foreground_color,
    const SkColor& background_color)
    : delegate_(delegate),
      current_device_id_(current_device_id),
      foreground_color_(foreground_color),
      background_color_(background_color) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  expand_button_strip_ = AddChildView(std::make_unique<views::View>());
  auto* expand_button_strip_layout =
      expand_button_strip_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kExpandButtonStripInsets));
  expand_button_strip_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  expand_button_strip_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  expand_button_strip_->SetPreferredSize(kExpandButtonStripSize);

  expand_button_ = expand_button_strip_->AddChildView(
      std::make_unique<ExpandDeviceSelectorButton>(this));
  expand_button_->set_listener(this);

  audio_device_entries_container_ =
      AddChildView(std::make_unique<views::View>());
  audio_device_entries_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  audio_device_entries_container_->SetVisible(false);

  SetBackground(views::CreateSolidBackground(background_color_));
  // Set the size of this view
  SetPreferredSize(kExpandButtonStripSize);
  Layout();

  // This view will become visible when devices are discovered.
  SetVisible(false);

  // Get a list of the connected audio output devices.
  audio_device_subscription_ =
      delegate->RegisterAudioOutputDeviceDescriptionsCallback(
          base::BindRepeating(
              &MediaNotificationDeviceSelectorView::UpdateAvailableAudioDevices,
              weak_ptr_factory_.GetWeakPtr()));

  // Get the availability of audio output device switching.
  is_device_switching_enabled_subscription_ =
      delegate_->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
          base::BindRepeating(&MediaNotificationDeviceSelectorView::
                                  UpdateIsAudioDeviceSwitchingEnabled,
                              weak_ptr_factory_.GetWeakPtr()));
}

void MediaNotificationDeviceSelectorView::UpdateCurrentAudioDevice(
    const std::string& current_device_id) {
  if (current_device_entry_view_)
    current_device_entry_view_->SetHighlighted(false);

  auto it = util::ranges::find_if(
      audio_device_entries_container_->children(),
      [&current_device_id](auto& item) {
        return static_cast<DeviceEntryView*>(item)->GetDeviceId() ==
               current_device_id;
      });

  if (it == audio_device_entries_container_->children().end()) {
    current_device_entry_view_ = nullptr;
    current_device_id_ = "";
    return;
  }

  current_device_id_ = current_device_id;
  current_device_entry_view_ = static_cast<DeviceEntryView*>(*it);
  current_device_entry_view_->SetHighlighted(true);
  audio_device_entries_container_->ReorderChildView(current_device_entry_view_,
                                                    0);

  current_device_entry_view_->Layout();
}

MediaNotificationDeviceSelectorView::~MediaNotificationDeviceSelectorView() {
  audio_device_subscription_.release();
}

void MediaNotificationDeviceSelectorView::UpdateAvailableAudioDevices(
    const media::AudioDeviceDescriptions& device_descriptions) {
  audio_device_entries_container_->RemoveAllChildViews(true);
  current_device_entry_view_ = nullptr;

  bool current_device_still_exists = false;
  for (auto description : device_descriptions) {
    auto device_entry_view = std::make_unique<DeviceEntryView>(
        foreground_color_, background_color_, description.unique_id,
        description.device_name, "");
    device_entry_view->set_listener(this);
    audio_device_entries_container_->AddChildView(std::move(device_entry_view));
    if (!current_device_still_exists &&
        description.unique_id == current_device_id_)
      current_device_still_exists = true;
  }

  // If the current device no longer exists, fallback to the default device
  UpdateCurrentAudioDevice(
      current_device_still_exists
          ? current_device_id_
          : media::AudioDeviceDescription::kDefaultDeviceId);

  UpdateVisibility();
}

void MediaNotificationDeviceSelectorView::OnColorsChanged(
    const SkColor& foreground_color,
    const SkColor& background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;

  SetBackground(views::CreateSolidBackground(background_color_));

  for (auto* view : audio_device_entries_container_->children()) {
    static_cast<DeviceEntryView*>(view)->OnColorsChanged(foreground_color_,
                                                         background_color_);
  }

  expand_button_->OnColorsChanged();

  SchedulePaint();
}

void MediaNotificationDeviceSelectorView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == expand_button_) {
    if (is_expanded_)
      HideDevices();
    else
      ShowDevices();

    delegate_->OnDeviceSelectorViewSizeChanged();
  } else {
    DCHECK(std::find(audio_device_entries_container_->children().cbegin(),
                     audio_device_entries_container_->children().cend(),
                     sender) !=
           audio_device_entries_container_->children().end());
    delegate_->OnAudioSinkChosen(
        static_cast<DeviceEntryView*>(sender)->GetDeviceId());
  }
}

SkColor MediaNotificationDeviceSelectorView::
    GetIconLabelBubbleSurroundingForegroundColor() const {
  return foreground_color_;
}

SkColor MediaNotificationDeviceSelectorView::GetIconLabelBubbleBackgroundColor()
    const {
  return background_color_;
}

views::Button*
MediaNotificationDeviceSelectorView::get_expand_button_for_testing() {
  return expand_button_;
}

// static
std::string MediaNotificationDeviceSelectorView::get_entry_label_for_testing(
    views::View* entry_view) {
  return static_cast<DeviceEntryView*>(entry_view)->GetDeviceName();
}

// static
bool MediaNotificationDeviceSelectorView::get_entry_is_highlighted_for_testing(
    views::View* entry_view) {
  return static_cast<DeviceEntryView*>(entry_view)
      ->is_highlighted_for_testing();
}

void MediaNotificationDeviceSelectorView::ShowDevices() {
  DCHECK(!is_expanded_);
  is_expanded_ = true;

  audio_device_entries_container_->SetVisible(true);
  PreferredSizeChanged();
}

void MediaNotificationDeviceSelectorView::HideDevices() {
  DCHECK(is_expanded_);
  is_expanded_ = false;

  audio_device_entries_container_->SetVisible(false);
  PreferredSizeChanged();
}

void MediaNotificationDeviceSelectorView::UpdateVisibility() {
  SetVisible(ShouldBeVisible());
  delegate_->OnDeviceSelectorViewSizeChanged();
}

bool MediaNotificationDeviceSelectorView::ShouldBeVisible() {
  if (!is_audio_device_switching_enabled_)
    return false;

  // The UI should be visible if there are more than one unique devices. That is
  // when:
  // * There are at least three devices
  // * Or, there are two devices and one of them has the default ID but not the
  // default name.
  if (audio_device_entries_container_->children().size() == 2) {
    return util::ranges::any_of(
        audio_device_entries_container_->children(), [](views::View* view) {
          auto* entry = static_cast<DeviceEntryView*>(view);
          return entry->GetDeviceId() ==
                     media::AudioDeviceDescription::kDefaultDeviceId &&
                 entry->GetDeviceName() !=
                     media::AudioDeviceDescription::GetDefaultDeviceName();
        });
  }
  return audio_device_entries_container_->children().size() > 2;
}

void MediaNotificationDeviceSelectorView::UpdateIsAudioDeviceSwitchingEnabled(
    bool enabled) {
  if (enabled == is_audio_device_switching_enabled_)
    return;

  is_audio_device_switching_enabled_ = enabled;
  UpdateVisibility();
}
