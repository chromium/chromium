// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "ui/gfx/geometry/size.h"

// static
TabStripUILayout TabStripUILayout::CalculateForWebViewportSize(
    const gfx::Size& viewport_size) {
  // The smaller of the thumbnail's height or width is fixed to this
  // value. The other dimension will be at least this long.
  constexpr int kThumbnailMinDimensionLength = 120;

  TabStripUILayout layout;
  layout.padding_around_tab_list = 16;
  layout.tab_title_height = 40;
  layout.viewport_width = viewport_size.width();

  if (viewport_size.IsEmpty()) {
    layout.tab_thumbnail_size =
        gfx::Size(kThumbnailMinDimensionLength, kThumbnailMinDimensionLength);
    return layout;
  }

  // Size the thumbnail to match the web viewport's aspect ratio.
  // Limit aspect ratio to minimum of 1 to prevent thumbnails getting too
  // narrow.
  double ratio =
      std::max(1.0, 1.0 * viewport_size.width() / viewport_size.height());
  layout.tab_thumbnail_size.set_height(kThumbnailMinDimensionLength);
  layout.tab_thumbnail_size.set_width(kThumbnailMinDimensionLength * ratio);
  layout.tab_thumbnail_aspect_ratio = ratio;

  return layout;
}

base::Value TabStripUILayout::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("--tabstrip-tab-list-vertical-padding",
                    base::NumberToString(padding_around_tab_list) + "px");
  dict.SetStringKey("--tabstrip-tab-title-height",
                    base::NumberToString(tab_title_height) + "px");
  dict.SetStringKey("--tabstrip-tab-thumbnail-width",
                    base::NumberToString(tab_thumbnail_size.width()) + "px");
  dict.SetStringKey("--tabstrip-tab-thumbnail-height",
                    base::NumberToString(tab_thumbnail_size.height()) + "px");
  dict.SetStringKey("--tabstrip-tab-thumbnail-aspect-ratio",
                    base::NumberToString(tab_thumbnail_aspect_ratio));
  dict.SetStringKey("--tabstrip-viewport-width",
                    base::NumberToString(viewport_width) + "px");
  return dict;
}

int TabStripUILayout::CalculateContainerHeight() const {
  return 2 * padding_around_tab_list + tab_title_height +
         tab_thumbnail_size.height();
}
