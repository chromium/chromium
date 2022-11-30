// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_ICON_COLOR_CACHE_H_
#define CHROME_BROWSER_UI_ASH_APP_ICON_COLOR_CACHE_H_

#include <map>
#include <string>

#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AppIconColorCache {
 public:
  // Returns a reference to a singleton instance of AppIconColorCache.
  static AppIconColorCache& GetInstance();

  AppIconColorCache(const AppIconColorCache& other) = delete;
  AppIconColorCache& operator=(const AppIconColorCache& other) = delete;
  ~AppIconColorCache();

  // Calculate the vibrant color for the app icon and cache it. If the |app_id|
  // already has a cached color then return that instead.
  SkColor GetLightVibrantColorForApp(const std::string& app_id,
                                     gfx::ImageSkia icon);

 private:
  friend class base::NoDestructor<AppIconColorCache>;
  AppIconColorCache();

  using AppIdLightVibrantColor = std::map<std::string, SkColor>;
  AppIdLightVibrantColor app_id_light_vibrant_color_map_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_APP_ICON_COLOR_CACHE_H_
