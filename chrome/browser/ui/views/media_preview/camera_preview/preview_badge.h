// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_PREVIEW_BADGE_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_PREVIEW_BADGE_H_

#include <memory>

namespace views {
class View;
}  // namespace views

namespace preview_badge {

// Creates a preview badge with icon.
std::unique_ptr<views::View> CreatePreviewBadge();

}  // namespace preview_badge

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_PREVIEW_BADGE_H_
