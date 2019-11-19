// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace media_session {
struct MediaMetadata;
enum class MediaSessionAction;
}  // namespace media_session

namespace message_center {
class NotificationHeaderView;
}  // namespace message_center

namespace views {
class BoxLayout;
class ToggleImageButton;
class View;
}  // namespace views

namespace media_message_center {

class MediaNotificationBackground;
class MediaNotificationContainer;
class MediaNotificationItem;

// MediaNotificationView will show up as a custom view. It will show the
// currently playing media and provide playback controls.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationView
    : public views::View,
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
    kMaxValue = kCount,
  };

  MediaNotificationView(MediaNotificationContainer* container,
                        base::WeakPtr<MediaNotificationItem> item,
                        std::unique_ptr<views::View> header_row_controls_view,
                        const base::string16& default_app_name,
                        int notification_width,
                        bool should_show_icon);
  ~MediaNotificationView() override;

  void SetExpanded(bool expanded);
  void UpdateCornerRadius(int top_radius, int bottom_radius);

  // When |forced_expanded_state| has a value, the notification will be forced
  // into that expanded state and the user won't be given a button to toggle the
  // expanded state. Subsequent |SetExpanded()| calls will be ignored until
  // |SetForcedExpandedState(nullptr)| is called.
  void SetForcedExpandedState(bool* forced_expanded_state);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info);
  void UpdateWithMediaMetadata(const media_session::MediaMetadata& metadata);
  void UpdateWithMediaActions(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions);
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image);

  const views::Label* title_label_for_testing() const { return title_label_; }

  const views::Label* artist_label_for_testing() const { return artist_label_; }

  views::Button* GetHeaderRowForTesting() const;

 private:
  friend class MediaNotificationViewTest;

  // Creates an image button with an icon that matches |action| and adds it
  // to |button_row_|. When clicked it will trigger |action| on the session.
  // |accessible_name| is the text used for screen readers.
  void CreateMediaButton(media_session::mojom::MediaSessionAction action,
                         const base::string16& accessible_name);

  void UpdateActionButtonsVisibility();
  void UpdateViewForExpandedState();

  MediaNotificationBackground* GetMediaNotificationBackground();

  bool IsExpandable() const;
  bool IsActuallyExpanded() const;

  void UpdateForegroundColor();

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
  // Contains the title, artist and album separated by hypens.
  base::string16 accessible_name_;

  // Container views directly attached to this view.
  message_center::NotificationHeaderView* header_row_ = nullptr;
  views::View* button_row_ = nullptr;
  views::ToggleImageButton* play_pause_button_ = nullptr;
  views::View* title_artist_row_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* artist_label_ = nullptr;
  views::View* layout_row_ = nullptr;
  views::View* main_row_ = nullptr;

  views::BoxLayout* title_artist_row_layout_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationView);
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_H_
