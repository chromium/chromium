// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_view.h"

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace global_media_controls {

namespace {

constexpr int kWidth = 400;
constexpr gfx::Insets kCrOSContainerInsets = gfx::Insets::TLBR(4, 16, 8, 16);

}  // anonymous namespace

MediaItemUIView::MediaItemUIView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    std::unique_ptr<MediaItemUIFooter> footer_view,
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
    media_message_center::MediaColorTheme media_color_theme,
    MediaDisplayPage media_display_page)
    : views::Button(PressedCallback()), id_(id) {
  CHECK(item);
  SetNotifyEnterExitOnChild(true);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_BACK_TO_TAB));

  // Pressing callback for the updated quick settings media view will be handled
  // in MediaItemUIDetailedView because it only wants to activate the original
  // web contents when media labels are pressed, but it also relies on the
  // button callback here to go to detailed media view.
  if (media_display_page == MediaDisplayPage::kQuickSettingsMediaView) {
    SetCallback(base::BindRepeating(&MediaItemUIView::ContainerClicked,
                                    base::Unretained(this),
                                    /*activate_original_media=*/false));
  } else {
    SetCallback(base::BindRepeating(&MediaItemUIView::ContainerClicked,
                                    base::Unretained(this),
                                    /*activate_original_media=*/true));
  }

  if (footer_view) {
    footer_view_ = footer_view.get();
  }
  if (device_selector_view) {
    device_selector_view->SetMediaItemUIView(this);
    device_selector_view_ = device_selector_view.get();
  }

  // Focus behavior will be set inside MediaItemUIDetailedView.
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(kCrOSContainerInsets));

  view_ = AddChildView(std::make_unique<MediaItemUIDetailedView>(
      this, std::move(item), std::move(footer_view),
      std::move(device_selector_view), /*dismiss_button=*/nullptr,
      media_color_theme, media_display_page));
}

MediaItemUIView::~MediaItemUIView() {
  for (auto& observer : observers_) {
    observer.OnMediaItemUIDestroyed(id_);
  }
  observers_.Clear();
}

void MediaItemUIView::OnGestureEvent(ui::GestureEvent* event) {
  // Tap gesture event should have the same behavior as a button click event, so
  // the button callback may be triggered.
  if (event->type() == ui::EventType::kGestureTap) {
    views::Button::OnGestureEvent(event);
  }
  if (scroll_view_ && event->IsScrollGestureEvent()) {
    scroll_view_->OnGestureEvent(event);
  }
}

gfx::Size MediaItemUIView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      kWidth, GetLayoutManager()->GetPreferredHeightForWidth(this, kWidth));
}

void MediaItemUIView::OnExpanded(bool expanded) {
  is_expanded_ = expanded;
  OnSizeChanged();
}

void MediaItemUIView::OnMediaSessionInfoChanged(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  if (session_info) {
    auto audio_sink_id = session_info->audio_sink_id.value_or(
        media::AudioDeviceDescription::kDefaultDeviceId);
    if (device_selector_view_ &&
        base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsSeamlessTransfer)) {
      device_selector_view_->UpdateCurrentAudioDevice(audio_sink_id);
    }
  }
}

void MediaItemUIView::OnMediaSessionMetadataChanged(
    const media_session::MediaMetadata& metadata) {
  title_ = metadata.title;

  for (auto& observer : observers_)
    observer.OnMediaItemUIMetadataChanged();
}

void MediaItemUIView::OnVisibleActionsChanged(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  for (auto& observer : observers_)
    observer.OnMediaItemUIActionsChanged();
}

void MediaItemUIView::OnColorsChanged(SkColor foreground,
                                      SkColor foreground_disabled,
                                      SkColor background) {
  if (foreground_color_ != foreground ||
      foreground_disabled_color_ != foreground_disabled) {
    foreground_color_ = foreground;
    foreground_disabled_color_ = foreground_disabled;
  }
  if (background_color_ != background) {
    background_color_ = background;
  }

  if (footer_view_)
    footer_view_->OnColorsChanged(foreground, background);
  if (device_selector_view_)
    device_selector_view_->OnColorsChanged(foreground, background);
}

void MediaItemUIView::OnHeaderClicked(bool activate_original_media) {
  // Since we disable the expand button, nothing happens on the
  // MediaNotificationView when the header is clicked. Treat the click as if we
  // were clicked directly.
  ContainerClicked(activate_original_media);
}

void MediaItemUIView::OnShowCastingDevicesRequested() {
  for (auto& observer : observers_) {
    observer.OnMediaItemUIShowDevices(id_);
  }
}

void MediaItemUIView::OnDeviceSelectorViewDevicesChanged(bool has_devices) {
  view_->UpdateDeviceSelectorAvailability(has_devices);
}

void MediaItemUIView::OnListViewSizeChanged() {
  OnSizeChanged();
}

void MediaItemUIView::AddObserver(
    global_media_controls::MediaItemUIObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaItemUIView::RemoveObserver(
    global_media_controls::MediaItemUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::u16string& MediaItemUIView::GetTitle() const {
  return title_;
}

void MediaItemUIView::SetScrollView(views::ScrollView* scroll_view) {
  scroll_view_ = scroll_view;
}

void MediaItemUIView::UpdateFooterView(
    std::unique_ptr<MediaItemUIFooter> footer_view) {
  footer_view_ = footer_view.get();
  view_->UpdateFooterView(std::move(footer_view));
}

void MediaItemUIView::UpdateDeviceSelector(
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view) {
  if (device_selector_view) {
    device_selector_view->SetMediaItemUIView(this);
  }
  device_selector_view_ = device_selector_view.get();
  view_->UpdateDeviceSelector(std::move(device_selector_view));
}

void MediaItemUIView::ContainerClicked(bool activate_original_media) {
  for (auto& observer : observers_) {
    observer.OnMediaItemUIClicked(id_, activate_original_media);
  }
}

void MediaItemUIView::OnSizeChanged() {
  if (device_selector_view_) {
    view_->UpdateDeviceSelectorVisibility(device_selector_view_->GetVisible());
  }

  for (auto& observer : observers_)
    observer.OnMediaItemUISizeChanged();
}

BEGIN_METADATA(MediaItemUIView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Title)
END_METADATA

}  // namespace global_media_controls
