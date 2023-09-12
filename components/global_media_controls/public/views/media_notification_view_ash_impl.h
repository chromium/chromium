// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
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
}  // namespace media_message_center

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace global_media_controls {

namespace {
class MediaButton;
}

// Indicates this media notification view will be displayed on which page.
enum class MediaDisplayPage {
  // Media will be displayed on the Quick Settings media view page.
  kQuickSettingsMediaView = 0,
  // Media will be displayed on the Quick Settings media detailed view page.
  kQuickSettingsMediaDetailedView = 1,
  // Media will be displayed on the lock screen view.
  kLockScreenMediaView = 2,
};

// CrOS implementation of media notification view.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaNotificationViewAshImpl
    : public media_message_center::MediaNotificationView {
 public:
  METADATA_HEADER(MediaNotificationViewAshImpl);

  MediaNotificationViewAshImpl(
      media_message_center::MediaNotificationContainer* container,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      std::unique_ptr<MediaItemUIFooter> footer_view,
      std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
      std::unique_ptr<views::View> dismiss_button,
      media_message_center::MediaColorTheme theme,
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
  void UpdateWithMuteStatus(bool mute) override {}
  void UpdateWithVolume(float volume) override {}
  void UpdateDeviceSelectorVisibility(bool visible) override {}
  void UpdateDeviceSelectorAvailability(bool has_devices) override;

  // views::View:
  void AddedToWidget() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Helper functions for testing:
  views::ImageView* GetArtworkViewForTesting();
  views::Label* GetSourceLabelForTesting();
  views::Label* GetArtistLabelForTesting();
  views::Label* GetTitleLabelForTesting();
  views::ImageView* GetChevronIconForTesting();
  views::Button* GetActionButtonForTesting(
      media_session::mojom::MediaSessionAction action);
  media_session::MediaPosition GetPositionForTesting();
  views::Button* GetStartCastingButtonForTesting();
  MediaItemUIFooter* GetFooterForTesting();
  MediaItemUIDeviceSelector* GetDeviceSelectorForTesting();
  views::View* GetDeviceSelectorSeparatorForTesting();

 private:
  friend class MediaNotificationViewAshImplTest;

  MediaButton* CreateMediaButton(views::View* parent,
                                 int button_id,
                                 const gfx::VectorIcon& vector_icon,
                                 int tooltip_text_id);

  void UpdateActionButtonsVisibility();

  // Callback for media action buttons.
  void ButtonPressed(views::Button* button);

  // Callback for the user dragging the squiggly progress view. A playing media
  // should be temporarily paused when the user is dragging the progress line.
  void OnProgressDragging(bool pause);

  // Callback for when the media squiggly progress view wants to update the
  // progress position.
  void SeekTo(double seek_progress);

  // Callback for when the start casting button is toggled by user.
  void StartCastingButtonPressed();

  // Update the display states of UI elements for casting devices.
  void UpdateCastingState();

  // Raw pointer to the container holding this view. The |container_| should
  // never be nullptr.
  const raw_ptr<media_message_center::MediaNotificationContainer> container_;

  // Weak pointer to the media notification item associated with this view. The
  // |item_| should never be nullptr.
  base::WeakPtr<media_message_center::MediaNotificationItem> item_;

  // The color theme for all the colors in this view.
  media_message_center::MediaColorTheme theme_;

  // The display page source for this view.
  MediaDisplayPage media_display_page_;

  media_session::MediaPosition position_;

  // The list of action buttons in the view.
  std::vector<views::Button*> action_buttons_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Whether the media is currently in picture-in-picture.
  bool in_picture_in_picture_ = false;

  raw_ptr<views::ImageView> artwork_view_ = nullptr;
  raw_ptr<views::Label> source_label_ = nullptr;
  raw_ptr<views::Label> artist_label_ = nullptr;

  raw_ptr<views::BoxLayoutView> title_row_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::ImageView> chevron_icon_ = nullptr;

  raw_ptr<media_message_center::MediaSquigglyProgressView>
      squiggly_progress_view_ = nullptr;
  raw_ptr<MediaButton> play_pause_button_ = nullptr;
  raw_ptr<MediaButton> start_casting_button_ = nullptr;
  raw_ptr<MediaButton> picture_in_picture_button_ = nullptr;

  raw_ptr<MediaItemUIFooter> footer_view_ = nullptr;
  raw_ptr<MediaItemUIDeviceSelector> device_selector_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> device_selector_view_separator_ = nullptr;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_NOTIFICATION_VIEW_ASH_IMPL_H_
