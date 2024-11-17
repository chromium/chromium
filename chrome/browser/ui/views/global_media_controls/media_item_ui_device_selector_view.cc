// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

using media_router::MediaRouterMetrics;
using media_router::mojom::MediaRouteProviderId;

namespace {

// Constants for the MediaItemUIDeviceSelectorView
const int kExpandButtonStripWidth = 400;
const int kExpandButtonStripHeight = 30;
constexpr auto kExpandButtonStripInsets = gfx::Insets::VH(6, 15);
constexpr auto kExpandButtonBorderInsets = gfx::Insets::VH(4, 8);

// Constant for DropdownButton
const int kDropdownButtonIconSize = 15;
const int kDropdownButtonBackgroundRadius = 15;
constexpr gfx::Insets kDropdownButtonBorderInsets{4};

// The maximum number of audio devices to count when recording the
// Media.GlobalMediaControls.NumberOfAvailableAudioDevices histogram. 30 was
// chosen because it would be very unlikely to see a user with 30+ audio
// devices.
const int kAudioDevicesCountHistogramMax = 30;

class ExpandDeviceSelectorLabel : public views::Label {
  METADATA_HEADER(ExpandDeviceSelectorLabel, views::Label)

 public:
  explicit ExpandDeviceSelectorLabel(
      global_media_controls::GlobalMediaControlsEntryPoint entry_point);
  ~ExpandDeviceSelectorLabel() override = default;

  void OnColorsChanged(SkColor foreground_color, SkColor background_color);
};

BEGIN_METADATA(ExpandDeviceSelectorLabel)
END_METADATA

class ExpandDeviceSelectorButton : public views::ToggleImageButton {
  METADATA_HEADER(ExpandDeviceSelectorButton, views::ToggleImageButton)

 public:
  explicit ExpandDeviceSelectorButton(PressedCallback callback,
                                      SkColor background_color);
  ~ExpandDeviceSelectorButton() override = default;

  void OnColorsChanged(SkColor foreground_color);
};

BEGIN_METADATA(ExpandDeviceSelectorButton)
END_METADATA

}  // namespace

ExpandDeviceSelectorLabel::ExpandDeviceSelectorLabel(
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  if (entry_point ==
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    SetText(l10n_util::GetStringUTF16(
        IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_LABEL_WITH_COLON));
  } else {
    SetText(l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_LABEL));
  }
  auto size = GetPreferredSize();
  size.set_height(kExpandButtonStripHeight);
  size.set_width(size.width() + kExpandButtonBorderInsets.width());
  SetPreferredSize(size);
}

void ExpandDeviceSelectorLabel::OnColorsChanged(SkColor foreground_color,
                                                SkColor background_color) {
  SetEnabledColor(foreground_color);
  SetBackgroundColor(background_color);
}

ExpandDeviceSelectorButton::ExpandDeviceSelectorButton(PressedCallback callback,
                                                       SkColor foreground_color)
    : ToggleImageButton(std::move(callback)) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetBorder(views::CreateEmptyBorder(kDropdownButtonBorderInsets));

  SetHasInkDropActionOnClick(true);
  views::InstallFixedSizeCircleHighlightPathGenerator(
      this, kDropdownButtonBackgroundRadius);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);

  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_SHOW_DEVICE_LIST));
  SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_HIDE_DEVICE_LIST));
  OnColorsChanged(foreground_color);
}

void ExpandDeviceSelectorButton::OnColorsChanged(SkColor foreground_color) {
  // When the button is not toggled, the device list is collapsed and the arrow
  // is pointing up. Otherwise, the device list is expanded and the arrow is
  // pointing down.
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(vector_icons::kCaretDownIcon,
                                               foreground_color,
                                               kDropdownButtonIconSize));
  const auto caret_down_image = ui::ImageModel::FromVectorIcon(
      vector_icons::kCaretUpIcon, foreground_color, kDropdownButtonIconSize);
  SetToggledImageModel(views::Button::STATE_NORMAL, caret_down_image);
  views::InkDrop::Get(this)->SetBaseColor(foreground_color);
}

