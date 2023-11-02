// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/text_elider.h"

namespace vr {

namespace {

constexpr float kCursorWidthRatio = 0.07f;
constexpr int kTextPixelPerDmm = 1100;
constexpr float kTextShadowScaleFactor = 1000.0f;
constexpr char kDefaultFontFamily[] = "sans-serif";

int DmmToPixel(float dmm) {
  return static_cast<int>(dmm * kTextPixelPerDmm);
}

float PixelToDmm(int pixel) {
  return static_cast<float>(pixel) / kTextPixelPerDmm;
}

bool IsFixedWidthLayout(TextLayoutMode mode) {
  return mode == kSingleLineFixedWidth || mode == kMultiLineFixedWidth;
}

void UpdateRenderText(gfx::RenderText* render_text,
                      const std::u16string& text,
                      const gfx::FontList& font_list,
                      SkColor color,
                      TextAlignment text_alignment,
                      bool shadows_enabled,
                      SkColor shadow_color,
                      float shadow_size) {
  // Disable the cursor to avoid reserving width for a trailing caret.
  render_text->SetCursorEnabled(false);

  // Subpixel rendering is counterproductive when drawing VR textures.
  render_text->set_subpixel_rendering_suppressed(true);

  render_text->SetText(text);
  render_text->SetFontList(font_list);
  render_text->SetColor(color);
  if (shadows_enabled) {
    render_text->set_shadows(
        {gfx::ShadowValue({0, 0}, shadow_size, shadow_color)});
  } else {
    render_text->set_shadows({});
  }

  switch (text_alignment) {
    case kTextAlignmentNone:
      break;
    case kTextAlignmentLeft:
      render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      break;
    case kTextAlignmentRight:
      render_text->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
      break;
    case kTextAlignmentCenter:
      render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      break;
  }

  const int font_style = font_list.GetFontStyle();
  render_text->SetStyle(gfx::TEXT_STYLE_ITALIC,
                        (font_style & gfx::Font::ITALIC) != 0);
  render_text->SetStyle(gfx::TEXT_STYLE_UNDERLINE,
                        (font_style & gfx::Font::UNDERLINE) != 0);
  render_text->SetWeight(font_list.GetFontWeight());
}

}  // namespace

TextFormattingAttribute::TextFormattingAttribute(SkColor color,
                                                 const gfx::Range& range)
    : type_(COLOR), range_(range), color_(color) {}

TextFormattingAttribute::TextFormattingAttribute(gfx::Font::Weight weight,
                                                 const gfx::Range& range)
    : type_(WEIGHT), range_(range), weight_(weight) {}

TextFormattingAttribute::TextFormattingAttribute(
    gfx::DirectionalityMode directionality)
    : type_(DIRECTIONALITY), directionality_(directionality) {}

bool TextFormattingAttribute::operator==(
    const TextFormattingAttribute& other) const {
  if (type_ != other.type_ || range_ != other.range_)
    return false;
  switch (type_) {
    case COLOR:
      return color_ == other.color_;
    case WEIGHT:
      return weight_ == other.weight_;
    case DIRECTIONALITY:
      return directionality_ == other.directionality_;
    default:
      NOTREACHED();
      return false;
  }
}

bool TextFormattingAttribute::operator!=(
    const TextFormattingAttribute& other) const {
  return !(*this == other);
}

void TextFormattingAttribute::Apply(RenderTextWrapper* render_text) const {
  switch (type_) {
    case COLOR: {
      if (range_.IsValid()) {
        render_text->ApplyColor(color_, range_);
      } else {
        render_text->SetColor(color_);
      }
      break;
    }
    case WEIGHT:
      if (range_.IsValid()) {
        render_text->ApplyWeight(weight_, range_);
      } else {
        render_text->SetWeight(weight_);
      }
      break;
    case DIRECTIONALITY:
      render_text->SetDirectionalityMode(directionality_);
      break;
    default:
      NOTREACHED();
  }
}

class TextTexture : public UiTexture {
 public:
  explicit TextTexture(Text* element) : element_(element) {}

  TextTexture(const TextTexture&) = delete;
  TextTexture& operator=(const TextTexture&) = delete;

  ~TextTexture() override {}

  void SetFontHeightInDmm(float font_height_dmms) {
    SetAndDirty(&font_height_dmms_, font_height_dmms);
  }

  void SetText(const std::u16string& text) { SetAndDirty(&text_, text); }

  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

