// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_H_
#define CHROME_BROWSER_VR_UI_SCENE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/sequence.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
class Transform;
}  // namespace gfx

namespace vr {

class UiElement;

class VR_UI_EXPORT UiScene {
 public:
  typedef base::RepeatingCallback<void()> PerFrameCallback;

  UiScene();
  ~UiScene();

  void AddUiElement(UiElementName parent, std::unique_ptr<UiElement> element);
  void AddParentUiElement(UiElementName child,
                          std::unique_ptr<UiElement> element);

  std::unique_ptr<UiElement> RemoveUiElement(int element_id);

  // Handles per-frame updates, giving each element the opportunity to update,
  // if necessary (eg, for animations). NB: |current_time| is the shared,
  // absolute begin frame time.
  // Returns true if *anything* was updated.
  bool OnBeginFrame(const base::TimeTicks& current_time,
                    const gfx::Transform& head_pose);

  // Returns true if any visible textures need to be redrawn.
  bool HasDirtyTextures() const;

  void UpdateTextures();

  UiElement& root_element();

  UiElement* GetUiElementById(int element_id) const;
  UiElement* GetUiElementByName(UiElementName name) const;

  typedef std::vector<const UiElement*> Elements;
  typedef std::vector<UiElement*> MutableElements;

  std::vector<UiElement*>& GetAllElements();
  Elements GetElementsToHitTest();
  Elements GetElementsToDraw();
  bool HasWebXrOverlayElementsToDraw();
  Elements GetWebVrOverlayElementsToDraw();

  float background_distance() const { return background_distance_; }
  void set_background_distance(float d) { background_distance_ = d; }

  void set_dirty() { is_dirty_ = true; }

  void OnGlInitialized(SkiaSurfaceProvider* provider);

  // The callback to call on every new frame. This is used for things we want to
  // do every frame regardless of element or subtree visibility.
  void AddPerFrameCallback(PerFrameCallback callback);

  void AddSequence(std::unique_ptr<Sequence> sequence);

  SkiaSurfaceProvider* SurfaceProviderForTesting() { return provider_; }

  void RunFirstFrameForTest();

 private:
  void InitializeElement(UiElement* element);

  MutableElements GetVisibleElementsMutable();

  std::unique_ptr<UiElement> root_element_;

  float background_distance_ = 10.0f;
  bool gl_initialized_ = false;
  bool initialized_scene_ = false;

  // TODO(mthiesse): Convert everything that manipulates UI elements to bindings
  // or layout updates. Don't allow any code to go in and manipulate UI elements
  // outside of these phases so that we can more easily compute dirtiness.
  bool is_dirty_ = false;

  // This is used to advance animations to completion on the first frame.
  bool first_frame_ = true;

  std::vector<UiElement*> all_elements_;

  std::vector<PerFrameCallback> per_frame_callback_;

  std::vector<std::unique_ptr<Sequence>> scheduled_tasks_;
  SkiaSurfaceProvider* provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UiScene);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_H_
