// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PROTO_PROTO_CONVERTOR_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PROTO_PROTO_CONVERTOR_H_

#include <map>
#include <string>

#include "ui/accessibility/ax_tree_update.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace screen_ai {

// Converts serialized VisualAnnotation proto from ScreenAI to AXTreeUpdate. The
// argument `image_rect` is the bounding box of the image from which the visual
// annotation was created.
ui::AXTreeUpdate ScreenAIVisualAnnotationToAXTreeUpdate(
    const std::string& serialized_proto,
    const gfx::Rect& image_rect);

// Converts an AXTreeUpdate snapshot to serialized ViewHierarchy proto for
// Screen2X.
std::string Screen2xSnapshotToViewHierarchy(const ui::AXTreeUpdate& snapshot);

// Returns a map of Screen2x role strings to Chrome roles.
const std::map<std::string, ax::mojom::Role>&
GetScreen2xToChromeRoleConversionMapForTesting();

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PROTO_PROTO_CONVERTOR_H_
