// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"

#include "base/strings/string_number_conversions.h"
#include "ui/gfx/geometry/size.h"

namespace {

// The height of the thumbnail.
constexpr int kThumbnailHeight = 120;

// The padding of the tab item to the top/bottom of the tab strip.
constexpr int kPaddingAroundTabList = 16;

// The height of the tab title.
constexpr int kTabTitleHeight = 40;

}  // namespace

// static
TabStripUILayout TabStripUILayout::CalculateForWebViewportSize(
    const gfx::Size& viewport_size) {
  TabStripUILayout layout;
  layout.viewport_width = viewport_size.width();

  if (viewport_size.IsEmpty()) {
    layout.tab_thumbnail_size = gfx::Size(kThumbnailHeight, kThumbnailHeight);
    return layout;
  }

  // Size the thumbnail to match the web viewport's aspect ratio.
  // Limit aspect ratio to minimum of 1 to prevent thumbnails getting too
  // narrow.
  double ratio =
      std::max(1.0, 1.0 * viewport_size.width() / viewport_size.height());
  layout.tab_thumbnail_size.set_height(kThumbnailHeight);
  layout.tab_thumbnail_size.set_width(kThumbnailHeight * ratio);
  layout.tab_thumbnail_aspect_ratio = ratio;

  return layout;
}

// static
int TabStripUILayout::GetContainerHeight() {
  return 2 * kPaddingAroundTabList + kTabTitleHeight + kThumbnailHeight;
}

base::flat_map<std::string, std::string> TabStripUILayout::AsDictionary()
    const {
  return base::flat_map<std::string, std::string>(
      {{"--tabstrip-tab-list-vertical-padding",
        base::NumberToString(kPaddingAroundTabList) + "px"},
       {"--tabstrip-tab-title-height",
        base::NumberToString(kTabTitleHeight) + "px"},
       {"--tabstrip-tab-thumbnail-width",
        base::NumberToString(tab_thumbnail_size.width()) + "px"},
       {"--tabstrip-tab-thumbnail-height",
        base::NumberToString(tab_thumbnail_size.height()) + "px"},
       {"--tabstrip-tab-thumbnail-aspect-ratio",
        base::NumberToString(tab_thumbnail_aspect_ratio)},
       {"--tabstrip-viewport-width",
        base::NumberToString(viewport_width) + "px"}});
}
