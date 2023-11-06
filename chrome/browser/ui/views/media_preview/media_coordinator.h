// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_COORDINATOR_H_

#include <stddef.h>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class View;
}  // namespace views

// MediaCoordinator sets up the media views.
class MediaCoordinator {
 public:
  enum class ViewType { kBoth, kCameraOnly, kMicOnly };

  MediaCoordinator(ViewType view_type,
                   views::View& parent_view,
                   absl::optional<size_t> index,
                   bool is_subsection);
  MediaCoordinator(const MediaCoordinator&) = delete;
  MediaCoordinator& operator=(const MediaCoordinator&) = delete;
  ~MediaCoordinator();
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_COORDINATOR_H_
