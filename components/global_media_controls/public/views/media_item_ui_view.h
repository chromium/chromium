// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/media_notification_container.h"
#include "media/base/media_switches.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace global_media_controls {

enum class MediaDisplayPage;
class MediaItemUIObserver;

// MediaItemUIView displays metadata for a media session in CrOS and can control
// media playback.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIView
    : public views::Button,
      public media_message_center::MediaNotificationContainer,
      public global_media_controls::MediaItemUI {
  METADATA_HEADER(MediaItemUIView, views::Button)

 public:
  MediaItemUIView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      std::unique_ptr<MediaItemUIFooter> footer_view,
      std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
      media_message_center::MediaColorTheme media_color_theme,
      MediaDisplayPage media_display_page);
  MediaItemUIView(const MediaItemUIView&) = delete;
  MediaItemUIView& operator=(const MediaItemUIView&) = delete;
  ~MediaItemUIView() override;

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override;
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void OnMediaSessionMetadataChanged(
      const media_session::MediaMetadata& metadata) override;
  void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void OnMediaArtworkChanged(const gfx::ImageSkia& image) override {}
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

  MediaItemUIDetailedView* view_for_testing() { return view_; }
  MediaItemUIDeviceSelector* device_selector_view_for_testing() {
    return device_selector_view_;
  }
  MediaItemUIFooter* footer_view_for_testing() { return footer_view_; }

 private:
  // Notify observers that we've been clicked.
  void ContainerClicked(bool activate_original_media);
  void OnSizeChanged();

  const std::string id_;

  std::u16string title_;

  // The scroll view that is currently holding this item.
  raw_ptr<views::ScrollView, DanglingUntriaged> scroll_view_ = nullptr;

  raw_ptr<MediaItemUIDetailedView> view_ = nullptr;
  raw_ptr<MediaItemUIFooter> footer_view_ = nullptr;
  raw_ptr<MediaItemUIDeviceSelector> device_selector_view_ = nullptr;

  SkColor foreground_color_ = kDefaultForegroundColor;
  SkColor foreground_disabled_color_ = kDefaultForegroundColor;
  SkColor background_color_ = kDefaultBackgroundColor;

  bool is_expanded_ = false;

  base::ObserverList<global_media_controls::MediaItemUIObserver> observers_;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_VIEW_H_
