// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_area.h"

#include <algorithm>
#include <ostream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"

namespace autofill_assistant {

namespace {

// Returns a debug representation of |rectangles|.
std::string ToDebugString(const std::vector<RectF>& rectangles) {
  if (rectangles.empty()) {
    return std::string();
  }

  std::ostringstream stream;
  std::string separator;
  for (const auto& rect : rectangles) {
    stream << separator << rect;
    separator = ", ";
  }

  return stream.str();
}

}  // namespace

ElementArea::ElementArea(const ClientSettings* settings,
                         WebController* web_controller)
    : settings_(settings), web_controller_(web_controller) {
  DCHECK(settings_ && web_controller_);
}

ElementArea::~ElementArea() = default;

void ElementArea::Clear() {
  SetFromProto(ElementAreaProto());
}

void ElementArea::SetFromProto(const ElementAreaProto& proto) {
  rectangles_.clear();
  last_visual_viewport_ = RectF();
  last_rectangles_.clear();

  AddRectangles(proto.touchable(), /* restricted= */ false);
  AddRectangles(proto.restricted(), /* restricted= */ true);

  if (rectangles_.empty()) {
    timer_.Stop();
    ReportUpdate();
    return;
  }

  Update();
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, settings_->element_position_update_interval,
        base::BindRepeating(
            &ElementArea::Update,
            // This ElementArea instance owns |update_element_positions_|
            base::Unretained(this)));
  }
}

void ElementArea::AddRectangles(
    const ::google::protobuf::RepeatedPtrField<ElementAreaProto::Rectangle>&
        rectangles_proto,
    bool restricted) {
  for (const auto& rectangle_proto : rectangles_proto) {
    rectangles_.emplace_back();
    Rectangle& rectangle = rectangles_.back();
    rectangle.full_width = rectangle_proto.full_width();
    rectangle.restricted = restricted;
    VLOG(3) << "Rectangle (full_width="
            << (rectangle.full_width ? "true" : "false")
            << ", restricted=" << (restricted ? "true" : "false") << "):";
    for (const auto& element_proto : rectangle_proto.elements()) {
      rectangle.positions.emplace_back();
      ElementPosition& position = rectangle.positions.back();
      position.selector = Selector(element_proto);
      DVLOG(3) << "  " << position.selector;
    }
  }
}

void ElementArea::Update() {
  if (rectangles_.empty())
    return;

  // If anything is still pending, skip the update.
  if (visual_viewport_pending_update_) {
    return;
  }
  for (auto& rectangle : rectangles_) {
    if (rectangle.IsPending())
      return;
  }

  // Mark everything as pending at the same time, to avoid reporting partial
  // results.
  visual_viewport_pending_update_ = true;
  for (auto& rectangle : rectangles_) {
    for (auto& position : rectangle.positions) {
      // To avoid reporting partial rectangles, all element positions become
      // pending at the same time.
      position.pending_update = true;
    }
  }

  // Viewport and element positions are always queried, and so reported, at the
  // same time. This allows supporting both elements whose position is relative
  // (and move with a scroll) as elements whose position is absolute (and don't
  // move with a scroll.) Being able to tell the difference would be more
  // effective and allow refreshing element positions less aggressively.
  web_controller_->GetVisualViewport(base::BindOnce(
      &ElementArea::OnGetVisualViewport, weak_ptr_factory_.GetWeakPtr()));

  for (auto& rectangle : rectangles_) {
    for (auto& position : rectangle.positions) {
      web_controller_->FindElement(
          position.selector, /* strict= */ true,
          base::BindOnce(
              &element_action_util::TakeElementAndGetProperty<const RectF&>,
              base::BindOnce(&WebController::GetElementRect,
                             web_controller_->GetWeakPtr()),
              RectF(),
              base::BindOnce(&ElementArea::OnGetElementRect,
                             weak_ptr_factory_.GetWeakPtr(),
                             position.selector)));
    }
  }
}

void ElementArea::GetTouchableRectangles(std::vector<RectF>* area) {
  for (auto& rectangle : rectangles_) {
    if (!rectangle.restricted) {
      area->emplace_back();
      rectangle.FillRect(&area->back());
    }
  }
}

