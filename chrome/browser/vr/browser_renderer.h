// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BROWSER_RENDERER_H_
#define CHROME_BROWSER_VR_BROWSER_RENDERER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/scheduler_browser_renderer_interface.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/util/sliding_average.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace vr {

enum class UiTestOperationResult;
class BrowserUiInterface;
class InputDelegate;
class PlatformUiInputDelegate;
class BrowserRendererBrowserInterface;
class SchedulerDelegate;
class UiInterface;
struct RenderInfo;
struct UiTestActivityExpectation;
struct VisibilityChangeExpectation;
struct UiTestState;
struct UiVisibilityState;

// The BrowserRenderer handles all input/output activities during a frame.
// This includes head movement, controller movement and input, audio output and
// rendering of the frame.
class VR_EXPORT BrowserRenderer : public SchedulerBrowserRendererInterface {
 public:
  BrowserRenderer(std::unique_ptr<UiInterface> ui,
                  std::unique_ptr<SchedulerDelegate> scheduler_delegate,
                  std::unique_ptr<GraphicsDelegate> graphics_delegate,
                  std::unique_ptr<InputDelegate> input_delegate,
                  BrowserRendererBrowserInterface* browser,
                  size_t sliding_time_size);

  BrowserRenderer(const BrowserRenderer&) = delete;
  BrowserRenderer& operator=(const BrowserRenderer&) = delete;

  ~BrowserRenderer() override;

  void OnPause();
  void OnResume();

  void OnExitPresent();
  void OnTriggerEvent(bool pressed);

  base::WeakPtr<BrowserRenderer> GetWeakPtr();
  base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr();

  void SetUiExpectingActivityForTesting(
      UiTestActivityExpectation ui_expectation);
  void WatchElementForVisibilityStatusForTesting(
      VisibilityChangeExpectation visibility_expectation);
  void SetBrowserRendererBrowserInterfaceForTesting(
      BrowserRendererBrowserInterface* interface_ptr);
  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options);

 private:
  // SchedulerBrowserRendererInterface implementation.
  void DrawBrowserFrame(base::TimeTicks current_time) override;
  void DrawWebXrFrame(base::TimeTicks current_time,
                      const gfx::Transform& head_pose) override;
  void ProcessControllerInputForWebXr(const gfx::Transform& head_pose,
                                      base::TimeTicks current_time) override;

  void Draw(FrameType frame_type,
            base::TimeTicks current_time,
            const gfx::Transform& head_pose);

  // Position, hide and/or show UI elements, process input and update textures.
  // Returns true if the scene changed.
  void UpdateUi(const RenderInfo& render_info,
                base::TimeTicks currrent_time,
                FrameType frame_type);
  void DrawWebXr();
  void DrawWebXrOverlay(const RenderInfo& render_info);
  void DrawBrowserUi(const RenderInfo& render_info);
  void ReportElementVisibilityStatusForTesting(
      const base::TimeTicks& current_time);
  void ReportElementVisibilityResultForTesting(UiTestOperationResult result);

  std::unique_ptr<SchedulerDelegate> scheduler_delegate_;
  std::unique_ptr<GraphicsDelegate> graphics_delegate_;
  std::unique_ptr<InputDelegate> input_delegate_;
  std::unique_ptr<InputDelegate> input_delegate_for_testing_;

  std::unique_ptr<PlatformUiInputDelegate> vr_dialog_input_delegate_;

  raw_ptr<BrowserRendererBrowserInterface, DanglingUntriaged> browser_;

  std::unique_ptr<UiTestState> ui_test_state_;
  std::unique_ptr<UiVisibilityState> ui_visibility_state_;
  device::SlidingTimeDeltaAverage ui_processing_time_;
  device::SlidingTimeDeltaAverage ui_controller_update_time_;

  // ui_ is using gl contexts during destruction (skia context specifically), so
  // it must be destroyed before graphics_delegate_.
  std::unique_ptr<UiInterface> ui_;

  base::WeakPtrFactory<BrowserRenderer> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BROWSER_RENDERER_H_
