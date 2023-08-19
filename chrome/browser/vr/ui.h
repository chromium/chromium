// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_H_
#define CHROME_BROWSER_VR_UI_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/scheduler_ui_interface.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_initial_state.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

class SkiaSurfaceProvider;
class UiBrowserInterface;
class UiRenderer;
struct Model;

// This class manages all GLThread owned objects and GL rendering for VrShell.
// It is not threadsafe and must only be used on the GL thread.
class VR_UI_EXPORT Ui : public UiInterface,
                        public BrowserUiInterface,
                        public SchedulerUiInterface {
 public:
  Ui(UiBrowserInterface* browser, const UiInitialState& ui_initial_state);

  Ui(const Ui&) = delete;
  Ui& operator=(const Ui&) = delete;

  ~Ui() override;

  void OnUiRequestedNavigation();

  void ReinitializeForTest(const UiInitialState& ui_initial_state);

  void Dump(bool include_bindings);
  // TODO(crbug.com/767957): Refactor to hide these behind the UI interface.
  UiScene* scene() { return scene_.get(); }
  UiElementRenderer* ui_element_renderer() {
    return ui_element_renderer_.get();
  }
  Model* model_for_test() { return model_.get(); }

 private:
  // BrowserUiInterface
  void SetCapturingState(
      const CapturingStateModel& active_capturing,
      const CapturingStateModel& background_capturing,
      const CapturingStateModel& potential_capturing) override;
  void SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType prompt) override;

  // UiInterface
  base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr() override;
  SchedulerUiInterface* GetSchedulerUiPtr() override;
  void OnGlInitialized() override;
  void OnPause() override;
  gfx::Point3F GetTargetPointForTesting(UserFriendlyElementName element_name,
                                        const gfx::PointF& position) override;
  bool GetElementVisibilityForTesting(
      UserFriendlyElementName element_name) override;
  bool OnBeginFrame(base::TimeTicks current_time,
                    const gfx::Transform& head_pose) override;
  bool SceneHasDirtyTextures() const override;
  void UpdateSceneTextures() override;
  void Draw(const RenderInfo& render_info) override;
  void DrawWebXr(int texture_data_handle,
                 const float (&uv_transform)[16]) override;
  void DrawWebVrOverlayForeground(const RenderInfo& render_info) override;
  bool HasWebXrOverlayElementsToDraw() override;
  void HandleMenuButtonEvents(InputEventList* input_event_list) override;
  FovRectangles GetMinimalFovForWebXrOverlayElements(
      const gfx::Transform& left_view,
      const FovRectangle& fov_recommended_left,
      const gfx::Transform& right_view,
      const FovRectangle& fov_recommended_right,
      float z_near) override;

  // SchedulerUiInterface
  void OnWebXrFrameAvailable() override;
  void OnWebXrTimedOut() override;
  void OnWebXrTimeoutImminent() override;

 private:
  void OnMenuButtonClicked();
  void InitializeModel(const UiInitialState& ui_initial_state);
  raw_ptr<UiBrowserInterface, DanglingUntriaged> browser_;
  FovRectangle GetMinimalFov(const gfx::Transform& view_matrix,
                             const std::vector<const UiElement*>& elements,
                             const FovRectangle& fov_recommended,
                             float z_near);

  // This state may be further abstracted into a SkiaUi object.
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<UiElementRenderer> ui_element_renderer_;
  std::unique_ptr<UiRenderer> ui_renderer_;
  std::unique_ptr<SkiaSurfaceProvider> provider_;

  base::WeakPtrFactory<Ui> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_H_