MediaItemUIDeviceSelectorView::MediaItemUIDeviceSelectorView(
    const std::string& item_id,
    MediaItemUIDeviceSelectorDelegate* delegate,
    mojo::PendingRemote<global_media_controls::mojom::DeviceListHost>
        device_list_host,
    mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
        receiver,
    bool has_audio_output,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_devices,
    std::optional<media_message_center::MediaColorTheme> media_color_theme)
    : item_id_(item_id),
      delegate_(delegate),
      entry_point_(entry_point),
      media_color_theme_(media_color_theme),
      device_list_host_(std::move(device_list_host)),
      receiver_(this, std::move(receiver)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Do not create the expand button strip if this device selector view is used
  // on Chrome OS ash.
  CreateExpandButtonStrip(
      /*show_expand_button=*/!media_color_theme_.has_value());

  device_entry_views_container_ = AddChildView(std::make_unique<views::View>());
  device_entry_views_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  device_entry_views_container_->SetVisible(false);

  if (show_devices) {
    ShowDevices();
  }
  SetBackground(views::CreateSolidBackground(background_color_));
  DeprecatedLayoutImmediately();

  // This view will become visible when devices are discovered.
  SetVisible(false);

  if (has_audio_output && base::FeatureList::IsEnabled(
                              media::kGlobalMediaControlsSeamlessTransfer)) {
    RegisterAudioDeviceCallbacks();
  }
}

void MediaItemUIDeviceSelectorView::UpdateCurrentAudioDevice(
    const std::string& current_device_id) {
  if (current_audio_device_entry_view_) {
    current_audio_device_entry_view_->SetHighlighted(false);
  }

  // Find DeviceEntryView* from |device_entry_ui_map_| with |current_device_id|.
  auto it = base::ranges::find(
      device_entry_ui_map_, current_device_id,
      [](const auto& pair) { return pair.second->raw_device_id(); });

  if (it == device_entry_ui_map_.end()) {
    current_audio_device_entry_view_ = nullptr;
    current_device_id_ = "";
    return;
  }
  current_device_id_ = current_device_id;
  current_audio_device_entry_view_ =
      static_cast<AudioDeviceEntryView*>(it->second);
  current_audio_device_entry_view_->SetHighlighted(true);
  device_entry_views_container_->ReorderChildView(
      current_audio_device_entry_view_, 0);
  current_audio_device_entry_view_->DeprecatedLayoutImmediately();
}

MediaItemUIDeviceSelectorView::~MediaItemUIDeviceSelectorView() {
  audio_device_subscription_ = {};

  // If this metric has not been recorded during the lifetime of this view, it
  // means that the device selector was never made available.
  if (!has_expand_button_been_shown_) {
    base::UmaHistogramBoolean(kDeviceSelectorAvailableHistogramName, false);
  } else if (!have_devices_been_shown_) {
    // Record if the device selector was available but never opened
    base::UmaHistogramBoolean(kDeviceSelectorOpenedHistogramName, false);
  }
}

void MediaItemUIDeviceSelectorView::UpdateAvailableAudioDevices(
    const media::AudioDeviceDescriptions& device_descriptions) {
  RemoveDevicesOfType(DeviceEntryUIType::kAudio);
  current_audio_device_entry_view_ = nullptr;

  bool current_device_still_exists = false;
  for (auto description : device_descriptions) {
    auto device_entry_view = std::make_unique<AudioDeviceEntryView>(
        base::BindRepeating(
            &MediaItemUIDeviceSelectorDelegate::OnAudioSinkChosen,
            base::Unretained(delegate_), item_id_, description.unique_id),
        foreground_color_, background_color_, description.unique_id,
        description.device_name);
    device_entry_view->set_tag(next_tag_++);
    device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
    device_entry_views_container_->AddChildView(std::move(device_entry_view));
    if (!current_device_still_exists &&
        description.unique_id == current_device_id_) {
      current_device_still_exists = true;
    }
  }
  // If the current device no longer exists, fallback to the default device
  UpdateCurrentAudioDevice(
      current_device_still_exists
          ? current_device_id_
          : media::AudioDeviceDescription::kDefaultDeviceId);

  UpdateVisibility();
  observers_.Notify(
      &MediaItemUIDeviceSelectorObserver::OnMediaItemUIDeviceSelectorUpdated,
      device_entry_ui_map_);
  if (media_item_ui_) {
    media_item_ui_->OnDeviceSelectorViewDevicesChanged(
        device_entry_views_container_->children().size() > 0);
  }
}

