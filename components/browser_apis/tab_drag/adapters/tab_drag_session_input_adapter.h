// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_SESSION_INPUT_ADAPTER_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragWindowAdapter;

struct TabDragInputEvent {
  enum class Type {
    kMoved,
    kCancelled,
    kDropped,
    kCaptureChanged,
  };
  Type type;
  gfx::Point screen_point;
};

using EventCallback =
    base::RepeatingCallback<void(const TabDragInputEvent& event)>;

class TabDragSessionInputAdapter {
 public:
  virtual ~TabDragSessionInputAdapter() = default;

  // Starts capturing input on the platform.
  virtual base::expected<void, mojo_base::mojom::ErrorPtr> StartInputCapture(
      EventCallback callback,
      TabDragWindowAdapter* initial_window) = 0;

  // Releases input capture.
  virtual void ReleaseInputCapture() = 0;

  // Temporarily suspends input capture (e.g. during native window move loops).
  virtual void SuspendInputCapture() {}

  // Resumes input capture after suspension.
  virtual void ResumeInputCapture() {}

  // Updates the active window context being monitored during dragging.
  virtual void SetActiveWindowContext(TabDragWindowAdapter* new_window) {}
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
