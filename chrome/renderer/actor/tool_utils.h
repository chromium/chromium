// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_UTILS_H_
#define CHROME_RENDERER_ACTOR_TOOL_UTILS_H_

#include <cstdint>
#include <optional>

namespace blink {
class WebNode;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace actor {

blink::WebNode GetNodeFromId(const content::RenderFrame& frame,
                             int32_t node_id);
std::optional<gfx::PointF> InteractionPointFromWebNode(
    const blink::WebNode& node);
}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_UTILS_H_
