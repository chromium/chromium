// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_TYPES_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_TYPES_H_

#include <string>
#include <vector>

#include "components/content_creation/notes/core/templates/template_storage.pb.h"

namespace content_creation {

using ARGBColor = uint32_t;

// Represents all currently known templates. Keep this enum in sync with its
// Java counterpart of the same name (in NoteCreationMetrics.java).
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

// Represents the different supported linear gradient directions. Keep this enum
// in sync with its Java counterpart of the same name.
enum class LinearGradientDirection {
  kInvalid = 0,
  kTopToBottom = 1,
  kTopRightToBottomLeft = 2,
  kRightToLeft = 3,
  kBottomRightToTopLeft = 4,
};

// Represents a colored background.
class Background {
 public:
  // Creates a solid colored background.
  explicit Background(ARGBColor color);

  // Creates a linear gradient background.
  explicit Background(const std::vector<ARGBColor>& colors,
                      LinearGradientDirection direction);

  // Creates an image background based on a remotely hosted image's URL.
  explicit Background(const std::string& image_url);

  // Creates a Background from a protobuf Background object while still
  // ensuring the DChecks().
  static Background Init(const proto::Background& background);

  Background(const Background& other);

  ~Background();

  ARGBColor color() const { return color_; }

  const std::vector<ARGBColor>& colors() const { return colors_; }
  LinearGradientDirection direction() const { return direction_; }

  const std::string& image_url() const { return image_url_; }

  bool is_linear_gradient() const { return is_linear_gradient_; }
  bool is_image() const { return is_image_; }

 private:
  ARGBColor color_;

  std::vector<ARGBColor> colors_;
  LinearGradientDirection direction_;

  std::string image_url_;

  bool is_linear_gradient_;
  bool is_image_;
};

// Represents the different supported text alignment. Keep this enum in sync with its Java
// counterpart of the same name.
enum class TextAlignment { kInvalid = 0, kStart = 1, kCenter = 2, kEnd = 3 };

// Represents the different supported text highlight styles. Keep this enum in
// sync with its Java counterpart of the same name.
enum class HighlightStyle { kNone = 0, kFull = 1, kHalf = 2 };

// Parameters dictating how to display text.
class TextStyle {
 public:
  explicit TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     uint16_t weight,
                     bool all_caps,
                     TextAlignment alignment,
                     int minTextSize,
                     int maxTextSize);

  explicit TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     uint16_t weight,
                     bool all_caps,
                     TextAlignment alignment,
                     int minTextSize,
                     int maxTextSize,
                     ARGBColor highlight_color,
                     HighlightStyle highlight_style);

  // Creates a TextStyle from a protobuf TextStyle object.
  explicit TextStyle(const proto::TextStyle& textstyle);

  TextStyle(const TextStyle& text_style);
  TextStyle& operator=(const TextStyle& text_style);

  const std::string& font_name() const { return font_name_; }
  ARGBColor font_color() const { return font_color_; }
  uint16_t weight() const { return weight_; }
  bool all_caps() const { return all_caps_; }
  TextAlignment alignment() const { return alignment_; }
  int min_text_size_sp() const { return min_text_size_sp_; }
  int max_text_size_sp() const { return max_text_size_sp_; }
  ARGBColor highlight_color() const { return highlight_color_; }
  HighlightStyle highlight_style() const { return highlight_style_; }

 private:
  std::string font_name_;
  ARGBColor font_color_;
  uint16_t weight_;
  bool all_caps_;
  TextAlignment alignment_;
  int min_text_size_sp_;
  int max_text_size_sp_;
  ARGBColor highlight_color_;
  HighlightStyle highlight_style_;
};

// Parameters to control the appearance of the elements in a note's footer.
class FooterStyle {
 public:
  explicit FooterStyle(ARGBColor text_color, ARGBColor logo_color);

  // Creates a FooterStyle from a protobuf FooterStyle object.
  explicit FooterStyle(const proto::FooterStyle& footerstyle);

  ARGBColor text_color() const { return text_color_; }
  ARGBColor logo_color() const { return logo_color_; }

 private:
  ARGBColor text_color_;
  ARGBColor logo_color_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_TYPES_H_
