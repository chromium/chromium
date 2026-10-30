// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_KNOWN_RESOURCE_IMAGES_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_KNOWN_RESOURCE_IMAGES_H_

#include <optional>
#include <string_view>

namespace ui {
class ImageModel;
}  // namespace ui

namespace webui_toolbar {

// If `image` is an image loaded from the ResourceBundle that the WebUI end can
// load directly via a fixed URL (e.g. chrome://theme/IDR_...), returns that
// URL. Otherwise returns std::nullopt.
std::optional<std::string_view> GetKnownResourceImageUrl(
    const ui::ImageModel& image);

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_KNOWN_RESOURCE_IMAGES_H_
