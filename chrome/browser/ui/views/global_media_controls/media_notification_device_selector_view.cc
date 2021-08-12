// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view_delegate.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom-shared.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/box_layout.h"

using media_router::mojom::MediaRouteProviderId;

namespace {

// Constants for the MediaNotificationDeviceSelectorView
constexpr gfx::Insets kExpandButtonStripInsets{6, 15};
constexpr gfx::Size kExpandButtonStripSize{400, 30};
constexpr gfx::Insets kExpandButtonBorderInsets{4, 8};

// The maximum number of audio devices to count when recording the
// Media.GlobalMediaControls.NumberOfAvailableAudioDevices histogram. 30 was
// chosen because it would be very unlikely to see a user with 30+ audio
// devices.
const int kAudioDevicesCountHistogramMax = 30;

media_router::MediaRouterDialogOpenOrigin ConvertToOrigin(
    GlobalMediaControlsEntryPoint entry_point) {
  switch (entry_point) {
    case GlobalMediaControlsEntryPoint::kPresentation:
      return media_router::MediaRouterDialogOpenOrigin::PAGE;
    case GlobalMediaControlsEntryPoint::kSystemTray:
      return media_router::MediaRouterDialogOpenOrigin::SYSTEM_TRAY;
    case GlobalMediaControlsEntryPoint::kToolbarIcon:
      return media_router::MediaRouterDialogOpenOrigin::TOOLBAR;
  }
}

void RecordCastDeviceCountMetrics(GlobalMediaControlsEntryPoint entry_point,
                                  std::vector<CastDeviceEntryView*> entries) {
  media_router::MediaRouterMetrics::RecordDeviceCount(entries.size());

  std::map<MediaRouteProviderId, std::map<bool, int>> counts = {
      {MediaRouteProviderId::CAST, {{true, 0}, {false, 0}}},
      {MediaRouteProviderId::DIAL, {{true, 0}, {false, 0}}},
      {MediaRouteProviderId::WIRED_DISPLAY, {{true, 0}, {false, 0}}}};
  for (const CastDeviceEntryView* entry : entries) {
    if (entry->sink().provider != MediaRouteProviderId::TEST) {
      counts.at(entry->sink().provider).at(entry->GetEnabled())++;
    }
  }
  for (auto provider : {MediaRouteProviderId::CAST, MediaRouteProviderId::DIAL,
                        MediaRouteProviderId::WIRED_DISPLAY}) {
    for (bool is_available : {true, false}) {
      int count = counts.at(provider).at(is_available);
      media_router::MediaRouterMetrics::RecordGmcDeviceCount(
          ConvertToOrigin(entry_point), provider, is_available, count);
    }
  }
}

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
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(true);

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
    std::unique_ptr<media_router::CastDialogController> cast_controller,
    bool has_audio_output,
    const std::string& current_device_id,
    const SkColor& foreground_color,
    const SkColor& background_color,
    GlobalMediaControlsEntryPoint entry_point,
    bool show_expand_button)
    : delegate_(delegate),
      current_device_id_(current_device_id),
      foreground_color_(foreground_color),
      background_color_(background_color),
      entry_point_(entry_point) {
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
  expand_button_->SetCallback(base::BindRepeating(
      &MediaNotificationDeviceSelectorView::ExpandButtonPressed,
      base::Unretained(this)));

  if (!show_expand_button)
    expand_button_strip_->SetVisible(false);

  device_entry_views_container_ = AddChildView(std::make_unique<views::View>());
  device_entry_views_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  device_entry_views_container_->SetVisible(false);

  if (entry_point_ == GlobalMediaControlsEntryPoint::kPresentation) {
    ShowDevices();
  }
  SetBackground(views::CreateSolidBackground(background_color_));
  // Set the size of this view
  SetPreferredSize(kExpandButtonStripSize);
  Layout();

  // This view will become visible when devices are discovered.
  SetVisible(false);

  if (has_audio_output && base::FeatureList::IsEnabled(
                              media::kGlobalMediaControlsSeamlessTransfer)) {
    RegisterAudioDeviceCallbacks();
  }

  if (cast_controller) {
    cast_controller_ = std::move(cast_controller);
    cast_controller_->AddObserver(this);
  }
}