void MediaItemUIDeviceSelectorView::SetMediaItemUIView(
    global_media_controls::MediaItemUIView* view) {
  media_item_ui_ = view;
}

void MediaItemUIDeviceSelectorView::OnColorsChanged(SkColor foreground_color,
                                                    SkColor background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;

  SetBackground(views::CreateSolidBackground(background_color_));
  for (auto it : device_entry_ui_map_) {
    it.second->OnColorsChanged(foreground_color_, background_color_);
  }

  expand_label_->OnColorsChanged(foreground_color_, background_color_);
  if (dropdown_button_) {
    dropdown_button_->OnColorsChanged(foreground_color_);
  }
  SchedulePaint();
}

SkColor
MediaItemUIDeviceSelectorView::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return foreground_color_;
}

SkColor MediaItemUIDeviceSelectorView::GetIconLabelBubbleBackgroundColor()
    const {
  return background_color_;
}

void MediaItemUIDeviceSelectorView::ShowDevices() {
  DCHECK(!is_expanded_);
  is_expanded_ = true;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);

  // When this device selector view is used on Chrome OS ash, accessibility text
  // will be handled by MediaItemUIDetailedView instead of here.
  if (!media_color_theme_.has_value()) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_SHOW_DEVICE_LIST));
  }

  if (!have_devices_been_shown_) {
    base::UmaHistogramExactLinear(
        kAudioDevicesCountHistogramName,
        device_entry_views_container_->children().size(),
        kAudioDevicesCountHistogramMax);
    base::UmaHistogramBoolean(kDeviceSelectorOpenedHistogramName, true);
    have_devices_been_shown_ = true;
  }

  device_entry_views_container_->SetVisible(true);
  PreferredSizeChanged();

  // When this device selector view is used on Chrome OS ash, focus the first
  // available device when the device list is shown for accessibility.
  if (media_color_theme_.has_value() &&
      device_entry_views_container_->children().size() > 0) {
    device_entry_views_container_->children()[0]->RequestFocus();
  }
}

void MediaItemUIDeviceSelectorView::HideDevices() {
  DCHECK(is_expanded_);
  is_expanded_ = false;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);

  // When this device selector view is used on Chrome OS ash, accessibility text
  // will be handled by MediaItemUIDetailedView instead of here.
  if (!media_color_theme_.has_value()) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_HIDE_DEVICE_LIST));
  }

  device_entry_views_container_->SetVisible(false);
  PreferredSizeChanged();
}

void MediaItemUIDeviceSelectorView::UpdateVisibility() {
  SetVisible(ShouldBeVisible());

  if (!has_expand_button_been_shown_ && GetVisible()) {
    base::UmaHistogramBoolean(kDeviceSelectorAvailableHistogramName, true);
    has_expand_button_been_shown_ = true;
  }

  if (media_item_ui_) {
    media_item_ui_->OnListViewSizeChanged();
  }
}

bool MediaItemUIDeviceSelectorView::ShouldBeVisible() const {
  if (has_cast_device_) {
    return true;
  }
  if (!is_audio_device_switching_enabled_) {
    return false;
  }
  // The UI should be visible if there are more than one unique devices. That is
  // when:
  // * There are at least three devices
  // * Or, there are two devices and one of them has the default ID but not the
  // default name.
  if (device_entry_views_container_->children().size() == 2) {
    return base::ranges::any_of(
        device_entry_views_container_->children(), [this](views::View* view) {
          DeviceEntryUI* entry = GetDeviceEntryUI(view);
          return entry->raw_device_id() ==
                     media::AudioDeviceDescription::kDefaultDeviceId &&
                 entry->device_name() !=
                     media::AudioDeviceDescription::GetDefaultDeviceName();
        });
  }
  return device_entry_views_container_->children().size() > 2;
}

