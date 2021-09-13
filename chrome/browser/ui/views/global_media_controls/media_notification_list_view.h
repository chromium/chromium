// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_

#include <map>
#include <memory>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"

class MediaNotificationContainerImplView;

// MediaNotificationListView is a container that holds a list of active media
// sessions.
class MediaNotificationListView : public views::ScrollView {
 public:
  METADATA_HEADER(MediaNotificationListView);
  struct SeparatorStyle {
    SeparatorStyle(SkColor separator_color, int separator_thickness);

    const SkColor separator_color;
    const int separator_thickness;
  };

  explicit MediaNotificationListView(
      const absl::optional<SeparatorStyle>& separator_style);
  MediaNotificationListView();
  MediaNotificationListView(const MediaNotificationListView&) = delete;
  MediaNotificationListView& operator=(const MediaNotificationListView&) =
      delete;
  ~MediaNotificationListView() override;

  // Adds the given notification into the list.
  void ShowNotification(
      const std::string& id,
      std::unique_ptr<MediaNotificationContainerImplView> notification);

  // Removes the given notification from the list.
  void HideNotification(const std::string& id);

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

  absl::optional<SeparatorStyle> separator_style_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_LIST_VIEW_H_
