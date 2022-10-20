// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXTURED_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXTURED_ELEMENT_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "device/vr/gl_bindings.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"

class SkSurface;

namespace vr {

class UiTexture;

class VR_UI_EXPORT TexturedElement : public UiElement {
 public:
  TexturedElement();

  TexturedElement(const TexturedElement&) = delete;
  TexturedElement& operator=(const TexturedElement&) = delete;

  ~TexturedElement() override;

  void Initialize(SkiaSurfaceProvider* provider) final;

  bool HasDirtyTexture() const override;
  void UpdateTexture() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  // Testing accessors.
  void PrepareToDrawForTest() { PrepareToDraw(); }
  gfx::Size texture_size_for_test() { return texture_size_; }

  // Foreground and background colors are used pervasively in textured elements,
  // but more element-specific colors should be set on the appropriate element.
  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);

 protected:
  virtual UiTexture* GetTexture() const = 0;

  bool PrepareToDraw() final;

 private:
  // Subclasses must return true if redrawing a texture depends on measurement
  // (text, for example).  If true, a texture dirtied by user input (after
  // measurement) will not be redrawn until the following frame.
  virtual bool TextureDependsOnMeasurement() const = 0;

  virtual gfx::Size MeasureTextureSize() = 0;

  gfx::Size texture_size_;
  GLuint texture_handle_ = 0;
  bool initialized_ = false;

  sk_sp<SkSurface> surface_;
  raw_ptr<SkiaSurfaceProvider, DanglingUntriaged> provider_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXTURED_ELEMENT_H_
