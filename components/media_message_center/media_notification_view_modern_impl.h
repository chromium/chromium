// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace views {
class Button;
class ToggleImageButton;
}  // namespace views

namespace media_message_center {

namespace {
class MediaButton;
}  // anonymous namespace

class MediaArtworkView;
class MediaControlsProgressView;
class MediaNotificationBackground;
class MediaNotificationContainer;
class MediaNotificationItem;
class MediaNotificationVolumeSliderView;

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewModernImpl
    : public MediaNotificationView {
 public:
  METADATA_HEADER(MediaNotificationViewModernImpl);

  // The type of metadata that was displayed. This is used in metrics so new
  // values must only be added to the end.
  enum class Metadata {
    kTitle,
    kArtist,
    kAlbum,
    kCount,
    kSource,
    kMaxValue = kSource,
  };

  MediaNotificationViewModernImpl(
      MediaNotificationContainer* container,
      base::WeakPtr<MediaNotificationItem> item,
      std::unique_ptr<views::View> notification_controls_view,
      std::unique_ptr<views::View> notification_footer_view,
      int notification_width,
      absl::optional<NotificationTheme> theme = absl::nullopt);
  MediaNotificationViewModernImpl(const MediaNotificationViewModernImpl&) =
      delete;
  MediaNotificationViewModernImpl& operator=(
      const MediaNotificationViewModernImpl&) = delete;
  ~MediaNotificationViewModernImpl() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // MediaNotificationView
  void SetForcedExpandedState(bool* forced_expanded_state) override {}
  void SetExpanded(bool expanded) override {}
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
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
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override;
  void UpdateWithVectorIcon(const gfx::VectorIcon* vector_icon) override {}
  void UpdateWithMuteStatus(bool mute) override;
  void UpdateWithVolume(float volume) override;
  void UpdateDeviceSelectorVisibility(bool visible) override;
  void UpdateDeviceSelectorAvailability(bool has_devices) override {}

  // Testing methods
  const views::Label* title_label_for_testing() const { return title_label_; }

  const views::Label* subtitle_label_for_testing() const {
    return subtitle_label_;
  }

  views::Button* picture_in_picture_button_for_testing() const;

  const views::View* media_controls_container_for_testing() const {
    return media_controls_container_;
  }

 private:
  friend class MediaNotificationViewModernImplTest;

  // Creates an image button with an icon that matches |action| and adds it
  // to |parent_view|. When clicked it will trigger |action| on the session.
  // |accessible_name| is the text used for screen readers and the
  // button's tooltip.
  void CreateMediaButton(views::View* parent_view,
                         media_session::mojom::MediaSessionAction action);

  void UpdateActionButtonsVisibility();

  MediaNotificationBackground* GetMediaNotificationBackground();

  void UpdateForegroundColor();

  void ButtonPressed(views::Button* button);

  void SeekTo(double seek_progress);

  void OnMuteButtonClicked();

  void SetVolume(float volume);

  // Container that receives events.
  const raw_ptr<MediaNotificationContainer> container_;

  // Keeps track of media metadata and controls the session when buttons are
  // clicked.
  base::WeakPtr<MediaNotificationItem> item_;

  bool has_artwork_ = false;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  raw_ptr<MediaNotificationBackground> background_;

  media_session::MediaPosition position_;

  // Container views directly attached to this view.
  raw_ptr<views::View> artwork_container_ = nullptr;
  raw_ptr<MediaArtworkView> artwork_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> subtitle_label_ = nullptr;
  raw_ptr<MediaButton> picture_in_picture_button_ = nullptr;
  raw_ptr<views::View> notification_controls_spacer_ = nullptr;
  raw_ptr<views::View> media_controls_container_ = nullptr;
  raw_ptr<MediaButton> play_pause_button_ = nullptr;
  raw_ptr<MediaControlsProgressView> progress_ = nullptr;
  raw_ptr<views::ToggleImageButton> mute_button_ = nullptr;
  raw_ptr<MediaNotificationVolumeSliderView> volume_slider_ = nullptr;

  absl::optional<NotificationTheme> theme_;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_
