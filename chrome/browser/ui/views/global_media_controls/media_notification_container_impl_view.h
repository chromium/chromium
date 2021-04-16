// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/views/global_media_controls/global_media_controls_types.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view_delegate.h"
#include "chrome/browser/ui/views/global_media_controls/overlay_media_notification_view.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace views {
class LabelButton;
class ImageButton;
class SlideOutController;
}  // namespace views

class CastMediaNotificationItem;
class MediaNotificationDeviceSelectorView;
class MediaNotificationContainerObserver;
class MediaNotificationService;

// MediaNotificationContainerImplView holds a media notification for display
// within the MediaDialogView. The media notification shows metadata for a media
// session and can control playback.
class MediaNotificationContainerImplView
    : public views::Button,
      public media_message_center::MediaNotificationContainer,
      public MediaNotificationContainerImpl,
      public MediaNotificationDeviceSelectorViewDelegate,
      public views::SlideOutControllerDelegate,
      public views::FocusChangeListener {
 public:
  METADATA_HEADER(MediaNotificationContainerImplView);

  MediaNotificationContainerImplView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      MediaNotificationService* service,
      GlobalMediaControlsEntryPoint entry_point,
      base::Optional<media_message_center::NotificationTheme> theme =
          base::nullopt);
  MediaNotificationContainerImplView(
      const MediaNotificationContainerImplView&) = delete;
  MediaNotificationContainerImplView& operator=(
      const MediaNotificationContainerImplView&) = delete;
  ~MediaNotificationContainerImplView() override;

  // views::Button:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void OnMediaSessionMetadataChanged(
      const media_session::MediaMetadata& metadata) override;
  void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void OnMediaArtworkChanged(const gfx::ImageSkia& image) override;
  void OnColorsChanged(SkColor foreground, SkColor background) override;
  void OnHeaderClicked() override;

  // views::SlideOutControllerDelegate:
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideStarted() override {}
  void OnSlideChanged(bool in_progress) override {}
  void OnSlideOut() override;

  // MediaNotificationContainerImpl:
  void AddObserver(MediaNotificationContainerObserver* observer) override;
  void RemoveObserver(MediaNotificationContainerObserver* observer) override;

  // MediaNotificationDeviceSelectorViewDelegate
  // Called when an audio device has been selected for output.
  void OnAudioSinkChosen(const std::string& sink_id) override;
  void OnDeviceSelectorViewSizeChanged() override;
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) override;
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      base::RepeatingCallback<void(bool)> callback) override;

  // Sets up the notification to be ready to display in an overlay instead of
  // the dialog.
  void PopOut();

  // Called when overlay notification is shown and setup |overlay_|.
  void OnOverlayNotificationShown(OverlayMediaNotificationView* overlay);

  const std::u16string& GetTitle() const;

  views::ImageButton* GetDismissButtonForTesting();
  views::Button* GetStopCastingButtonForTesting();

  media_message_center::MediaNotificationViewImpl* view_for_testing() {
    DCHECK(!base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI));
    return static_cast<media_message_center::MediaNotificationViewImpl*>(view_);
  }

  bool is_playing_for_testing() { return is_playing_; }
  bool is_expanded_for_testing() { return is_expanded_; }

  views::Widget* drag_image_widget_for_testing() {
    return drag_image_widget_.get();
  }

 private:
  class DismissButton;

  void AddStopCastButton(CastMediaNotificationItem* item);
  void AddDeviceSelectorView(bool is_local_media_session);
  void StopCasting(CastMediaNotificationItem* item);
  void UpdateDismissButtonIcon();
  void UpdateDismissButtonBackground();
  void UpdateDismissButtonVisibility();
  void DismissNotification();
  void CreateDragImageWidget();
  // Updates the forced expanded state of |view_|.
  void ForceExpandedState();
  // Notify observers that we've been clicked.
  void ContainerClicked();
  // True if we should handle the given mouse event for dragging purposes.
  bool ShouldHandleMouseEvent(const ui::MouseEvent& event, bool is_press);
  void OnSizeChanged();

  const std::string id_;
  views::View* swipeable_container_ = nullptr;

  std::u16string title_;

  // Always "visible" so that it reserves space in the header so that the
  // dismiss button can appear without forcing things to shift.
  views::View* dismiss_button_placeholder_ = nullptr;

  // Shows the colored circle background behind the dismiss button to give it
  // proper contrast against the artwork. The background can't be on the dismiss
  // button itself because it messes up the ink drop.
  views::View* dismiss_button_container_ = nullptr;

  DismissButton* dismiss_button_ = nullptr;
  media_message_center::MediaNotificationView* view_ = nullptr;
  MediaNotificationDeviceSelectorView* device_selector_view_ = nullptr;

  // Only shows up for cast notifications.
  views::View* stop_button_strip_ = nullptr;
  views::LabelButton* stop_cast_button_ = nullptr;

  SkColor foreground_color_;
  SkColor background_color_;

  bool has_artwork_ = false;
  bool has_many_actions_ = false;

  // True if we've been dragged out of the dialog and into an overlay.
  bool dragged_out_ = false;

  // True if we're currently tracking a mouse drag. Used for dragging
  // notifications out into an overlay notification, not for swiping to dismiss
  // (see |slide_out_controller_| for swiping to dismiss).
  bool is_mouse_pressed_ = false;

  // The start point of a mouse drag. Used for dragging notifications out into
  // an overlay notification, not for swiping to dismiss (see
  // |slide_out_controller_| for swiping to dismiss).
  gfx::Point initial_drag_location_;

  // True if the current mouse press has been dragged enough to be considered a
  // drag instead of a button click.
  bool is_dragging_ = false;

  bool is_playing_ = false;

  bool is_expanded_ = false;

  std::string audio_sink_id_ = media::AudioDeviceDescription::kDefaultDeviceId;

  base::ObserverList<MediaNotificationContainerObserver> observers_;

  // Handles gesture events for swiping to dismiss notifications.
  std::unique_ptr<views::SlideOutController> slide_out_controller_;

  OverlayMediaNotificationView* overlay_ = nullptr;

  views::UniqueWidgetPtr drag_image_widget_;

  MediaNotificationService* const service_;

  const bool is_cros_;
  const GlobalMediaControlsEntryPoint entry_point_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_VIEW_H_
