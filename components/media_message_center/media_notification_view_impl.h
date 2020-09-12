// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_IMPL_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/media_message_center/media_notification_view.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

namespace message_center {
class NotificationHeaderView;
}  // namespace message_center

namespace views {
class BoxLayout;
class ToggleImageButton;
}  // namespace views

namespace media_message_center {

class MediaNotificationBackground;
class MediaNotificationContainer;
class MediaNotificationItem;

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewImpl
    : public MediaNotificationView,
      public views::ButtonListener {
 public:
  // The name of the histogram used when recorded whether the artwork was
  // present.
  static const char kArtworkHistogramName[];

  // The name of the histogram used when recording the type of metadata that was
  // displayed.
  static const char kMetadataHistogramName[];

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

  // Allow MediaNotificationViewImpl show different styled background.
  enum class BackgroundStyle {
    kDefault,
    kAshStyle,
  };

  MediaNotificationViewImpl(
      MediaNotificationContainer* container,
      base::WeakPtr<MediaNotificationItem> item,
      std::unique_ptr<views::View> header_row_controls_view,
      const base::string16& default_app_name,
      int notification_width,
      bool should_show_icon,
      BackgroundStyle background_style = BackgroundStyle::kDefault);
  ~MediaNotificationViewImpl() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

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
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image) override;
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override;
  void UpdateWithVectorIcon(const gfx::VectorIcon& vector_icon) override;
  void UpdateDeviceSelectorAvailability(bool availability) override;

  void OnThemeChanged() override;

  const views::Label* title_label_for_testing() const { return title_label_; }

  const views::Label* artist_label_for_testing() const { return artist_label_; }

  const views::Button* picture_in_picture_button_for_testing() const {
    return picture_in_picture_button_;
  }

  const views::View* playback_button_container_for_testing() const {
    return playback_button_container_;
  }

  std::vector<views::View*> get_buttons_for_testing() { return GetButtons(); }

  views::Button* GetHeaderRowForTesting() const;
  base::string16 GetSourceTitleForTesting() const;

 private:
  friend class MediaNotificationViewImplTest;

  // Creates an image button with an icon that matches |action| and adds it
  // to |button_row_|. When clicked it will trigger |action| on the session.
  // |accessible_name| is the text used for screen readers and the
  // button's tooltip.
  void CreateMediaButton(media_session::mojom::MediaSessionAction action,
                         const base::string16& accessible_name);

  void UpdateActionButtonsVisibility();
  void UpdateViewForExpandedState();

  MediaNotificationBackground* GetMediaNotificationBackground();

  bool IsExpandable() const;
  bool IsActuallyExpanded() const;

  void UpdateForegroundColor();

  // Returns the buttons contained in the button row and playback button
  // container.
  std::vector<views::View*> GetButtons();

  // Container that receives OnExpanded events.
  MediaNotificationContainer* const container_;

  // Keeps track of media metadata and controls the session when buttons are
  // clicked.
  base::WeakPtr<MediaNotificationItem> item_;

  // Optional View that is put into the header row. E.g. in Ash we show
  // notification control buttons.
  views::View* header_row_controls_view_ = nullptr;

  // String to set as the app name of the header when there is no source title.
  base::string16 default_app_name_;

  // Width of the notification in pixels. Used for calculating artwork bounds.
  int notification_width_;

  bool has_artwork_ = false;

  // Whether this notification is expanded or not.
  bool expanded_ = false;

  // Used to force the notification to remain in a specific expanded state.
  base::Optional<bool> forced_expanded_state_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Stores the text to be read by screen readers describing the notification.
  // Contains the title, artist and album separated by hyphens.
  base::string16 accessible_name_;

  // Container views directly attached to this view.
  message_center::NotificationHeaderView* header_row_ = nullptr;
  views::View* button_row_ = nullptr;
  views::View* playback_button_container_ = nullptr;
  views::View* pip_button_separator_view_ = nullptr;
  views::ToggleImageButton* play_pause_button_ = nullptr;
  views::ToggleImageButton* picture_in_picture_button_ = nullptr;
  views::View* title_artist_row_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* artist_label_ = nullptr;
  views::View* layout_row_ = nullptr;
  views::View* main_row_ = nullptr;

  views::BoxLayout* title_artist_row_layout_ = nullptr;
  const gfx::VectorIcon* vector_header_icon_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationViewImpl);
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_IMPL_H_
