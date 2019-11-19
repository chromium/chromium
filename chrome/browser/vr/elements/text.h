// Copyright 2017 The Chromium Authors. All rights reserved.
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

class RenderTextWrapper;
class TextTexture;

enum TextLayoutMode {
  kMultiLineFixedWidth,
  kSingleLine,
  kSingleLineFixedWidth,
};

// This class describes a formatting attribute, applicable to a Text element.
// Attributes are applied in order, and may override previous attributes.
// Formatting may be applied only to non-wrapping text.
class VR_UI_EXPORT TextFormattingAttribute {
 public:
  enum Type {
    COLOR,
    WEIGHT,
    DIRECTIONALITY,
  };

  TextFormattingAttribute(SkColor color, const gfx::Range& range);
  TextFormattingAttribute(gfx::Font::Weight weight, const gfx::Range& range);
  explicit TextFormattingAttribute(gfx::DirectionalityMode directionality);

  void Apply(RenderTextWrapper* render_text) const;

  bool operator==(const TextFormattingAttribute& other) const;
  bool operator!=(const TextFormattingAttribute& other) const;

  Type type() { return type_; }
  gfx::Range range() { return range_; }
  SkColor color() { return color_; }
  gfx::Font::Weight weight() { return weight_; }
  gfx::DirectionalityMode directionality() { return directionality_; }

 private:
  Type type_;
  gfx::Range range_;
  union {
    SkColor color_;
    gfx::Font::Weight weight_;
    gfx::DirectionalityMode directionality_;
  };
};

typedef std::vector<TextFormattingAttribute> TextFormatting;

enum TextAlignment {
  kTextAlignmentNone,
  kTextAlignmentLeft,
  kTextAlignmentCenter,
  kTextAlignmentRight,
};

enum WrappingBehavior {
  kWrappingBehaviorWrap,
  kWrappingBehaviorNoWrap,
};

struct TextRenderParameters {
  SkColor color = SK_ColorBLACK;
  TextAlignment text_alignment = kTextAlignmentNone;
  WrappingBehavior wrapping_behavior = kWrappingBehaviorNoWrap;
  bool cursor_enabled = false;
  int cursor_position = 0;
  bool shadows_enabled = false;
  SkColor shadow_color = SK_ColorBLACK;
  float shadow_size = 10.0f;
};

class VR_UI_EXPORT Text : public TexturedElement {
 public:
  explicit Text(float font_height_dmms);
  ~Text() override;

  void SetFontHeightInDmm(float font_height_dmms);
  void SetText(const base::string16& text);

  // SetSize() should not be called on the Text element, because the element
  // updates its size according to text layout.
  void SetFieldWidth(float width);

  virtual void SetColor(SkColor color);
  void SetSelectionColors(const TextSelectionColors& colors);

  // Formatting must be applied only to non-wrapping text elements.
  void SetFormatting(const TextFormatting& formatting);

  void SetAlignment(TextAlignment alignment);
  void SetLayoutMode(TextLayoutMode mode);

  // This text element does not typically feature a cursor, but since the cursor
  // position is determined while laying out text, a parent may enable the
  // cursor and query the location at which it was last draw.
  void SetCursorEnabled(bool enabled);

  // Sets the current selection on the text field.  The selection is drawn only
  // if the cursor is enabled.
  void SetSelectionIndices(int start, int end);

  // Returns the most recently computed cursor position, in pixels.  This is
  // used for scene dirtiness and testing.
  gfx::Rect GetRawCursorBounds() const;

  // Returns the most recently computed cursor position, in fractions of the
  // texture size, relative to the upper-left corner of the element.
  gfx::RectF GetCursorBounds() const;

  int GetCursorPositionFromPoint(const gfx::PointF& point) const;

  // This causes the text to become uniformly shadowed.
  void SetShadowsEnabled(bool enabled);

  const std::vector<std::unique_ptr<gfx::RenderText>>& LinesForTest();
  void SetUnsupportedCodePointsForTest(bool unsupported);

 protected:
  void SetOnRenderTextCreated(
      base::RepeatingCallback<void(gfx::RenderText*)> callback);
  void SetOnRenderTextRendered(
      base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas* canvas)>
          callback);
  float MetersToPixels(float meters);

 private:
  UiTexture* GetTexture() const override;
  bool TextureDependsOnMeasurement() const override;
  gfx::Size MeasureTextureSize() override;

  TextLayoutMode text_layout_mode_ = kMultiLineFixedWidth;
  std::unique_ptr<TextTexture> texture_;
  gfx::Size text_texture_size_;
  float field_width_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(Text);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
