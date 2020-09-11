// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_

#include <map>
#include <memory>

#include "base/optional.h"
#include "ui/views/controls/scroll_view.h"

class MediaNotificationContainerImplView;
class OverlayMediaNotification;

// MediaNotificationListView is a container that holds a list of active media
// sessions.
class MediaNotificationListView : public views::ScrollView {
 public:
  struct SeparatorStyle {
    SeparatorStyle(SkColor separator_color, int separator_thickness);

    const SkColor separator_color;
    const int separator_thickness;
  };

  explicit MediaNotificationListView(
      const base::Optional<SeparatorStyle>& separator_style);
  MediaNotificationListView();
  ~MediaNotificationListView() override;

  // Adds the given notification into the list.
  void ShowNotification(
      const std::string& id,
      std::unique_ptr<MediaNotificationContainerImplView> notification);

  // Removes the given notification from the list.
  void HideNotification(const std::string& id);

  // Removes the given notification from the list and returns an
  // OverlayMediaNotificationView that contains it.
  std::unique_ptr<OverlayMediaNotification> PopOut(const std::string& id,
                                                   gfx::Rect bounds);

  bool empty() { return notifications_.empty(); }

  const std::map<const std::string, MediaNotificationContainerImplView*>&
  notifications_for_testing() const {
    return notifications_;
  }

 private:
  std::unique_ptr<MediaNotificationContainerImplView> RemoveNotification(
      const std::string& id);

  std::map<const std::string, MediaNotificationContainerImplView*>
      notifications_;

  base::Optional<SeparatorStyle> separator_style_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationListView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_
