// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_utils.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {

namespace {

using ::blink::WebNode;
using ::blink::WebWidget;

std::vector<gfx::Rect> getNewHitBoxesFor(gfx::Rect& rect,
                                         const gfx::Rect exclusion) {
  std::vector<gfx::Rect> new_rects;
  // This function only makes sense if at least part of the exclusion falls
  // inside of `rect`.
  DCHECK(rect.Intersects(exclusion));
  DCHECK(!exclusion.Contains(rect));

  // There are 4 possible rects. We handle the top and bottom first so that we
  // can assign the corners to them.

  // The y position and height of the left and right rects. We may have to
  // adjust the y position if the top or bottom are assigned to avoid
  // overlapping.
  int side_y = rect.y();
  int side_height = rect.height();

  // Handle above the exclusion.
  if (rect.y() < exclusion.y()) {
    // Left and right rects will start at the exclusion y position.
    side_y = exclusion.y();
    side_height -= exclusion.y() - rect.y();

    new_rects.emplace_back(rect.x(), rect.y(), rect.width(),
                           exclusion.y() - rect.y());
  }
  // Handle below the exclusion.
  if (rect.bottom() > exclusion.bottom()) {
    // Left and right rects end at the exclusion y position + height.
    side_height -= rect.bottom() - exclusion.bottom();

    new_rects.emplace_back(rect.x(), exclusion.bottom(), rect.width(),
                           rect.bottom() - exclusion.bottom());
  }

  // Handle left of the exclusion.
  if (rect.x() < exclusion.x()) {
    new_rects.emplace_back(rect.x(), side_y, exclusion.x() - rect.x(),
                           side_height);
  }
  // Handle to the right of the exclusion.
  if (rect.right() > exclusion.right()) {
    new_rects.emplace_back(exclusion.right(), side_y,
                           rect.right() - exclusion.right(), side_height);
  }
  return new_rects;
}

std::vector<gfx::Rect> getHitBoxesForElement(blink::WebElement& element) {
  gfx::Rect visible_rect = element.VisibleBoundsInWidget();
  if (visible_rect.IsEmpty()) {
    return {};
  }

  std::vector<gfx::Rect> rects = element.ClientRectsInWidget();
  for (auto& rect : rects) {
    rect.InclusiveIntersect(visible_rect);
  }
  std::erase_if(rects, [](const gfx::Rect& rect) { return rect.IsEmpty(); });

  // Just ignore some boxes if there are too many.
  rects.resize(std::min(rects.size(), InteractionPointRefiner::kMaxRects));
  return rects;
}

}  // namespace

InteractionPointRefiner::InteractionPointRefiner(const blink::WebNode& node) {
  blink::WebElement element = node.DynamicTo<blink::WebElement>();
  if (!element.IsNull()) {
    hit_boxes_ = getHitBoxesForElement(element);
  }
}
InteractionPointRefiner::InteractionPointRefiner(std::vector<gfx::Rect> rects)
    : hit_boxes_(rects) {}
InteractionPointRefiner::~InteractionPointRefiner() = default;
InteractionPointRefiner::InteractionPointRefiner(InteractionPointRefiner&&) =
    default;
InteractionPointRefiner& InteractionPointRefiner::operator=(
    InteractionPointRefiner&&) = default;

std::optional<gfx::PointF> InteractionPointRefiner::GetPoint() const {
  for (const auto& rect : hit_boxes_) {
    if (!rect.IsEmpty()) {
      return gfx::PointF(rect.CenterPoint());
    }
  }
  return std::nullopt;
}

void InteractionPointRefiner::ExcludeElement(blink::WebElement& element) {
  std::vector<gfx::Rect> exclusions = getHitBoxesForElement(element);
  for (const auto& exclusion : exclusions) {
    AddExclusion(exclusion);
  }
}

void InteractionPointRefiner::AddExclusion(const gfx::Rect& exclusion) {
  std::vector<gfx::Rect> saved_hit_boxes;
  std::vector<gfx::Rect> new_hit_boxes;
  for (auto& rect : hit_boxes_) {
    if (new_hit_boxes.size() > kMaxRects) {
      // Avoid too much churn in the degenerate case. Just ignore some boxes
      // if there are too many.
      break;
    }
    if (!rect.Intersects(exclusion)) {
      // If the exclusion doesn't affect the rect, keep it as-is.
      saved_hit_boxes.emplace_back(std::move(rect));
      continue;
    }
    if (exclusion.Contains(rect)) {
      // Drop the rect if it is completely contained in the exclusion.
      continue;
    }
    std::vector<gfx::Rect> new_boxes = getNewHitBoxesFor(rect, exclusion);
    new_hit_boxes.insert(new_hit_boxes.end(), new_boxes.begin(),
                         new_boxes.end());
  }
  // Put the unmodified rects first so we effectively do a breadth first
  // search.
  saved_hit_boxes.insert(saved_hit_boxes.end(), new_hit_boxes.begin(),
                         new_hit_boxes.end());
  std::swap(saved_hit_boxes, hit_boxes_);
}

