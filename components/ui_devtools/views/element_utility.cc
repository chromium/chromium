// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/element_utility.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ui_devtools {

namespace {

std::optional<gfx::RoundedCornersF> ParseRoundedCornersF(
    const std::string& value) {
  std::vector<std::string> parts = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 1) {
    double v;
    if (base::StringToDouble(parts[0], &v)) {
      return gfx::RoundedCornersF(static_cast<float>(v));
    }
  } else if (parts.size() == 4) {
    double ul, ur, lr, ll;
    if (base::StringToDouble(parts[0], &ul) &&
        base::StringToDouble(parts[1], &ur) &&
        base::StringToDouble(parts[2], &lr) &&
        base::StringToDouble(parts[3], &ll)) {
      return gfx::RoundedCornersF(
          static_cast<float>(ul), static_cast<float>(ur),
          static_cast<float>(lr), static_cast<float>(ll));
    }
  }
  return std::nullopt;
}

}  // namespace

void AppendLayerPropertiesMatchedStyle(
    const ui::Layer* layer,
    std::vector<UIElement::UIProperty>* ret) {
  ret->emplace_back("type", std::string(LayerTypeToString(layer->type())));
  ret->emplace_back("layer_mask_layer",
                    base::ToString(layer->layer_mask_layer()));
  ret->emplace_back("visible", base::ToString(layer->IsVisible()));
  ret->emplace_back("opacity", base::NumberToString(layer->opacity()));
  ret->emplace_back("combined_opacity",
                    base::NumberToString(layer->GetCombinedOpacity()));
  ret->emplace_back("background_blur",
                    base::NumberToString(layer->background_blur()));
  ret->emplace_back("layer_blur", base::NumberToString(layer->layer_blur()));
  ret->emplace_back("layer_saturation",
                    base::NumberToString(layer->layer_saturation()));
  ret->emplace_back("layer_brightness",
                    base::NumberToString(layer->layer_brightness()));
  ret->emplace_back("layer_grayscale",
                    base::NumberToString(layer->layer_grayscale()));
  ret->emplace_back("fills_bounds_opaquely",
                    base::ToString(layer->fills_bounds_opaquely()));
  if (auto* solid_color = layer->AsSolidColor(); solid_color) {
    std::u16string color_str = ui::metadata::SkColorConverter::ToString(
        solid_color->GetTargetColor().toSkColor());
    ret->emplace_back("color", base::UTF16ToUTF8(color_str));
  }

  const auto offset = layer->GetSubpixelOffset();
  if (!offset.IsZero())
    ret->emplace_back("subpixel_offset", offset.ToString());
  const auto& rounded_corners = layer->rounded_corner_radii();
  if (!rounded_corners.IsEmpty())
    ret->emplace_back("rounded_corner_radii", rounded_corners.ToString());

  const ui::Layer::ShapeRects* alpha_shape_bounds = layer->alpha_shape();
  if (alpha_shape_bounds && alpha_shape_bounds->size()) {
    gfx::Rect bounding_box = gfx::UnionRects(*alpha_shape_bounds);
    ret->emplace_back("alpha_shape_bounding_box", bounding_box.ToString());
  }

  const cc::Layer* cc_layer = layer->cc_layer_for_testing();
  if (cc_layer) {
    // Property trees must be updated in order to get valid render surface
    // reasons.
    if (!cc_layer->layer_tree_host() ||
        cc_layer->layer_tree_host()->property_trees()->needs_rebuild())
      return;
    cc::RenderSurfaceReason render_surface = cc_layer->GetRenderSurfaceReason();
    if (render_surface != cc::RenderSurfaceReason::kNone) {
      ret->emplace_back("render_surface_reason",
                        cc::RenderSurfaceReasonToString(render_surface));
    }
  }
}

bool SetLayerPropertyFromString(ui::Layer* layer,
                                const std::string& name,
                                const std::string& value) {
  if (!layer) {
    return false;
  }

  if (name == "visible") {
    auto v =
        ui::metadata::TypeConverter<bool>::FromString(base::UTF8ToUTF16(value));
    if (v) {
      layer->SetVisible(*v);
      return true;
    }
  } else if (name == "opacity") {
    double v;
    if (base::StringToDouble(value, &v)) {
      layer->SetOpacity(static_cast<float>(v));
      return true;
    }
  } else if (name == "background_blur") {
    int v;
    if (base::StringToInt(value, &v)) {
      layer->SetBackgroundBlur(v);
      return true;
    }
  } else if (name == "layer_blur") {
    double v;
    if (base::StringToDouble(value, &v)) {
      layer->SetLayerBlur(static_cast<float>(v));
      return true;
    }
  } else if (name == "layer_saturation") {
    double v;
    if (base::StringToDouble(value, &v)) {
      layer->SetLayerSaturation(static_cast<float>(v));
      return true;
    }
  } else if (name == "layer_brightness") {
    double v;
    if (base::StringToDouble(value, &v)) {
      layer->SetLayerBrightness(static_cast<float>(v));
      return true;
    }
  } else if (name == "layer_grayscale") {
    double v;
    if (base::StringToDouble(value, &v)) {
      layer->SetLayerGrayscale(static_cast<float>(v));
      return true;
    }
  } else if (name == "fills_bounds_opaquely") {
    auto v =
        ui::metadata::TypeConverter<bool>::FromString(base::UTF8ToUTF16(value));
    if (v) {
      layer->SetFillsBoundsOpaquely(*v);
      return true;
    }
  } else if (name == "color") {
    auto new_color =
        ui::metadata::SkColorConverter::FromString(base::UTF8ToUTF16(value));
    if (new_color) {
      if (auto* solid_color = layer->AsSolidColor(); solid_color) {
        solid_color->SetColor(SkColor4f::FromColor(*new_color));
        return true;
      }
    }
  } else if (name == "rounded_corner_radii") {
    auto radii = ParseRoundedCornersF(value);
    if (radii) {
      layer->SetRoundedCornerRadius(*radii);
      return true;
    }
  }
  return false;
}

}  // namespace ui_devtools
