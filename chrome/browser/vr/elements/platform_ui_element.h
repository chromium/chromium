// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_PLATFORM_UI_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_PLATFORM_UI_ELEMENT_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/gl_texture_location.h"

namespace vr {

class PlatformUiInputDelegate;

// This element hosts the texture of a platform UI and surfaces all frames of
// the UI to VR. For example, on Android, any views UI can be drawn to a texture
// and then displayed by this element in VR.
// It also dispatches events to the platform UI.
class PlatformUiElement : public UiElement {
 public:
  PlatformUiElement();
  ~PlatformUiElement() override;

  void OnHoverEnter(const gfx::PointF& position,
                    base::TimeTicks timestamp) override;
  void OnHoverLeave(base::TimeTicks timestamp) override;
  void OnHoverMove(const gfx::PointF& position,
                   base::TimeTicks timestamp) override;
  void OnButtonDown(const gfx::PointF& position,
                    base::TimeTicks timestamp) override;
  void OnButtonUp(const gfx::PointF& position,
                  base::TimeTicks timestamp) override;
  void OnTouchMove(const gfx::PointF& position,
                   base::TimeTicks timestamp) override;
  void OnFlingCancel(std::unique_ptr<InputEvent> gesture,
                     const gfx::PointF& position) override;
  void OnScrollBegin(std::unique_ptr<InputEvent> gesture,
                     const gfx::PointF& position) override;
  void OnScrollUpdate(std::unique_ptr<InputEvent> gesture,
                      const gfx::PointF& position) override;
  void OnScrollEnd(std::unique_ptr<InputEvent> gesture,
                   const gfx::PointF& position) override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const override;

  void SetTextureId(unsigned int texture_id);
  void SetTextureLocation(GlTextureLocation location);

  void SetDelegate(PlatformUiInputDelegate* delegate);

 protected:
  PlatformUiInputDelegate* delegate() const { return delegate_; }
  unsigned int texture_id() const { return texture_id_; }
  GlTextureLocation texture_location() const { return texture_location_; }

 private:
  PlatformUiInputDelegate* delegate_ = nullptr;
  unsigned int texture_id_ = 0;
  GlTextureLocation texture_location_ = kGlTextureLocationExternal;

  DISALLOW_COPY_AND_ASSIGN(PlatformUiElement);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_PLATFORM_UI_ELEMENT_H_
