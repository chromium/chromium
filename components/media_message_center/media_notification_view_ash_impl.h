// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class Button;
class Label;
}  // namespace views

namespace media_message_center {

class MediaNotificationContainer;
class MediaNotificationItem;
class MediaArtworkView;
class MediaControlsProgressView;

namespace {
class MediaButton;
}

// CrOS implementation of media notification view.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewAshImpl
    : public MediaNotificationView {
 public:
  METADATA_HEADER(MediaNotificationViewAshImpl);

  MediaNotificationViewAshImpl(MediaNotificationContainer* container,
                               base::WeakPtr<MediaNotificationItem> item,
                               std::unique_ptr<views::View> dismiss_button,
                               absl::optional<NotificationTheme> theme);
  MediaNotificationViewAshImpl(const MediaNotificationViewAshImpl&) = delete;
  MediaNotificationViewAshImpl& operator=(const MediaNotificationViewAshImpl&) =
      delete;
  ~MediaNotificationViewAshImpl() override;

  // MediaNotificationView
  void SetForcedExpandedState(bool* forced_expanded_state) override {}
  void SetExpanded(bool expanded) override {}
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {}
  void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void UpdateWithMediaMetadata(
      const media_session::MediaMetadata& metadata) override;
  void UpdateWithMediaActions(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void UpdateWithMediaPosition(
      const media_session::MediaPosition& position) override;
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image) override;
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override {}
  void UpdateWithVectorIcon(const gfx::VectorIcon* vector_icon) override {}
  void UpdateDeviceSelectorAvailability(bool availability) override {}
  void UpdateWithMuteStatus(bool mute) override {}
  void UpdateWithVolume(float volume) override {}

 private:
  friend class MediaNotificationViewAshImplTest;

  MediaButton* CreateMediaButton(
      views::View* parent,
      media_session::mojom::MediaSessionAction action);

  void UpdateActionButtonsVisibility();

  // Callback for media action buttons.
  void ButtonPressed(views::Button* button);

  // Callback for progress view to update media position.
  void SeekTo(double seek_progress);

  // Raw pointer to the container holding this view. The |container_| should
  // never be nullptr.
  const raw_ptr<MediaNotificationContainer> container_;

  // Weak pointer to the media notification item associated with this view. The
  // |item_| should never be nullptr.
  base::WeakPtr<MediaNotificationItem> item_;

  // The color theme passed from ash. The |theme_| should always have a value.
  absl::optional<NotificationTheme> theme_;

  media_session::MediaPosition position_;

  // The list of action buttons in the view.
  std::vector<views::Button*> action_buttons_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  raw_ptr<MediaArtworkView> artwork_view_ = nullptr;
  raw_ptr<views::Label> source_label_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> artist_label_ = nullptr;
  raw_ptr<MediaControlsProgressView> progress_view_ = nullptr;
  raw_ptr<MediaButton> play_pause_button_ = nullptr;
  raw_ptr<MediaButton> picture_in_picture_button_ = nullptr;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_