void ElementArea::GetRestrictedRectangles(std::vector<RectF>* area) {
  for (auto& rectangle : rectangles_) {
    if (rectangle.restricted) {
      area->emplace_back();
      rectangle.FillRect(&area->back());
    }
  }
}

ElementArea::ElementPosition::ElementPosition() = default;
ElementArea::ElementPosition::ElementPosition(const ElementPosition& orig) =
    default;
ElementArea::ElementPosition::~ElementPosition() = default;
bool ElementArea::ElementPosition::operator==(
    const ElementPosition& another) const {
  return selector == another.selector && rect == another.rect;
}

ElementArea::Rectangle::Rectangle() = default;
ElementArea::Rectangle::Rectangle(const Rectangle& orig) = default;
ElementArea::Rectangle::~Rectangle() = default;

bool ElementArea::Rectangle::operator==(const Rectangle& another) const {
  return full_width == another.full_width && restricted == another.restricted &&
         positions == another.positions;
}

bool ElementArea::Rectangle::IsPending() const {
  for (const auto& position : positions) {
    if (position.pending_update)
      return true;
  }
  return false;
}

void ElementArea::Rectangle::FillRect(RectF* rect) const {
  bool has_first_rect = false;
  for (const auto& position : positions) {
    if (position.rect.empty()) {
      continue;
    }

    if (!has_first_rect) {
      *rect = position.rect;
      has_first_rect = true;
      continue;
    }
    rect->top = std::min(rect->top, position.rect.top);
    rect->bottom = std::max(rect->bottom, position.rect.bottom);
    rect->left = std::min(rect->left, position.rect.left);
    rect->right = std::max(rect->right, position.rect.right);
  }
  rect->full_width = full_width;
  return;
}

void ElementArea::OnGetElementRect(const Selector& selector,
                                   const ClientStatus& rect_status,
                                   const RectF& rect) {
  // !rect_status.ok() has all coordinates set to 0.0, which clears the area.
  bool updated = false;
  for (auto& rectangle : rectangles_) {
    for (auto& position : rectangle.positions) {
      if (selector == position.selector) {
        position.pending_update = false;
        position.rect = rect;
        updated = true;
      }
    }
  }

  if (updated) {
    ReportUpdate();
  }
  // If the set of elements has changed, the given selector will not be found in
  // rectangles_. This is fine.
}

void ElementArea::OnGetVisualViewport(const ClientStatus& rect_status,
                                      const RectF& rect) {
  if (!visual_viewport_pending_update_)
    return;

  visual_viewport_pending_update_ = false;
  if (!rect_status.ok())
    return;

  visual_viewport_ = rect;
  ReportUpdate();
}

void ElementArea::ReportUpdate() {
  if (!on_update_)
    return;

  if (rectangles_.empty()) {
    // Reporting of visual viewport is best effort when reporting empty
    // rectangles. It might also be empty.
    on_update_.Run(visual_viewport_, {}, {});
    return;
  }

  // If there are rectangles, delay reporting until both the visual viewport
  // size and the rectangles are available.
  if (visual_viewport_pending_update_) {
    return;
  }

  for (const auto& rectangle : rectangles_) {
    if (rectangle.IsPending()) {
      // We don't have everything we need yet
      return;
    }
  }

  if (visual_viewport_ == last_visual_viewport_ &&
      rectangles_ == last_rectangles_) {
    // The positions have not changed since the last update.
    return;
  }

  std::vector<RectF> touchable_area;
  std::vector<RectF> restricted_area;
  GetTouchableRectangles(&touchable_area);
  GetRestrictedRectangles(&restricted_area);
  if (VLOG_IS_ON(3)) {
    if (!(visual_viewport_ == last_visual_viewport_)) {
      VLOG(3) << "New viewport: " << visual_viewport_;
    }
    if (!(rectangles_ == last_rectangles_)) {
      if (!touchable_area.empty()) {
        VLOG(3) << "New touchable rects: " << ToDebugString(touchable_area);
      }
      if (!restricted_area.empty()) {
        VLOG(3) << "New restricted rects: " << ToDebugString(restricted_area);
      }
    }
  }

  last_visual_viewport_ = visual_viewport_;
  last_rectangles_ = rectangles_;
  on_update_.Run(visual_viewport_, touchable_area, restricted_area);
}

}  // namespace autofill_assistant