  void SetSelectionColors(const TextSelectionColors& colors) {
    SetAndDirty(&selection_colors_, colors);
  }

  void SetFormatting(const TextFormatting& formatting) {
    SetAndDirty(&formatting_, formatting);
  }

  void SetAlignment(TextAlignment alignment) {
    SetAndDirty(&alignment_, alignment);
  }

  void SetLayoutMode(TextLayoutMode mode) {
    SetAndDirty(&text_layout_mode_, mode);
  }

  void SetCursorEnabled(bool enabled) {
    SetAndDirty(&cursor_enabled_, enabled);
  }

  void SetSelectionIndices(int start, int end) {
    SetAndDirty(&selection_start_, start);
    SetAndDirty(&selection_end_, end);
  }

  void SetShadowsEnabled(bool enabled) {
    SetAndDirty(&shadows_enabled_, enabled);
  }

  void SetTextWidth(float width) { SetAndDirty(&text_width_, width); }

  gfx::Rect get_cursor_bounds() { return cursor_bounds_; }

  // This method does all text preparation for the element other than drawing to
  // the texture. This allows for deeper unit testing of the Text element
  // without having to mock canvases and simulate frame rendering. The state of
  // the texture is modified here.
  gfx::Size LayOutText();

  const std::vector<std::unique_ptr<gfx::RenderText>>& lines() const {
    return lines_;
  }

  void SetOnRenderTextCreated(
      base::RepeatingCallback<void(gfx::RenderText*)> callback) {
    render_text_created_callback_ = callback;
  }

  void SetOnRenderTextRendered(
      base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas* canvas)>
          callback) {
    render_text_rendered_callback_ = callback;
  }

  void set_unsupported_code_points_for_test(bool unsupported) {
    unsupported_code_point_for_test_ = unsupported;
  }

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  void PrepareDrawStringRect(const std::u16string& text,
                             const gfx::FontList& font_list,
                             gfx::Rect* bounds,
                             const TextRenderParameters& parameters);
  void PrepareDrawWrapText(const std::u16string& text,
                           const gfx::FontList& font_list,
                           gfx::Rect* bounds,
                           const TextRenderParameters& parameters);
  void PrepareDrawSingleLineText(const std::u16string& text,
                                 const gfx::FontList& font_list,
                                 gfx::Rect* bounds,
                                 const TextRenderParameters& parameters);

  gfx::SizeF size_;
  gfx::Vector2d texture_offset_;
  std::u16string text_;
  float font_height_dmms_ = 0;
  float text_width_ = 0;
  TextAlignment alignment_ = kTextAlignmentCenter;
  TextLayoutMode text_layout_mode_ = kMultiLineFixedWidth;
  SkColor color_ = SK_ColorBLACK;
  TextSelectionColors selection_colors_;
  TextFormatting formatting_;
  bool cursor_enabled_ = false;
  int selection_start_ = 0;
  int selection_end_ = 0;
  gfx::Rect cursor_bounds_;
  bool shadows_enabled_ = false;
  std::vector<std::unique_ptr<gfx::RenderText>> lines_;
  raw_ptr<Text> element_ = nullptr;

  base::RepeatingCallback<void(gfx::RenderText*)> render_text_created_callback_;
  base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas*)>
      render_text_rendered_callback_;

  bool unsupported_code_point_for_test_ = false;
};

Text::Text(float font_height_dmms)
    : TexturedElement(), texture_(std::make_unique<TextTexture>(this)) {
  texture_->SetFontHeightInDmm(font_height_dmms);
}

Text::~Text() {}

void Text::SetFontHeightInDmm(float font_height_dmms) {
  texture_->SetFontHeightInDmm(font_height_dmms);
}

void Text::SetText(const std::u16string& text) {
  texture_->SetText(text);
}

void Text::SetFieldWidth(float width) {
  field_width_ = width;
  texture_->SetTextWidth(width);
}

void Text::SetColor(SkColor color) {
  texture_->SetColor(color);
}

void Text::SetSelectionColors(const TextSelectionColors& colors) {
  texture_->SetSelectionColors(colors);
}

void Text::SetFormatting(const TextFormatting& formatting) {
  texture_->SetFormatting(formatting);
}

void Text::SetAlignment(TextAlignment alignment) {
  texture_->SetAlignment(alignment);
}

void Text::SetLayoutMode(TextLayoutMode mode) {
  text_layout_mode_ = mode;
  texture_->SetLayoutMode(mode);
}

