// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_WARM_COMPOSITOR_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_WARM_COMPOSITOR_H_

#include <memory>

#include "base/task/task_runner.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace paint_preview {

// A class that can hold a pre-warmed compositor service for use.
class WarmCompositor {
 public:
  ~WarmCompositor();

  WarmCompositor(const WarmCompositor&) = delete;
  WarmCompositor& operator=(const WarmCompositor&) = delete;

  static WarmCompositor* GetInstance();

  // Warms up the compositor service.
  void WarmupCompositor();

  // Releases the warmed compositor service if there is one. Returns true if a
  // compositor was released.
  bool StopCompositor();

  // Passes the pre-warmed compositor to the caller if one is present.
  // Otherwise starts a new compositor.
  std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
  GetOrStartCompositorService(base::OnceClosure disconnect_handler);

 private:
  WarmCompositor();
  friend struct base::DefaultSingletonTraits<WarmCompositor>;

  void OnDisconnect();

  std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
      compositor_service_;

  base::WeakPtrFactory<WarmCompositor> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_WARM_COMPOSITOR_H_