void MediaNotificationDeviceSelectorView::UpdateCurrentAudioDevice(
    const std::string& current_device_id) {
  if (current_audio_device_entry_view_)
    current_audio_device_entry_view_->SetHighlighted(false);

  // Find DeviceEntryView* from |device_entry_ui_map_| with |current_device_id|.
  auto it = base::ranges::find_if(
      device_entry_ui_map_, [&current_device_id](const auto& pair) {
        return pair.second->raw_device_id() == current_device_id;
      });

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
  current_audio_device_entry_view_->Layout();
}

MediaNotificationDeviceSelectorView::~MediaNotificationDeviceSelectorView() {
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

void MediaNotificationDeviceSelectorView::UpdateAvailableAudioDevices(
    const media::AudioDeviceDescriptions& device_descriptions) {
  RemoveDevicesOfType(DeviceEntryUIType::kAudio);
  current_audio_device_entry_view_ = nullptr;

  bool current_device_still_exists = false;
  for (auto description : device_descriptions) {
    auto device_entry_view = std::make_unique<AudioDeviceEntryView>(
        base::BindRepeating(
            &MediaNotificationDeviceSelectorViewDelegate::OnAudioSinkChosen,
            base::Unretained(delegate_), description.unique_id),
        foreground_color_, background_color_, description.unique_id,
        description.device_name);
    device_entry_view->set_tag(next_tag_++);
    device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
    device_entry_views_container_->AddChildView(std::move(device_entry_view));
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
  for (auto& observer : observers_)
    observer.OnMediaNotificationDeviceSelectorUpdated(device_entry_ui_map_);
}

void MediaNotificationDeviceSelectorView::OnColorsChanged(
    const SkColor& foreground_color,
    const SkColor& background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;

  SetBackground(views::CreateSolidBackground(background_color_));
  for (auto it : device_entry_ui_map_) {
    it.second->OnColorsChanged(foreground_color_, background_color_);
  }

  expand_button_->OnColorsChanged();

  SchedulePaint();
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
MediaNotificationDeviceSelectorView::GetExpandButtonForTesting() {
  return expand_button_;
}

std::string MediaNotificationDeviceSelectorView::GetEntryLabelForTesting(
    views::View* entry_view) {
  return GetDeviceEntryUI(entry_view)->device_name();
}

bool MediaNotificationDeviceSelectorView::GetEntryIsHighlightedForTesting(
    views::View* entry_view) {
  return GetDeviceEntryUI(entry_view)->GetEntryIsHighlightedForTesting();
}

std::vector<media_router::CastDialogSinkButton*>
MediaNotificationDeviceSelectorView::GetCastSinkButtonsForTesting() {
  std::vector<media_router::CastDialogSinkButton*> buttons;
  for (auto* view : device_entry_views_container_->children()) {
    if (GetDeviceEntryUI(view)->GetType() == DeviceEntryUIType::kCast) {
      buttons.push_back(static_cast<media_router::CastDialogSinkButton*>(view));
    }
  }
  return buttons;
}

void MediaNotificationDeviceSelectorView::ShowDevices() {
  DCHECK(!is_expanded_);
  is_expanded_ = true;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_SHOW_DEVICE_LIST));

  if (!have_devices_been_shown_) {
    base::UmaHistogramExactLinear(
        kAudioDevicesCountHistogramName,
        device_entry_views_container_->children().size(),
        kAudioDevicesCountHistogramMax);
    base::UmaHistogramBoolean(kDeviceSelectorOpenedHistogramName, true);
    RecordCastDeviceCountAfterDelay();
    have_devices_been_shown_ = true;
  }

  device_entry_views_container_->SetVisible(true);
  PreferredSizeChanged();
}

void MediaNotificationDeviceSelectorView::HideDevices() {
  DCHECK(is_expanded_);
  is_expanded_ = false;
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_HIDE_DEVICE_LIST));

  device_entry_views_container_->SetVisible(false);
  PreferredSizeChanged();
}

