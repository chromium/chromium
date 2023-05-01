// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class BoxLayoutView;
class Button;
class ImageView;
class Label;
}  // namespace views

namespace media_message_center {

class MediaNotificationContainer;
class MediaNotificationItem;
class MediaSquigglyProgressView;

namespace {
class MediaButton;
}

// Indicates this media notification view will be displayed on which page.
enum class MediaDisplayPage {
  // Media will be display on the Quick Settings media view page.
  kQuickSettingsMediaView = 0,
  // Media will be display on the Quick Settings media detailed view page.
  kQuickSettingsMediaDetailedView = 1,
};

// CrOS implementation of media notification view.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewAshImpl
    : public MediaNotificationView {
 public:
  METADATA_HEADER(MediaNotificationViewAshImpl);

  MediaNotificationViewAshImpl(MediaNotificationContainer* container,
                               base::WeakPtr<MediaNotificationItem> item,
                               MediaColorTheme theme,
                               MediaDisplayPage media_display_page);
  MediaNotificationViewAshImpl(const MediaNotificationViewAshImpl&) = delete;
  MediaNotificationViewAshImpl& operator=(const MediaNotificationViewAshImpl&) =
      delete;
  ~MediaNotificationViewAshImpl() override;

  // MediaNotificationView:
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

  // Helper functions for testing:
  views::ImageView* GetArtworkViewForTesting();
  views::Label* GetSourceLabelForTesting();
  views::Label* GetArtistLabelForTesting();
  views::Label* GetTitleLabelForTesting();
  views::ImageView* GetChevronIconForTesting();

 private:
  friend class MediaNotificationViewAshImplTest;

  MediaButton* CreateMediaButton(
      views::View* parent,
      media_session::mojom::MediaSessionAction action);

  void UpdateActionButtonsVisibility();

  // Callback for media action buttons.
  void ButtonPressed(views::Button* button);

  // Callback for when the media squiggly progress view wants to update the
  // progress position.
  void SeekTo(double seek_progress);

  // Raw pointer to the container holding this view. The |container_| should
  // never be nullptr.
  const raw_ptr<MediaNotificationContainer> container_;

  // Weak pointer to the media notification item associated with this view. The
  // |item_| should never be nullptr.
  base::WeakPtr<MediaNotificationItem> item_;

  // The color theme for all the colors in this view.
  MediaColorTheme theme_;

  media_session::MediaPosition position_;

  // The list of action buttons in the view.
  std::vector<views::Button*> action_buttons_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  raw_ptr<views::ImageView> artwork_view_ = nullptr;
  raw_ptr<views::Label> source_label_ = nullptr;
  raw_ptr<views::Label> artist_label_ = nullptr;

  raw_ptr<views::BoxLayoutView> title_row_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::ImageView> chevron_icon_ = nullptr;

  raw_ptr<MediaSquigglyProgressView> squiggly_progress_view_ = nullptr;
  raw_ptr<MediaButton> play_pause_button_ = nullptr;
  raw_ptr<MediaButton> picture_in_picture_button_ = nullptr;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_