void MediaItemUIDeviceSelectorView::CreateExpandButtonStrip(
    bool show_expand_button) {
  expand_button_strip_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetInsideBorderInsets(kExpandButtonStripInsets)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetVisible(show_expand_button)
          .Build());

  expand_label_ = expand_button_strip_->AddChildView(
      std::make_unique<ExpandDeviceSelectorLabel>(entry_point_));

  // Show a button to show/hide the device list if dialog is opened from Cast
  // SDK.
  if (entry_point_ !=
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    dropdown_button_ = expand_button_strip_->AddChildView(
        std::make_unique<ExpandDeviceSelectorButton>(
            base::BindRepeating(
                &MediaItemUIDeviceSelectorView::ShowOrHideDeviceList,
                base::Unretained(this)),
            foreground_color_));
  }
}

void MediaItemUIDeviceSelectorView::ShowOrHideDeviceList() {
  if (is_expanded_) {
    HideDevices();
  } else {
    ShowDevices();
  }
  if (dropdown_button_) {
    dropdown_button_->SetToggled(is_expanded_);
  }

  if (media_item_ui_) {
    media_item_ui_->OnListViewSizeChanged();
  }
}

void MediaItemUIDeviceSelectorView::UpdateIsAudioDeviceSwitchingEnabled(
    bool enabled) {
  if (enabled == is_audio_device_switching_enabled_) {
    return;
  }
  is_audio_device_switching_enabled_ = enabled;
  UpdateVisibility();
}

void MediaItemUIDeviceSelectorView::RemoveDevicesOfType(
    DeviceEntryUIType type) {
  std::vector<views::View*> views_to_remove;
  for (views::View* view : device_entry_views_container_->children()) {
    if (GetDeviceEntryUI(view)->GetType() == type) {
      views_to_remove.push_back(view);
    }
  }
  for (auto* view : views_to_remove) {
    device_entry_ui_map_.erase(static_cast<views::Button*>(view)->tag());
    device_entry_views_container_->RemoveChildView(view);
    delete view;
  }
}

DeviceEntryUI* MediaItemUIDeviceSelectorView::GetDeviceEntryUI(
    views::View* view) const {
  auto it = device_entry_ui_map_.find(static_cast<views::Button*>(view)->tag());
  CHECK(it != device_entry_ui_map_.end(), base::NotFatalUntil::M130);
  return it->second;
}

void MediaItemUIDeviceSelectorView::OnDevicesUpdated(
    std::vector<global_media_controls::mojom::DevicePtr> devices) {
  RemoveDevicesOfType(DeviceEntryUIType::kCast);
  has_cast_device_ = false;
  for (const auto& device : devices) {
    has_cast_device_ = true;
    if (media_color_theme_.has_value()) {
      auto device_entry_view = std::make_unique<CastDeviceEntryViewAsh>(
          base::BindRepeating(
              &MediaItemUIDeviceSelectorView::OnCastDeviceSelected,
              base::Unretained(this), device->id),
          media_color_theme_.value().primary_foreground_color_id,
          media_color_theme_.value().secondary_foreground_color_id, device);
      device_entry_view->set_tag(next_tag_++);
      device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
      device_entry_views_container_->AddChildView(std::move(device_entry_view));
    } else {
      auto device_entry_view = std::make_unique<CastDeviceEntryView>(
          base::BindRepeating(
              &MediaItemUIDeviceSelectorView::OnCastDeviceSelected,
              base::Unretained(this), device->id),
          foreground_color_, background_color_, device);
      device_entry_view->set_tag(next_tag_++);
      device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
      auto* entry = device_entry_views_container_->AddChildView(
          std::move(device_entry_view));
      // After the |device_entry_view| is added, its icon color will change
      // according to the system theme. So we need to override the system color.
      entry->OnColorsChanged(foreground_color_, background_color_);
    }
  }
  device_entry_views_container_->DeprecatedLayoutImmediately();

  UpdateVisibility();
  observers_.Notify(
      &MediaItemUIDeviceSelectorObserver::OnMediaItemUIDeviceSelectorUpdated,
      device_entry_ui_map_);
  if (media_item_ui_) {
    media_item_ui_->OnDeviceSelectorViewDevicesChanged(
        device_entry_views_container_->children().size() > 0);
  }
}

