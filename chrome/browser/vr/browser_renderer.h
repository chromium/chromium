// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BROWSER_RENDERER_H_
#define CHROME_BROWSER_VR_BROWSER_RENDERER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/util/sliding_average.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace vr {

class UiInterface;
struct RenderInfo;
struct UiVisibilityState;

// The BrowserRenderer handles all input/output activities during a frame.
// This includes head movement, controller movement and input, audio output and
// rendering of the frame.
class VR_EXPORT BrowserRenderer {
 public:
  BrowserRenderer(std::unique_ptr<UiInterface> ui,
                  std::unique_ptr<GraphicsDelegate> graphics_delegate,
                  size_t sliding_time_size);

  BrowserRenderer(const BrowserRenderer&) = delete;
  BrowserRenderer& operator=(const BrowserRenderer&) = delete;

  ~BrowserRenderer();

  void DrawBrowserFrame(base::TimeTicks current_time,
                        const gfx::Transform& head_pose);
  void DrawWebXrFrame(base::TimeTicks current_time,
                      const gfx::Transform& head_pose);

  // Allows passing std::nullopt in case the test is shutdown before the
  // visibility notification has fired.
  void WatchElementForVisibilityStatusForTesting(
      std::optional<UiVisibilityState> visibility_expectation);

 private:

  void Draw(FrameType frame_type,
            base::TimeTicks current_time,
            const gfx::Transform& head_pose);

  // Position, hide and/or show UI elements, process input and update textures.
  // Returns true if the scene changed.
  void UpdateUi(const RenderInfo& render_info,
                base::TimeTicks currrent_time,
                FrameType frame_type);
  void DrawWebXrOverlay(const RenderInfo& render_info);
  void DrawBrowserUi(const RenderInfo& render_info);
  void ReportElementVisibilityStatus(const base::TimeTicks& current_time);
  void ReportElementVisibilityResult(bool result);

  std::unique_ptr<GraphicsDelegate> graphics_delegate_;

  std::optional<UiVisibilityState> ui_visibility_state_;
  device::SlidingTimeDeltaAverage ui_processing_time_;

  // ui_ is using gl contexts during destruction (skia context specifically), so
  // it must be destroyed before graphics_delegate_.
  std::unique_ptr<UiInterface> ui_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BROWSER_RENDERER_H_
