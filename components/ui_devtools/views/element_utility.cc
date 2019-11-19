// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/element_utility.h"

#include "base/strings/string_number_conversions.h"
#include "extensions/common/image_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_owner.h"

namespace ui_devtools {

void AppendLayerPropertiesMatchedStyle(
    const ui::Layer* layer,
    std::vector<UIElement::UIProperty>* ret) {
  ret->emplace_back("layer-type",
                    std::string(LayerTypeToString(layer->type())));
  ret->emplace_back("has-layer-mask",
                    layer->layer_mask_layer() ? "true" : "false");
  ret->emplace_back("layer-opacity", base::NumberToString((layer->opacity())));
  ret->emplace_back("layer-combined-opacity",
                    base::NumberToString(layer->GetCombinedOpacity()));
  ret->emplace_back("background-blur",
                    base::NumberToString(layer->background_blur()));
  ret->emplace_back("layer-blur", base::NumberToString(layer->layer_blur()));
  ret->emplace_back("layer-saturation",
                    base::NumberToString(layer->layer_saturation()));
  ret->emplace_back("layer-brightness",
                    base::NumberToString(layer->layer_brightness()));
  ret->emplace_back("layer-grayscale",
                    base::NumberToString(layer->layer_grayscale()));
  const ui::Layer::ShapeRects* alpha_shape_bounds = layer->alpha_shape();
  if (alpha_shape_bounds && alpha_shape_bounds->size()) {
    gfx::Rect bounding_box;
    for (auto& shape_bound : *alpha_shape_bounds)
      bounding_box.Union(shape_bound);
    ret->emplace_back("alpha-shape-bounding-box", bounding_box.ToString());
  }
  const cc::Layer* cc_layer = layer->cc_layer_for_testing();
  if (cc_layer) {
    // Property trees must be updated in order to get valid render surface
    // reasons.
    if (!cc_layer->layer_tree_host() ||
        cc_layer->layer_tree_host()->property_trees()->needs_rebuild)
      return;
    cc::RenderSurfaceReason render_surface = cc_layer->GetRenderSurfaceReason();
    if (render_surface != cc::RenderSurfaceReason::kNone) {
      ret->emplace_back("render-surface-reason",
                        cc::RenderSurfaceReasonToString(render_surface));
    }
  }
}

bool ParseColorFromFrontend(const std::string& input, std::string* output) {
  std::string value;
  base::TrimWhitespaceASCII(input, base::TRIM_ALL, &value);
  SkColor color;
  if (!extensions::image_util::ParseCssColorString(value, &color))
    return false;
  *output = base::NumberToString(color);
  return true;
}

}  // namespace ui_devtools
