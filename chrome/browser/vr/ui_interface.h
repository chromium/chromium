// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_UI_INTERFACE_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/vr/fov_rectangle.h"

namespace gfx {
class Point3F;
class PointF;
class Transform;
}

namespace vr {

class BrowserUiInterface;
class SchedulerUiInterface;
struct RenderInfo;
enum class UserFriendlyElementName;

// This interface represents the methods that should be called by its owner, and
// also serves to make all such methods virtual for the sake of separating a UI
// feature module.
class UiInterface {
 public:
  virtual ~UiInterface() = default;

  virtual base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr() = 0;
  virtual SchedulerUiInterface* GetSchedulerUiPtr() = 0;

  virtual void OnGlInitialized() = 0;

  virtual gfx::Point3F GetTargetPointForTesting(
      UserFriendlyElementName element_name,
      const gfx::PointF& position) = 0;
  virtual bool GetElementVisibility(UserFriendlyElementName element_name) = 0;
  virtual bool OnBeginFrame(base::TimeTicks current_time,
                            const gfx::Transform& head_pose) = 0;
  virtual bool SceneHasDirtyTextures() const = 0;
  virtual void UpdateSceneTextures() = 0;
  virtual void Draw(const RenderInfo& render_info) = 0;
  virtual void DrawWebVrOverlayForeground(const RenderInfo&) = 0;
  virtual bool HasWebXrOverlayElementsToDraw() = 0;

  // This function calculates the minimal FOV (in degrees) which covers all
  // visible overflow elements as if it was viewing from fov_recommended. For
  // example, if fov_recommended is {20.f, 20.f, 20.f, 20.f}. And all elements
  // appear on screen within a FOV of {-11.f, 19.f, 9.f, 9.f} if we use
  // fov_recommended. Ideally, the calculated minimal FOV should be the same. In
  // practice, the elements might get clipped near the edge sometimes due to
  // float precision. To fix this, we add a small margin (1 degree) to all
  // directions. So the |out_fov| set by this function should be {-10.f, 20.f,
  // 10.f, 10.f} in the example case.
  // Using a smaller FOV could improve the performance a lot while we are
  // showing UIs on top of WebVR content.
  virtual FovRectangles GetMinimalFovForWebXrOverlayElements(
      const gfx::Transform& left_view,
      const FovRectangle& fov_recommended_left,
      const gfx::Transform& right_view,
      const FovRectangle& fov_recommended_right,
      float z_near) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INTERFACE_H_