void Text::SetCursorEnabled(bool enabled) {
  texture_->SetCursorEnabled(enabled);
}

void Text::SetSelectionIndices(int start, int end) {
  texture_->SetSelectionIndices(start, end);
}

gfx::Rect Text::GetRawCursorBounds() const {
  return texture_->get_cursor_bounds();
}

gfx::RectF Text::GetCursorBounds() const {
  // Note that gfx:: cursor bounds always indicate a one-pixel width, so we
  // override the width here to be a percentage of height for the sake of
  // arbitrary texture sizes.
  gfx::Rect bounds = texture_->get_cursor_bounds();
  float scale = size().width() / text_texture_size_.width();
  return gfx::RectF(
      bounds.CenterPoint().x() * scale, bounds.CenterPoint().y() * scale,
      bounds.height() * scale * kCursorWidthRatio, bounds.height() * scale);
}

int Text::GetCursorPositionFromPoint(const gfx::PointF& point) const {
  DCHECK_EQ(texture_->lines().size(), 1u);
  gfx::Point pixel_position(point.x() * text_texture_size_.width(),
                            point.y() * text_texture_size_.height());
  return texture_->lines()
      .front()
      ->FindCursorPosition(pixel_position)
      .caret_pos();
}

void Text::SetShadowsEnabled(bool enabled) {
  texture_->SetShadowsEnabled(enabled);
}

const std::vector<std::unique_ptr<gfx::RenderText>>& Text::LinesForTest() {
  return texture_->lines();
}

void Text::SetUnsupportedCodePointsForTest(bool unsupported) {
  texture_->set_unsupported_code_points_for_test(unsupported);
}

void Text::SetOnRenderTextCreated(
    base::RepeatingCallback<void(gfx::RenderText*)> callback) {
  texture_->SetOnRenderTextCreated(callback);
}

void Text::SetOnRenderTextRendered(
    base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas* canvas)>
        callback) {
  texture_->SetOnRenderTextRendered(callback);
}

float Text::MetersToPixels(float meters) {
  return DmmToPixel(meters);
}

UiTexture* Text::GetTexture() const {
  return texture_.get();
}

bool Text::TextureDependsOnMeasurement() const {
  return true;
}

gfx::Size Text::MeasureTextureSize() {
  text_texture_size_ = texture_->LayOutText();

  // Adjust the actual size of the element to match the texture.
  float width = IsFixedWidthLayout(text_layout_mode_)
                    ? field_width_
                    : PixelToDmm(text_texture_size_.width());
  TexturedElement::SetSize(width, PixelToDmm(text_texture_size_.height()));

  return text_texture_size_;
}

gfx::Size TextTexture::LayOutText() {
  int pixel_font_height = DmmToPixel(font_height_dmms_);
  gfx::Rect text_bounds;
  if (IsFixedWidthLayout(text_layout_mode_)) {
    DCHECK(text_width_ > 0.f) << element_->DebugName();
    text_bounds.set_width(DmmToPixel(text_width_));
  }

  gfx::FontList fonts =
      gfx::FontList(gfx::Font(kDefaultFontFamily, pixel_font_height));

  TextRenderParameters parameters;
  parameters.color = color_;
  parameters.text_alignment = alignment_;
  parameters.wrapping_behavior = text_layout_mode_ == kMultiLineFixedWidth
                                     ? kWrappingBehaviorWrap
                                     : kWrappingBehaviorNoWrap;
  parameters.cursor_enabled = cursor_enabled_;
  parameters.cursor_position = selection_end_;
  parameters.shadows_enabled = shadows_enabled_;
  parameters.shadow_size = kTextShadowScaleFactor * font_height_dmms_;

  PrepareDrawStringRect(text_, fonts, &text_bounds, parameters);

  if (cursor_enabled_) {
    DCHECK_EQ(lines_.size(), 1u);
    gfx::RenderText* render_text = lines_.front().get();

    if (selection_start_ != selection_end_) {
      render_text->set_focused(true);
      gfx::Range range(selection_start_, selection_end_);
      render_text->SetSelection(gfx::SelectionModel(
          range, gfx::LogicalCursorDirection::CURSOR_FORWARD));
      render_text->set_selection_background_focused_color(
          selection_colors_.background);
      render_text->set_selection_color(selection_colors_.foreground);
    }

    cursor_bounds_ = render_text->GetUpdatedCursorBounds();
  }

  if (!formatting_.empty()) {
    DCHECK_EQ(parameters.wrapping_behavior, kWrappingBehaviorNoWrap);
    DCHECK_EQ(lines_.size(), 1u);
    RenderTextWrapper render_text(lines_.front().get());
    for (const auto& attribute : formatting_) {
      attribute.Apply(&render_text);
    }
  }

  if (render_text_created_callback_) {
    DCHECK_EQ(lines_.size(), 1u);
    render_text_created_callback_.Run(lines_.front().get());
  }

  // Note, there is no padding here whatsoever.
  if (parameters.shadows_enabled) {
    const int offset = base::ClampFloor(parameters.shadow_size);
    texture_offset_ = gfx::Vector2d(offset, offset);
  }

  set_measured();

  return text_bounds.size();
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;
  canvas->Translate(texture_offset_);

  for (auto& render_text : lines_)
    render_text->Draw(canvas);

  if (render_text_rendered_callback_) {
    DCHECK_EQ(lines_.size(), 1u);
    render_text_rendered_callback_.Run(*lines_.front().get(), sk_canvas);
  }
}

