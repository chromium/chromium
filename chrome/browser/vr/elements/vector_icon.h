// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_H_
#define CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_H_

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class PointF;
struct VectorIcon;
}  // namespace gfx

namespace vr {

class VectorIconTexture;

class VR_UI_EXPORT VectorIcon : public TexturedElement {
 public:
  explicit VectorIcon(int texture_width);

  VectorIcon(const VectorIcon&) = delete;
  VectorIcon& operator=(const VectorIcon&) = delete;

  ~VectorIcon() override;

  // TODO(vollick): should just use TexturedElement::SetForegroundColor.
  void SetColor(SkColor color);
  SkColor GetColor() const;
  void SetIcon(const gfx::VectorIcon& icon);
  void SetIcon(const gfx::VectorIcon* icon);

  static void DrawVectorIcon(gfx::Canvas* canvas,
                             const gfx::VectorIcon& icon,
                             float size_px,
                             const gfx::PointF& corner,
                             SkColor color);

 protected:
  UiTexture* GetTexture() const override;

 private:
  bool TextureDependsOnMeasurement() const override;
  gfx::Size MeasureTextureSize() override;

  std::unique_ptr<VectorIconTexture> texture_;
  int texture_width_ = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_H_
