// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_MULTI_CAPTURE_NOTIFICATION_DETAILS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_MULTI_CAPTURE_NOTIFICATION_DETAILS_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace multi_capture {

// This view shows a detail dialog informing the user which apps are allowed to
// use the multi screen capture API (`getAllScreensMedia`). It is shown when the
// user clicks on "View details" in the notification that is shown on login if
// any app is allowed to use the API.
class MultiCaptureNotificationDetailsView : public views::View {
  METADATA_HEADER(MultiCaptureNotificationDetailsView, views::View)

 public:
  struct AppInfo {
    AppInfo(const std::string& name, const gfx::ImageSkia& icon);
    ~AppInfo();

    std::string name;
    gfx::ImageSkia icon;
  };

  MultiCaptureNotificationDetailsView(
      const std::vector<AppInfo>& apps_with_notification,
      const std::vector<AppInfo>& apps_without_notification);
  ~MultiCaptureNotificationDetailsView() override;

  static void ShowCaptureDetails(
      const std::vector<AppInfo>& app_names_with_notification,
      const std::vector<AppInfo>& app_names_without_notification);

 private:
  void ShowAppListAllWithNotification(
      const std::vector<AppInfo>& app_names_with_notification);
  void ShowAppListNoneWithNotification(
      const std::vector<AppInfo>& app_names_without_notification);
  void ShowAppListsWitMixedhNotifications(
      const std::vector<AppInfo>& app_names_with_notification,
      const std::vector<AppInfo>& app_names_without_notification);
  void CloseWidget();

  base::WeakPtrFactory<MultiCaptureNotificationDetailsView> weak_ptr_factory_{
      this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_MULTI_CAPTURE_NOTIFICATION_DETAILS_VIEW_H_
