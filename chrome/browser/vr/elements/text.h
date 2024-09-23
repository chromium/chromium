// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_H_

#include <memory>

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/font.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"

namespace gfx {
class RenderText;
}

namespace vr {

class TextTexture;

// Used to render text in a VR Overlay. All text will be left aligned,
// multiline, and fixed width.
class VR_UI_EXPORT Text : public TexturedElement {
 public:
  explicit Text(float font_height_dmms);

  Text(const Text&) = delete;
  Text& operator=(const Text&) = delete;

  ~Text() override;

  // TODO(crbug.com/41430992): Make this part of the constructor.
  void SetText(const std::u16string& text);

  // SetSize() should not be called on the Text element, because the element
  // updates its size according to text layout.
  // TODO(crbug.com/41430992): Make this part of the constructor.
  void SetFieldWidth(float width);

  // TODO(crbug.com/41430992): Make this part of the constructor.
  virtual void SetColor(SkColor color);

  const std::vector<std::unique_ptr<gfx::RenderText>>& LinesForTest();

 private:
  UiTexture* GetTexture() const override;
  bool TextureDependsOnMeasurement() const override;
  gfx::Size MeasureTextureSize() override;

  std::unique_ptr<TextTexture> texture_;
  gfx::Size text_texture_size_;
  float field_width_ = 0.f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
