// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_STRING_MAP_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_STRING_MAP_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"

// This class is used to map the keys of the JSON response from the wallpaper
// search API to the strings that will be displayed in the UI.
class WallpaperSearchStringMap {
 public:
  using Factory =
      base::RepeatingCallback<std::unique_ptr<WallpaperSearchStringMap>()>;
  static std::unique_ptr<WallpaperSearchStringMap> Create();
  static void SetFactory(Factory factory);

  virtual ~WallpaperSearchStringMap() = default;

  virtual std::optional<std::string> FindCategory(std::string_view key) const;
  virtual std::optional<std::string> FindDescriptorA(
      std::string_view key) const;
  virtual std::optional<std::string> FindDescriptorB(
      std::string_view key) const;
  virtual std::optional<std::string> FindDescriptorC(
      std::string_view key) const;
  virtual std::optional<std::string> FindInspirationDescription(
      std::string_view key) const;

 protected:
  WallpaperSearchStringMap() = default;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_STRING_MAP_H_
