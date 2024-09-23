// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_IMPL_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

namespace message_center {
class NotificationHeaderView;
}  // namespace message_center

namespace views {
class BoxLayout;
class Button;
class ToggleImageButton;
}  // namespace views

namespace media_message_center {

class MediaNotificationBackground;
class MediaNotificationContainer;
class MediaNotificationItem;

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewImpl
    : public MediaNotificationView {
  METADATA_HEADER(MediaNotificationViewImpl, MediaNotificationView)

 public:
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

  MediaNotificationViewImpl(
      MediaNotificationContainer* container,
      base::WeakPtr<MediaNotificationItem> item,
      std::unique_ptr<views::View> header_row_controls_view,
      const std::u16string& default_app_name,
      int notification_width,
      bool should_show_icon,
      std::optional<NotificationTheme> theme = std::nullopt);
  MediaNotificationViewImpl(const MediaNotificationViewImpl&) = delete;
  MediaNotificationViewImpl& operator=(const MediaNotificationViewImpl&) =
      delete;
  ~MediaNotificationViewImpl() override;

  // MediaNotificationView:
  void SetExpanded(bool expanded) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void SetForcedExpandedState(bool* forced_expanded_state) override;
  void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void UpdateWithMediaMetadata(
      const media_session::MediaMetadata& metadata) override;
  void UpdateWithMediaActions(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void UpdateWithMediaPosition(
      const media_session::MediaPosition& position) override {}
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image) override;
  // This view is used before `media::kGlobalMediaControlsCrOSUpdatedUI` is
  // enabled in the `MediaItemUIView`. So we don't need to implement this method
  // since there's no chapter images in this view.
  void UpdateWithChapterArtwork(int index,
                                const gfx::ImageSkia& image) override {}
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override;
  void UpdateWithVectorIcon(const gfx::VectorIcon* vector_icon) override;
  void UpdateWithMuteStatus(bool mute) override {}
  void UpdateWithVolume(float volume) override {}
  void UpdateDeviceSelectorVisibility(bool visible) override;
  void UpdateDeviceSelectorAvailability(bool has_devices) override {}

  void OnThemeChanged() override;

  const views::Label* title_label_for_testing() const { return title_label_; }

  const views::Label* artist_label_for_testing() const { return artist_label_; }

  const views::Button* picture_in_picture_button_for_testing() const {
    return picture_in_picture_button_;
  }

  const views::View* playback_button_container_for_testing() const {
    return playback_button_container_;
  }

  std::vector<raw_ptr<views::View, VectorExperimental>>
  get_buttons_for_testing() {
    return GetButtons();
  }

  views::Button* GetHeaderRowForTesting() const;
  std::u16string GetSourceTitleForTesting() const;

 private:
  friend class MediaNotificationViewImplTest;

  // Creates an image button with an icon that matches |action| and adds it
  // to |button_row_|. When clicked it will trigger |action| on the session.
  // |accessible_name| is the text used for screen readers and the
  // button's tooltip.
  void CreateMediaButton(media_session::mojom::MediaSessionAction action,
                         const std::u16string& accessible_name);

  void CreateHeaderRow(std::unique_ptr<views::View> header_row_controls_view,
                       bool should_show_icon);
  void CreateCrOSHeaderRow(
      std::unique_ptr<views::View> header_row_controls_view);

  void UpdateActionButtonsVisibility();
  void UpdateViewForExpandedState();

  MediaNotificationBackground* GetMediaNotificationBackground();

  bool GetExpandable() const;
  bool GetActuallyExpanded() const;

  void UpdateForegroundColor();

  void ButtonPressed(views::Button* button);

  void MaybeShowOrHideArtistLabel();

  // Returns the buttons contained in the button row and playback button
  // container.
  std::vector<raw_ptr<views::View, VectorExperimental>> GetButtons();

  // Container that receives OnExpanded events.
  const raw_ptr<MediaNotificationContainer> container_;

  // Keeps track of media metadata and controls the session when buttons are
  // clicked.
  base::WeakPtr<MediaNotificationItem> item_;

  // Optional View that is put into the header row. E.g. in Ash we show
  // notification control buttons.
  raw_ptr<views::View> header_row_controls_view_ = nullptr;

  // String to set as the app name of the header when there is no source title.
  std::u16string default_app_name_;

  // Width of the notification in pixels. Used for calculating artwork bounds.
  int notification_width_;

  bool has_artwork_ = false;

  // Whether this notification is expanded or not.
  bool expanded_ = false;

  // Used to force the notification to remain in a specific expanded state.
  std::optional<bool> forced_expanded_state_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Container views directly attached to this view.
  raw_ptr<message_center::NotificationHeaderView> header_row_ = nullptr;
  raw_ptr<views::Label> cros_header_label_ = nullptr;
  raw_ptr<views::View> button_row_ = nullptr;
  raw_ptr<views::View> playback_button_container_ = nullptr;
  raw_ptr<views::View> pip_button_separator_view_ = nullptr;
  raw_ptr<views::ToggleImageButton> play_pause_button_ = nullptr;
  raw_ptr<views::ToggleImageButton> picture_in_picture_button_ = nullptr;
  raw_ptr<views::View> title_artist_row_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> artist_label_ = nullptr;
  raw_ptr<views::View> layout_row_ = nullptr;
  raw_ptr<views::View> main_row_ = nullptr;

  raw_ptr<views::BoxLayout> title_artist_row_layout_ = nullptr;
  raw_ptr<const gfx::VectorIcon> vector_header_icon_ = nullptr;

  std::optional<NotificationTheme> theme_;

  const bool is_cros_;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_IMPL_H_
