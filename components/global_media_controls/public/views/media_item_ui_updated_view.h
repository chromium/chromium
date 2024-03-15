// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_UPDATED_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_UPDATED_VIEW_H_

#include "components/global_media_controls/public/media_item_ui.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/notification_theme.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace views {
class Button;
class ImageView;
class Label;
}  // namespace views

namespace global_media_controls {

class MediaActionButton;
class MediaItemUIObserver;

// MediaItemUIUpdatedView holds the media information and playback controls for
// a media session or cast session. This will be displayed within
// MediaDialogView on non-CrOS desktop platforms and replace MediaItemUIView and
// MediaItemUIDetailedView when the media::kGlobalMediaControlsUpdatedUI flag is
// enabled.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIUpdatedView
    : public MediaItemUI,
      public media_message_center::MediaNotificationView {
  METADATA_HEADER(MediaItemUIUpdatedView, views::View)

 public:
  MediaItemUIUpdatedView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      media_message_center::MediaColorTheme media_color_theme);
  MediaItemUIUpdatedView(const MediaItemUIUpdatedView&) = delete;
  MediaItemUIUpdatedView& operator=(const MediaItemUIUpdatedView&) = delete;
  ~MediaItemUIUpdatedView() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // MediaItemUI:
  void AddObserver(MediaItemUIObserver* observer) override;
  void RemoveObserver(MediaItemUIObserver* observer) override;

  // media_message_center::MediaNotificationView:
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
  void UpdateWithChapterArtwork(int index,
                                const gfx::ImageSkia& image) override {}
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override {}
  void UpdateWithVectorIcon(const gfx::VectorIcon* vector_icon) override {}
  void UpdateWithMuteStatus(bool mute) override {}
  void UpdateWithVolume(float volume) override {}
  void UpdateDeviceSelectorVisibility(bool visible) override {}
  void UpdateDeviceSelectorAvailability(bool has_devices) override {}

  // Helper functions for testing:
  views::ImageView* GetArtworkViewForTesting();
  views::Label* GetSourceLabelForTesting();
  views::Label* GetTitleLabelForTesting();
  views::Label* GetArtistLabelForTesting();

 private:
  MediaActionButton* CreateMediaActionButton(views::View* parent,
                                             int button_id,
                                             const gfx::VectorIcon& vector_icon,
                                             int tooltip_text_id);

  // Callback for a media action button being pressed.
  void MediaActionButtonPressed(views::Button* button);

  // Whether the media is currently in picture-in-picture.
  bool in_picture_in_picture_ = false;

  base::ObserverList<MediaItemUIObserver> observers_;

  raw_ptr<views::ImageView> artwork_view_ = nullptr;
  raw_ptr<views::Label> source_label_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> artist_label_ = nullptr;

  std::vector<MediaActionButton*> media_action_buttons_;
  raw_ptr<MediaActionButton> picture_in_picture_button_ = nullptr;
  raw_ptr<MediaActionButton> play_pause_button_ = nullptr;

  const std::string id_;
  base::WeakPtr<media_message_center::MediaNotificationItem> item_;
  media_message_center::MediaColorTheme media_color_theme_;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_UPDATED_VIEW_H_