void TextTexture::PrepareDrawStringRect(
    const std::u16string& text,
    const gfx::FontList& font_list,
    gfx::Rect* bounds,
    const TextRenderParameters& parameters) {
  DCHECK(bounds);

  if (parameters.wrapping_behavior == kWrappingBehaviorWrap)
    PrepareDrawWrapText(text, font_list, bounds, parameters);
  else
    PrepareDrawSingleLineText(text, font_list, bounds, parameters);

  if (parameters.shadows_enabled) {
    bounds->Inset(-parameters.shadow_size);
    bounds->Offset(parameters.shadow_size, parameters.shadow_size);
  }
}

void TextTexture::PrepareDrawWrapText(const std::u16string& text,
                                      const gfx::FontList& font_list,
                                      gfx::Rect* bounds,
                                      const TextRenderParameters& parameters) {
  lines_.clear();
  DCHECK(!parameters.cursor_enabled);

  gfx::Rect rect(*bounds);
  std::vector<std::u16string> strings;
  gfx::ElideRectangleText(text, font_list, bounds->width(),
                          bounds->height() ? bounds->height() : INT_MAX,
                          gfx::WRAP_LONG_WORDS, &strings);

  int height = 0;
  int line_height = 0;
  for (size_t i = 0; i < strings.size(); i++) {
    std::unique_ptr<gfx::RenderText> render_text =
        gfx::RenderText::CreateRenderText();
    UpdateRenderText(render_text.get(), strings[i], font_list, parameters.color,
                     parameters.text_alignment, parameters.shadows_enabled,
                     parameters.shadow_color, parameters.shadow_size);

    if (i == 0) {
      // Measure line and center text vertically.
      line_height = render_text->GetStringSize().height();
      rect.set_height(line_height);
      if (bounds->height()) {
        const int text_height = strings.size() * line_height;
        rect += gfx::Vector2d(0, (bounds->height() - text_height) / 2);
      }
    }

    render_text->SetDisplayRect(rect);
    height += line_height;
    rect += gfx::Vector2d(0, line_height);
    lines_.push_back(std::move(render_text));
  }

  // Set calculated height.
  if (bounds->height() == 0)
    bounds->set_height(height);
}

void TextTexture::PrepareDrawSingleLineText(
    const std::u16string& text,
    const gfx::FontList& font_list,
    gfx::Rect* bounds,
    const TextRenderParameters& parameters) {
  if (lines_.size() != 1) {
    lines_.clear();
    lines_.push_back(gfx::RenderText::CreateRenderText());
  }

  auto* render_text = lines_.front().get();
  UpdateRenderText(render_text, text, font_list, parameters.color,
                   parameters.text_alignment, parameters.shadows_enabled,
                   parameters.shadow_color, parameters.shadow_size);
  if (bounds->width() != 0 && !parameters.cursor_enabled)
    render_text->SetElideBehavior(gfx::TRUNCATE);
  if (parameters.cursor_enabled) {
    render_text->SetCursorEnabled(true);
    render_text->SetCursorPosition(parameters.cursor_position);
  }
  if (bounds->width() == 0)
    bounds->set_width(render_text->GetStringSize().width());
  if (bounds->height() == 0)
    bounds->set_height(render_text->GetStringSize().height());

  render_text->SetDisplayRect(*bounds);
}

}  // namespace vr
