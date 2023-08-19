// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
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

constexpr int kTextPixelPerDmm = 1100;
constexpr char kDefaultFontFamily[] = "sans-serif";

int DmmToPixel(float dmm) {
  return static_cast<int>(dmm * kTextPixelPerDmm);
}

float PixelToDmm(int pixel) {
  return static_cast<float>(pixel) / kTextPixelPerDmm;
}

void UpdateRenderText(gfx::RenderText* render_text,
                      const std::u16string& text,
                      const gfx::FontList& font_list,
                      SkColor color) {
  // Disable the cursor to avoid reserving width for a trailing caret.
  render_text->SetCursorEnabled(false);

  // Subpixel rendering is counterproductive when drawing VR textures.
  render_text->set_subpixel_rendering_suppressed(true);

  render_text->SetText(text);
  render_text->SetFontList(font_list);
  render_text->SetColor(color);
  render_text->set_shadows({});

  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  const int font_style = font_list.GetFontStyle();
  render_text->SetStyle(gfx::TEXT_STYLE_ITALIC,
                        (font_style & gfx::Font::ITALIC) != 0);
  render_text->SetStyle(gfx::TEXT_STYLE_UNDERLINE,
                        (font_style & gfx::Font::UNDERLINE) != 0);
  render_text->SetWeight(font_list.GetFontWeight());
}

}  // namespace

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

  void SetTextWidth(float width) { SetAndDirty(&text_width_, width); }

  // This method does all text preparation for the element other than drawing to
  // the texture. This allows for deeper unit testing of the Text element
  // without having to mock canvases and simulate frame rendering. The state of
  // the texture is modified here.
  gfx::Size LayOutText();

  const std::vector<std::unique_ptr<gfx::RenderText>>& lines() const {
    return lines_;
  }

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  void PrepareDrawText(const std::u16string& text,
                       const gfx::FontList& font_list,
                       gfx::Rect* bounds,
                       SkColor color);

  gfx::SizeF size_;
  std::u16string text_;
  float font_height_dmms_ = 0;
  float text_width_ = 0;
  SkColor color_ = SK_ColorBLACK;
  std::vector<std::unique_ptr<gfx::RenderText>> lines_;
  raw_ptr<Text> element_ = nullptr;
};

Text::Text(float font_height_dmms)
    : TexturedElement(), texture_(std::make_unique<TextTexture>(this)) {
  texture_->SetFontHeightInDmm(font_height_dmms);
}

Text::~Text() {}

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

const std::vector<std::unique_ptr<gfx::RenderText>>& Text::LinesForTest() {
  return texture_->lines();
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
  TexturedElement::SetSize(field_width_,
                           PixelToDmm(text_texture_size_.height()));

  return text_texture_size_;
}

gfx::Size TextTexture::LayOutText() {
  int pixel_font_height = DmmToPixel(font_height_dmms_);
  gfx::Rect text_bounds;
  DCHECK(text_width_ > 0.f) << element_->DebugName();
  text_bounds.set_width(DmmToPixel(text_width_));

  gfx::FontList fonts =
      gfx::FontList(gfx::Font(kDefaultFontFamily, pixel_font_height));

  PrepareDrawText(text_, fonts, &text_bounds, color_);

  set_measured();

  return text_bounds.size();
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  for (auto& render_text : lines_)
    render_text->Draw(canvas);
}

void TextTexture::PrepareDrawText(const std::u16string& text,
                                  const gfx::FontList& font_list,
                                  gfx::Rect* bounds,
                                  SkColor color) {
  DCHECK(bounds);
  lines_.clear();

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
    UpdateRenderText(render_text.get(), strings[i], font_list, color);

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

}  // namespace vr
