// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_view.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace views {
class SlideOutController;
}  // namespace views

class MediaNotificationContainerObserver;

// MediaNotificationContainerImplView holds a media notification for display
// within the MediaDialogView. The media notification shows metadata for a media
// session and can control playback.
class MediaNotificationContainerImplView
    : public views::Button,
      public media_message_center::MediaNotificationContainer,
      public MediaNotificationContainerImpl,
      public views::SlideOutControllerDelegate,
      public views::ButtonListener,
      public views::FocusChangeListener {
 public:
  MediaNotificationContainerImplView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item);
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
      const media_session::mojom::MediaSessionInfoPtr& session_info) override {}
  void OnMediaSessionMetadataChanged() override;
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

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // MediaNotificationContainerImpl:
  void AddObserver(MediaNotificationContainerObserver* observer) override;
  void RemoveObserver(MediaNotificationContainerObserver* observer) override;

  // Sets up the notification to be ready to display in an overlay instead of
  // the dialog.
  void PopOut();

  views::ImageButton* GetDismissButtonForTesting();

  media_message_center::MediaNotificationView* view_for_testing() {
    return view_;
  }

 private:
  class DismissButton;

  void UpdateDismissButtonIcon();

  void UpdateDismissButtonBackground();

  void UpdateDismissButtonVisibility();

  void DismissNotification();

  // Updates the forced expanded state of |view_|.
  void ForceExpandedState();

  // Notify observers that we've been clicked.
  void ContainerClicked();

  // True if we should handle the given mouse event for dragging purposes.
  bool ShouldHandleMouseEvent(const ui::MouseEvent& event, bool is_press);

  const std::string id_;
  views::View* swipeable_container_ = nullptr;

  // Always "visible" so that it reserves space in the header so that the
  // dismiss button can appear without forcing things to shift.
  views::View* dismiss_button_placeholder_ = nullptr;

  // Shows the colored circle background behind the dismiss button to give it
  // proper contrast against the artwork. The background can't be on the dismiss
  // button itself because it messes up the ink drop.
  views::View* dismiss_button_container_ = nullptr;

  DismissButton* dismiss_button_ = nullptr;
  media_message_center::MediaNotificationView* view_ = nullptr;

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

  base::ObserverList<MediaNotificationContainerObserver> observers_;

  // Handles gesture events for swiping to dismiss notifications.
  std::unique_ptr<views::SlideOutController> slide_out_controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImplView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_VIEW_H_