void MediaItemUIDeviceSelectorView::OnDeviceSelected(int tag) {
  auto it = device_entry_ui_map_.find(tag);
  CHECK(it != device_entry_ui_map_.end(), base::NotFatalUntil::M130);

  if (it->second->GetType() == DeviceEntryUIType::kAudio) {
    delegate_->OnAudioSinkChosen(item_id_, it->second->raw_device_id());
  }
}

void MediaItemUIDeviceSelectorView::OnDropdownButtonClicked() {
  ShowOrHideDeviceList();
}

bool MediaItemUIDeviceSelectorView::IsDeviceSelectorExpanded() {
  return is_expanded_;
}

bool MediaItemUIDeviceSelectorView::OnMousePressed(
    const ui::MouseEvent& event) {
  if (entry_point_ !=
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    ShowOrHideDeviceList();
  }
  // Stop the mouse click event from bubbling to parent views.
  return true;
}

gfx::Size MediaItemUIDeviceSelectorView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int height = GetLayoutManager()->GetPreferredHeightForWidth(
      this, kExpandButtonStripWidth);
  int expand_button_strip_height =
      expand_button_strip_->GetVisible() ? kExpandButtonStripHeight : 0;
  return gfx::Size(kExpandButtonStripWidth,
                   std::max(expand_button_strip_height, height));
}

void MediaItemUIDeviceSelectorView::AddObserver(
    MediaItemUIDeviceSelectorObserver* observer) {
  observers_.AddObserver(observer);
}

views::Label*
MediaItemUIDeviceSelectorView::GetExpandDeviceSelectorLabelForTesting() {
  return expand_label_;
}

views::Button* MediaItemUIDeviceSelectorView::GetDropdownButtonForTesting() {
  return dropdown_button_;
}

std::string MediaItemUIDeviceSelectorView::GetEntryLabelForTesting(
    views::View* entry_view) {
  return GetDeviceEntryUI(entry_view)->device_name();
}

bool MediaItemUIDeviceSelectorView::GetEntryIsHighlightedForTesting(
    views::View* entry_view) {
  return GetDeviceEntryUI(entry_view)
      ->GetEntryIsHighlightedForTesting();  // IN-TEST
}

bool MediaItemUIDeviceSelectorView::GetDeviceEntryViewVisibilityForTesting() {
  return device_entry_views_container_->GetVisible();
}

std::vector<CastDeviceEntryView*>
MediaItemUIDeviceSelectorView::GetCastDeviceEntryViewsForTesting() {
  std::vector<CastDeviceEntryView*> buttons;
  for (views::View* view : device_entry_views_container_->children()) {
    if (GetDeviceEntryUI(view)->GetType() == DeviceEntryUIType::kCast) {
      buttons.push_back(static_cast<CastDeviceEntryView*>(view));
    }
  }
  return buttons;
}

void MediaItemUIDeviceSelectorView::OnCastDeviceSelected(
    const std::string& device_id) {
  if (device_list_host_) {
    device_list_host_->SelectDevice(device_id);
  }
}

void MediaItemUIDeviceSelectorView::RegisterAudioDeviceCallbacks() {
  // Get a list of the connected audio output devices.
  audio_device_subscription_ =
      delegate_->RegisterAudioOutputDeviceDescriptionsCallback(
          base::BindRepeating(
              &MediaItemUIDeviceSelectorView::UpdateAvailableAudioDevices,
              weak_ptr_factory_.GetWeakPtr()));

  // Get the availability of audio output device switching.
  is_device_switching_enabled_subscription_ =
      delegate_->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
          item_id_, base::BindRepeating(&MediaItemUIDeviceSelectorView::
                                            UpdateIsAudioDeviceSwitchingEnabled,
                                        weak_ptr_factory_.GetWeakPtr()));
}

BEGIN_METADATA(MediaItemUIDeviceSelectorView)
END_METADATA
