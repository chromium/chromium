// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_types.h"

#include "base/check.h"

namespace content_creation {

Background::Background(ARGBColor color)
    : color_(color),
      colors_(),
      direction_(LinearGradientDirection::kInvalid),
      image_url_(""),
      is_linear_gradient_(false),
      is_image_(false) {}

Background::Background(const std::vector<ARGBColor>& colors,
                       LinearGradientDirection direction)
    : color_(0U),
      colors_(colors),
      direction_(direction),
      image_url_(""),
      is_linear_gradient_(true),
      is_image_(false) {
  // Can't have a linear gradient with only one (or no) color.
  DCHECK(colors_.size() > 1);
}

Background::Background(const std::string& image_url)
    : color_(0U),
      colors_(),
      direction_(LinearGradientDirection::kInvalid),
      image_url_(image_url),
      is_linear_gradient_(false),
      is_image_(true) {
  DCHECK(image_url.size() > 0);
}

Background Background::Init(const proto::Background& background) {
  if (background.color() != 0) {
    return Background(static_cast<ARGBColor>(background.color()));
  }

  if (!background.url().empty()) {
    return Background(background.url());
  }

  return Background(
      std::vector<ARGBColor>(background.gradient().colors().begin(),
                             background.gradient().colors().end()),
      static_cast<LinearGradientDirection>(
          background.gradient().orientation()));
}

Background::Background(const Background& other) {
  color_ = other.color();
  colors_ = other.colors();
  direction_ = other.direction();
  image_url_ = other.image_url();
  is_linear_gradient_ = other.is_linear_gradient();
  is_image_ = other.is_image();
}

Background::~Background() = default;

TextStyle::TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     uint16_t weight,
                     bool all_caps,
                     TextAlignment alignment,
                     int min_text_size_sp,
                     int max_text_size_sp)
    : font_name_(font_name),
      font_color_(font_color),
      weight_(weight),
      all_caps_(all_caps),
      alignment_(alignment),
      min_text_size_sp_(min_text_size_sp),
      max_text_size_sp_(max_text_size_sp),
      highlight_color_(0U),
      highlight_style_(HighlightStyle::kNone) {}

TextStyle::TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     uint16_t weight,
                     bool all_caps,
                     TextAlignment alignment,
                     int min_text_size_sp,
                     int max_text_size_sp,
                     ARGBColor highlight_color,
                     HighlightStyle highlight_style)
    : font_name_(font_name),
      font_color_(font_color),
      weight_(weight),
      all_caps_(all_caps),
      alignment_(alignment),
      min_text_size_sp_(min_text_size_sp),
      max_text_size_sp_(max_text_size_sp),
      highlight_color_(highlight_color),
      highlight_style_(highlight_style) {}

TextStyle::TextStyle(const proto::TextStyle& textstyle)
    : TextStyle(textstyle.name(),
                textstyle.color(),
                textstyle.weight(),
                textstyle.allcaps(),
                static_cast<TextAlignment>(textstyle.alignment()),
                textstyle.mintextsize(),
                textstyle.maxtextsize()) {
  if (textstyle.highlightcolor() != 0) {
    highlight_color_ = textstyle.highlightcolor();
    highlight_style_ = static_cast<HighlightStyle>(textstyle.highlightstyle());
  }
}

TextStyle::TextStyle(const TextStyle& text_style) = default;

TextStyle& TextStyle::operator=(const TextStyle& text_style) = default;

FooterStyle::FooterStyle(ARGBColor text_color, ARGBColor logo_color)
    : text_color_(text_color), logo_color_(logo_color) {}

FooterStyle::FooterStyle(const proto::FooterStyle& footerstyle)
    : FooterStyle(footerstyle.textcolor(), footerstyle.logocolor()) {}

}  // namespace content_creation
