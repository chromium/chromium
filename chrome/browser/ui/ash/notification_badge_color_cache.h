// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NOTIFICATION_BADGE_COLOR_CACHE_H_
#define CHROME_BROWSER_UI_ASH_NOTIFICATION_BADGE_COLOR_CACHE_H_

#include <map>
#include <string>

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class NotificationBadgeColorCache {
 public:
  // Returns a reference to a singleton instance of NotificationBadgeColorCache.
  static NotificationBadgeColorCache& GetInstance();

  NotificationBadgeColorCache();
  NotificationBadgeColorCache(const NotificationBadgeColorCache& other) =
      delete;
  NotificationBadgeColorCache& operator=(
      const NotificationBadgeColorCache& other) = delete;
  ~NotificationBadgeColorCache();

  // Calculate the color for the notification badge and cache it. If the
  // |app_id| already has a cached color then return that instead.
  SkColor GetBadgeColorForApp(const std::string& app_id, gfx::ImageSkia icon);

 private:
  using AppIdBadgeColor = std::map<std::string, SkColor>;
  AppIdBadgeColor app_id_badge_color_map_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NOTIFICATION_BADGE_COLOR_CACHE_H_
