// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "media/base/media_switches.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace views {
class ImageButton;
}  // namespace views

namespace global_media_controls {

enum class MediaDisplayPage;
class MediaItemUIObserver;

// MediaItemUIView holds a media notification for display
// within the MediaDialogView. The media notification shows metadata for a media
// session and can control playback.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIView
    : public views::Button,
      public media_message_center::MediaNotificationContainer,
      public global_media_controls::MediaItemUI,
      public views::FocusChangeListener {
  METADATA_HEADER(MediaItemUIView, views::Button)

 public:
  // MediaItemUIView is used in multiple places so some optional parameters may
  // not be set:
  // - Chrome OS media UI will set notification_theme for color theme.
  // - Chrome OS Zenith 2.0 media UI will set media_color_theme for color theme
  // and media_display_page for display page source.
  // - Chrome browser media UI will set none of them.
  MediaItemUIView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      std::unique_ptr<MediaItemUIFooter> footer_view,
      std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
      std::optional<media_message_center::NotificationTheme>
          notification_theme = std::nullopt,
      std::optional<media_message_center::MediaColorTheme> media_color_theme =
          std::nullopt,
      std::optional<MediaDisplayPage> media_display_page = std::nullopt);
  MediaItemUIView(const MediaItemUIView&) = delete;
  MediaItemUIView& operator=(const MediaItemUIView&) = delete;
  ~MediaItemUIView() override;

  // views::Button:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void OnMediaSessionMetadataChanged(
      const media_session::MediaMetadata& metadata) override;
  void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void OnMediaArtworkChanged(const gfx::ImageSkia& image) override;
  void OnColorsChanged(SkColor foreground,
                       SkColor foreground_disabled,
                       SkColor background) override;
  void OnHeaderClicked(bool activate_original_media) override;
  void OnShowCastingDevicesRequested() override;
  void OnListViewSizeChanged() override;

  // global_media_controls::MediaItemUI:
  void AddObserver(
      global_media_controls::MediaItemUIObserver* observer) override;
  void RemoveObserver(
      global_media_controls::MediaItemUIObserver* observer) override;

  // Called when the devices in the device selector view have changed.
  void OnDeviceSelectorViewDevicesChanged(bool has_devices);

  const std::u16string& GetTitle() const;

  // Set the scroll view that is currently holding this item.
  void SetScrollView(views::ScrollView* scroll_view);

  // Remove and add new views.
  void UpdateDeviceSelector(
      std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view);
  void UpdateFooterView(std::unique_ptr<MediaItemUIFooter> footer_view);

  views::ImageButton* GetDismissButtonForTesting();

  media_message_center::MediaNotificationViewImpl* view_for_testing() {
    return static_cast<media_message_center::MediaNotificationViewImpl*>(view_);
  }
  MediaItemUIDeviceSelector* device_selector_view_for_testing() {
    return device_selector_view_;
  }
  MediaItemUIFooter* footer_view_for_testing() { return footer_view_; }

  bool is_playing_for_testing() { return is_playing_; }
  bool is_expanded_for_testing() { return is_expanded_; }

 private:
  class DismissButton;

  void UpdateDismissButtonIcon();
  void UpdateDismissButtonBackground();
  void UpdateDismissButtonVisibility();
  void DismissNotification();
  // Updates the forced expanded state of |view_|.
  void ForceExpandedState();
  // Notify observers that we've been clicked.
  void ContainerClicked(bool activate_original_media);
  void OnSizeChanged();

  const std::string id_;

  std::u16string title_;

  // The scroll view that is currently holding this item.
  raw_ptr<views::ScrollView, DanglingUntriaged> scroll_view_ = nullptr;

  // Always "visible" so that it reserves space in the header so that the
  // dismiss button can appear without forcing things to shift.
  raw_ptr<views::View> dismiss_button_placeholder_ = nullptr;

  // Shows the colored circle background behind the dismiss button to give it
  // proper contrast against the artwork. The background can't be on the dismiss
  // button itself because it messes up the ink drop.
  raw_ptr<views::View> dismiss_button_container_ = nullptr;

  raw_ptr<DismissButton> dismiss_button_ = nullptr;
  raw_ptr<media_message_center::MediaNotificationView> view_ = nullptr;

  raw_ptr<MediaItemUIFooter> footer_view_ = nullptr;
  raw_ptr<MediaItemUIDeviceSelector> device_selector_view_ = nullptr;

  SkColor foreground_color_ = kDefaultForegroundColor;
  SkColor foreground_disabled_color_ = kDefaultForegroundColor;
  SkColor background_color_ = kDefaultBackgroundColor;

  bool has_artwork_ = false;
  bool has_many_actions_ = false;

  bool is_playing_ = false;

  bool is_expanded_ = false;

  base::ObserverList<global_media_controls::MediaItemUIObserver> observers_;

  // Sets to true when the notification theme is provided on Chrome OS.
  const bool has_notification_theme_;

  // Sets to true if the updated UI is enabled.
  bool use_updated_ui_ = false;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_
