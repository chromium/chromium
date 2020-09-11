// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_

#include "components/media_message_center/media_notification_view.h"

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/media_message_center/media_notification_view.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace views {
class ToggleImageButton;
}  // namespace views

namespace media_message_center {

namespace {
class MediaArtworkView;
}  // anonymous namespace

class MediaNotificationBackground;
class MediaNotificationContainer;
class MediaNotificationItem;

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewModernImpl
    : public MediaNotificationView,
      public views::ButtonListener {
 public:
  // The name of the histogram used when recording whether the artwork was
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

  MediaNotificationViewModernImpl(
      MediaNotificationContainer* container,
      base::WeakPtr<MediaNotificationItem> item,
      std::unique_ptr<views::View> notification_controls_view,
      int notification_width);
  MediaNotificationViewModernImpl(const MediaNotificationViewModernImpl&) =
      delete;
  MediaNotificationViewModernImpl& operator=(
      const MediaNotificationViewModernImpl&) = delete;
  ~MediaNotificationViewModernImpl() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

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
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image) override;
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override;
  void UpdateWithVectorIcon(const gfx::VectorIcon& vector_icon) override {}
  void UpdateDeviceSelectorAvailability(bool availability) override;

  // Testing methods
  const views::Label* title_label_for_testing() const { return title_label_; }

  const views::Label* subtitle_label_for_testing() const {
    return subtitle_label_;
  }

  const views::Button* picture_in_picture_button_for_testing() const {
    return picture_in_picture_button_;
  }

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
                         media_session::mojom::MediaSessionAction action,
                         const base::string16& accessible_name);

  void UpdateActionButtonsVisibility();

  MediaNotificationBackground* GetMediaNotificationBackground();

  void UpdateForegroundColor();

  // Container that receives events.
  MediaNotificationContainer* const container_;

  // Keeps track of media metadata and controls the session when buttons are
  // clicked.
  base::WeakPtr<MediaNotificationItem> item_;

  bool has_artwork_ = false;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Stores the text to be read by screen readers describing the notification.
  // Contains the title, artist and album separated by hyphens.
  base::string16 accessible_name_;

  MediaNotificationBackground* background_;

  // Container views directly attached to this view.
  views::View* artwork_container_ = nullptr;
  MediaArtworkView* artwork_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* subtitle_label_ = nullptr;
  views::ToggleImageButton* picture_in_picture_button_ = nullptr;
  views::View* notification_controls_spacer_ = nullptr;
  views::View* media_controls_container_ = nullptr;
  views::ToggleImageButton* play_pause_button_ = nullptr;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_
