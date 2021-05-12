// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_TYPES_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_TYPES_H_

#include <string>
#include <vector>

namespace content_creation {

using ARGBColor = uint32_t;

// Represents all currently known templates.
enum class NoteTemplateIds {
  kUnknown = 0,
  kClassic = 1,
  kFriendly = 2,
  kFresh = 3,
  kPowerful = 4,
  kImpactful = 5,
  kLovely = 6,
  kGroovy = 7,
  kMonochrome = 8,
  kBold = 9,
  kDreamy = 10,

  kMaxValue = kDreamy
};

// Represents a colored background.
class Background {
 public:
  explicit Background(ARGBColor color);

  ARGBColor color() const { return color_; }

 private:
  ARGBColor color_;
};

// Parameters dictating how to display text.
class TextStyle {
 public:
  explicit TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     uint16_t weight,
                     bool all_caps);

  const std::string font_name() const { return font_name_; }
  ARGBColor font_color() const { return font_color_; }
  uint16_t weight() const { return weight_; }
  bool all_caps() const { return all_caps_; }

 private:
  std::string font_name_;
  ARGBColor font_color_;
  uint16_t weight_;
  bool all_caps_;
};

// Parameters to control the appearance of the elements in a note's footer.
class FooterStyle {
 public:
  explicit FooterStyle(ARGBColor text_color, ARGBColor logo_color);

  ARGBColor text_color() const { return text_color_; }
  ARGBColor logo_color() const { return logo_color_; }

 private:
  ARGBColor text_color_;
  ARGBColor logo_color_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_TYPES_H_
