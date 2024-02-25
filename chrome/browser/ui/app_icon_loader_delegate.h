// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_ICON_LOADER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_ICON_LOADER_DELEGATE_H_

#include <optional>
#include <string>

namespace gfx {
class ImageSkia;
}

class AppIconLoaderDelegate {
 public:
  // Called when the image for an app is loaded.
  // 'image' is the main app image, `badge_image` if set is the badge that
  // should be painted on top of the main image for certain app types
  // (currently, `badge_image` will be set for app shortcuts).
  // 'is_placeholder_icon' is true if the main app image is a placeholder icon.
  // A promise app may have a placeholder icon while the IconLoader finishes
  // resolving the resource or is unable to fetch one.
  virtual void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) = 0;

 protected:
  virtual ~AppIconLoaderDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_APP_ICON_LOADER_DELEGATE_H_
