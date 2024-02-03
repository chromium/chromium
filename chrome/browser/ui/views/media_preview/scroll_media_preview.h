// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_SCROLL_MEDIA_PREVIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_SCROLL_MEDIA_PREVIEW_H_

#include <optional>

namespace views {
class View;
}  // namespace views

namespace scroll_media_preview {

// Creats a scroll view as a child to `parent_view` and returns a view, which
// represetns the parent of all views within the scroll view.
views::View* CreateScrollViewAndGetContents(
    views::View& parent_view,
    std::optional<size_t> index = std::nullopt);

}  // namespace scroll_media_preview

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_SCROLL_MEDIA_PREVIEW_H_