std::optional<gfx::PointF> InteractionPointFromWebNode(
    WebWidget* widget,
    const blink::WebNode& node) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicActorIterativeInteractionPointDiscovery)) {
    blink::WebElement element = node.DynamicTo<blink::WebElement>();
    if (element.IsNull()) {
      return std::nullopt;
    }

    gfx::Rect visible_rect = element.VisibleBoundsInWidget();
    if (visible_rect.IsEmpty()) {
      return std::nullopt;
    }

    std::vector<gfx::Rect> rects = element.ClientRectsInWidget();
    for (auto rect : rects) {
      rect.InclusiveIntersect(visible_rect);
      if (!rect.IsEmpty()) {
        return gfx::PointF(rect.CenterPoint());
      }
    }
    return std::nullopt;
  }

  // Iteratively find an acceptable interaction point. Each iteration we
  // perform a hit test for the center of one of our rectangles. If we hit
  // something other than the tree we expect, we exclude the bounds of that
  // element from further checking. This effectively means we find a point
  // to interact with in O(log(N)) time, if it exists.
  InteractionPointRefiner ipr(node);
  size_t iterations = 0;
  std::optional<gfx::PointF> first_point = ipr.GetPoint();
  if (!first_point.has_value()) {
    return std::nullopt;
  }
  DCHECK(widget);
  for (std::optional<gfx::PointF> test_point = ipr.GetPoint();
       test_point.has_value(); test_point = ipr.GetPoint()) {
    const blink::WebHitTestResult hit_test_result =
        widget->HitTestResultAt(test_point.value());
    blink::WebElement hit_element = hit_test_result.GetElement();

    // The action target from APC is not as granular as the live DOM hit
    // test. Include shadow host element as the hit test would land on
    // those. Also check if the hit element was pulled in via a Web
    // Components slot.
    if (node.ContainsViaFlatTree(&hit_element)) {
      // Found an interaction point.
      return test_point;
    }
    if (++iterations ==
        features::kGlicActorInterationPointDiscoveryMaxIterations.Get()) {
      break;
    }

    ipr.ExcludeElement(hit_element);
  }
  return first_point;
}

blink::WebNode GetNodeFromId(const content::RenderFrame& local_root_frame,
                             int32_t node_id) {
  const blink::WebLocalFrame* web_frame = local_root_frame.GetWebFrame();
  if (!web_frame) {
    return blink::WebNode();
  }

  // The passed in frame must be a local root.
  CHECK_EQ(web_frame, web_frame->LocalRoot());

  blink::WebNode node = blink::WebNode::FromDomNodeId(node_id);
  // Make sure the node we're getting belongs to a frame under the local root
  // frame.
  if (node.IsNull() || !node.GetDocument() || !node.GetDocument().GetFrame() ||
      node.GetDocument().GetFrame()->LocalRoot() != web_frame) {
    return blink::WebNode();
  }
  return node;
}

bool IsNodeFocused(const content::RenderFrame& frame,
                   const blink::WebNode& node) {
  blink::WebDocument document = frame.GetWebFrame()->GetDocument();
  blink::WebElement currently_focused = document.FocusedElement();
  blink::WebElement element = node.To<blink::WebElement>();
  return element == currently_focused;
}

bool IsPointWithinViewport(const gfx::Point& point,
                           const content::RenderFrame& frame) {
  CHECK(frame.GetWebFrame());
  CHECK_EQ(frame.GetWebFrame(), frame.GetWebFrame()->LocalRoot());
  gfx::Rect viewport(frame.GetWebFrame()->FrameWidget()->VisibleViewportSize());
  return viewport.Contains(point);
}

bool IsPointWithinViewport(const gfx::PointF& point,
                           const content::RenderFrame& frame) {
  return IsPointWithinViewport(gfx::ToFlooredPoint(point), frame);
}

std::string ToDebugString(const mojom::ToolTargetPtr& target) {
  if (target.is_null()) {
    return "target(null)";
  }

  std::stringstream ss;
  ss << "target(";
  if (target->is_coordinate_dip()) {
    ss << "XY[DIP]=" << target->get_coordinate_dip().x() << ","
       << target->get_coordinate_dip().y();
  } else {
    ss << "ID=" << target->get_dom_node_id();
  }
  ss << ")";
  return ss.str();
}

bool IsNodeWithinViewport(const blink::WebNode& node) {
  blink::WebElement element = node.DynamicTo<blink::WebElement>();
  if (element.IsNull()) {
    return false;
  }

  gfx::Rect rect = element.VisibleBoundsInWidget();
  return !rect.IsEmpty();
}

std::string NodeToDebugString(const blink::WebNode& node) {
  if (node.IsNull()) {
    return "";
  }
  if (node.IsTextNode()) {
    // Truncate it to 100 characters, enough for debugging.
    return base::StrCat({"text=", node.NodeValue().Substring(0u, 100u).Utf8()});
  }
  if (node.IsElementNode()) {
    const blink::WebElement element = node.DynamicTo<blink::WebElement>();
    return base::StrCat({element.TagName().Utf8(),
                         " id=", element.GetIdAttribute().Utf8(),
                         " class=", element.GetAttribute("class").Utf8()});
  }
  if (node.IsDocumentNode()) {
    return "document";
  }
  return "";
}

}  // namespace actor
