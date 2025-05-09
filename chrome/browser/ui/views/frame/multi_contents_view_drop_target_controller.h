// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"

namespace content {
struct DropData;
}  // namespace content

// `MultiContentsViewDropTargetController` is responsible for handling
// the drag-entrypoint of a single `MultiContentsView`. This includes dragging
// links,  bookmarks, or tab headers to create a split view.
// There exists one `MultiContentsViewDropTargetController` per
// `MultiContentesView`.
class MultiContentsViewDropTargetController final {
 public:
  explicit MultiContentsViewDropTargetController(views::View& drop_target_view);
  ~MultiContentsViewDropTargetController() = default;
  MultiContentsViewDropTargetController(
      const MultiContentsViewDropTargetController&) = delete;
  MultiContentsViewDropTargetController& operator=(
      const MultiContentsViewDropTargetController&) = delete;

  // Handles a drag within the web contents area.
  // `point` should be relative to the multi contents view.
  void OnWebContentsDragUpdate(const content::DropData& data,
                               const gfx::PointF& point);
  void OnWebContentsDragExit();

 private:
  // The view that is displayed when drags hover over the "drop" region of
  // the content area.
  const raw_ref<views::View> drop_target_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_
