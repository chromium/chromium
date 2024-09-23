// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_ELEMENT_UTILITY_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_ELEMENT_UTILITY_H_

#include <string>
#include <vector>

#include "components/ui_devtools/ui_element.h"

namespace ui {
class Layer;
}

namespace ui_devtools {

// TODO(crbug.com/41340254): Remove this file when LayerElement exists

// Appends Layer properties to ret (ex: layer-type, layer-mask, etc).
// This is used to display information about the layer on devtools.
// Note that ret may not be empty when it's passed in.
void AppendLayerPropertiesMatchedStyle(const ui::Layer* layer,
                                       std::vector<UIElement::UIProperty>* ret);

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_ELEMENT_UTILITY_H_