void MediaNotificationDeviceSelectorView::UpdateVisibility() {
  SetVisible(ShouldBeVisible());

  if (!has_expand_button_been_shown_ && GetVisible()) {
    base::UmaHistogramBoolean(kDeviceSelectorAvailableHistogramName, true);
    has_expand_button_been_shown_ = true;
  }

  delegate_->OnDeviceSelectorViewSizeChanged();
}

bool MediaNotificationDeviceSelectorView::ShouldBeVisible() const {
  if (has_cast_device_)
    return true;
  if (!is_audio_device_switching_enabled_)
    return false;
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

void MediaNotificationDeviceSelectorView::ExpandButtonPressed() {
  if (is_expanded_)
    HideDevices();
  else
    ShowDevices();
  delegate_->OnDeviceSelectorViewSizeChanged();
}

void MediaNotificationDeviceSelectorView::UpdateIsAudioDeviceSwitchingEnabled(
    bool enabled) {
  if (enabled == is_audio_device_switching_enabled_)
    return;

  is_audio_device_switching_enabled_ = enabled;
  UpdateVisibility();
}

void MediaNotificationDeviceSelectorView::RemoveDevicesOfType(
    DeviceEntryUIType type) {
  std::vector<views::View*> views_to_remove;
  for (auto* view : device_entry_views_container_->children()) {
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

DeviceEntryUI* MediaNotificationDeviceSelectorView::GetDeviceEntryUI(
    views::View* view) const {
  auto it = device_entry_ui_map_.find(static_cast<views::Button*>(view)->tag());
  DCHECK(it != device_entry_ui_map_.end());
  return it->second;
}

void MediaNotificationDeviceSelectorView::OnModelUpdated(
    const media_router::CastDialogModel& model) {
  RemoveDevicesOfType(DeviceEntryUIType::kCast);
  has_cast_device_ = false;
  for (auto sink : model.media_sinks()) {
    if (!base::Contains(sink.cast_modes,
                        media_router::MediaCastMode::PRESENTATION)) {
      continue;
    }
    has_cast_device_ = true;
    auto device_entry_view = std::make_unique<CastDeviceEntryView>(
        base::BindRepeating(
            &MediaNotificationDeviceSelectorView::StartCastSession,
            base::Unretained(this)),
        foreground_color_, background_color_, sink);
    device_entry_view->set_tag(next_tag_++);
    device_entry_ui_map_[device_entry_view->tag()] = device_entry_view.get();
    auto* entry = device_entry_views_container_->AddChildView(
        std::move(device_entry_view));
    // After the |device_entry_view| is added, its icon color will change
    // according to the system theme. So we need to override the system color.
    entry->OnColorsChanged(foreground_color_, background_color_);
  }
  device_entry_views_container_->Layout();

  UpdateVisibility();
  for (auto& observer : observers_)
    observer.OnMediaNotificationDeviceSelectorUpdated(device_entry_ui_map_);
}

void MediaNotificationDeviceSelectorView::OnControllerInvalidated() {
  cast_controller_.reset();
}

void MediaNotificationDeviceSelectorView::OnDeviceSelected(int tag) {
  auto it = device_entry_ui_map_.find(tag);
  DCHECK(it != device_entry_ui_map_.end());

  if (it->second->GetType() == DeviceEntryUIType::kAudio)
    delegate_->OnAudioSinkChosen(it->second->raw_device_id());
  else
    StartCastSession(static_cast<CastDeviceEntryView*>(it->second));
}

void MediaNotificationDeviceSelectorView::OnDropdownButtonClicked() {
  ExpandButtonPressed();
}

bool MediaNotificationDeviceSelectorView::IsDeviceSelectorExpanded() {
  return is_expanded_;
}

void MediaNotificationDeviceSelectorView::AddObserver(
    MediaNotificationDeviceSelectorObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationDeviceSelectorView::StartCastSession(
    CastDeviceEntryView* entry) {
  if (!cast_controller_)
    return;
  const media_router::UIMediaSink& sink = entry->sink();
  // Clicking on the device entry with an issue will clear the issue without
  // starting casting.
  if (sink.issue) {
    cast_controller_->ClearIssue(sink.issue->id());
    return;
  }
  // When users click on a CONNECTED sink,
  // if it is a CAST sink, a new cast session will replace the existing cast
  // session.
  // if it is a DIAL sink, the existing session will be terminated and users
  // need to click on the sink again to start a new session.
  // TODO(crbug.com/1206830): implement "terminate existing route and start a
  // new session" in DIAL MRP.
  if (sink.state == media_router::UIMediaSinkState::AVAILABLE) {
    DoStartCastSession(sink);
  } else if (sink.state == media_router::UIMediaSinkState::CONNECTED) {
    // We record stopping casting here even if we are starting casting, because
    // the existing session is being stopped and replaced by a new session.
    RecordStopCastingMetrics();
    if (sink.provider == media_router::mojom::MediaRouteProviderId::DIAL) {
      DCHECK(sink.route);
      cast_controller_->StopCasting(sink.route->media_route_id());
    } else {
      DoStartCastSession(sink);
    }
  }
}
void MediaNotificationDeviceSelectorView::DoStartCastSession(
    const media_router::UIMediaSink& sink) {
  DCHECK(base::Contains(sink.cast_modes,
                        media_router::MediaCastMode::PRESENTATION));
  cast_controller_->StartCasting(sink.id,
                                 media_router::MediaCastMode::PRESENTATION);
  RecordStartCastingMetrics();
}

void MediaNotificationDeviceSelectorView::RecordStartCastingMetrics() {
  GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point_) {
    case GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStartViaToolbarIcon;
      break;
    case GlobalMediaControlsEntryPoint::kPresentation:
      action =
          GlobalMediaControlsCastActionAndEntryPoint::kStartViaPresentation;
      break;
    case GlobalMediaControlsEntryPoint::kSystemTray:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStartViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(
      media_message_center::MediaNotificationItem::kCastStartStopHistogramName,
      action);
}

void MediaNotificationDeviceSelectorView::RecordStopCastingMetrics() {
  GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point_) {
    case GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaToolbarIcon;
      break;
    case GlobalMediaControlsEntryPoint::kPresentation:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaPresentation;
      break;
    case GlobalMediaControlsEntryPoint::kSystemTray:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(
      media_message_center::MediaNotificationItem::kCastStartStopHistogramName,
      action);
}

void MediaNotificationDeviceSelectorView::RecordCastDeviceCountAfterDelay() {
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &MediaNotificationDeviceSelectorView::RecordCastDeviceCount,
          weak_ptr_factory_.GetWeakPtr()),
      media_router::MediaRouterMetrics::kDeviceCountMetricDelay);
}

void MediaNotificationDeviceSelectorView::RecordCastDeviceCount() {
  std::vector<CastDeviceEntryView*> entries;
  for (views::View* view : device_entry_views_container_->children()) {
    DeviceEntryUI* entry = GetDeviceEntryUI(view);
    if (entry->GetType() == DeviceEntryUIType::kCast) {
      entries.push_back(static_cast<CastDeviceEntryView*>(entry));
    }
  }
  RecordCastDeviceCountMetrics(entry_point_, entries);
}

void MediaNotificationDeviceSelectorView::RegisterAudioDeviceCallbacks() {
  // Get a list of the connected audio output devices.
  audio_device_subscription_ =
      delegate_->RegisterAudioOutputDeviceDescriptionsCallback(
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

BEGIN_METADATA(MediaNotificationDeviceSelectorView, views::View)
END_METADATA
