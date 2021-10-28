// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/actions/action.h"
#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"
#include "components/arc/input_overlay/touch_id_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace arc {
namespace input_overlay {

Action::Action(aura::Window* window) : target_window_(window) {}

Action::~Action() = default;

bool Action::ParseFromJson(const base::Value& value) {
  // Name can be empty.
  auto* name = value.FindStringKey(kName);
  if (name) {
    name_ = *name;
  }
  // Location can be empty for mouse related actions.
  const base::Value* position = value.FindListKey(kLocation);
  if (position) {
    auto parsed_pos = ParseLocation(*position);
    if (parsed_pos) {
      std::move(parsed_pos->begin(), parsed_pos->end(),
                std::back_inserter(locations_));
    }
  }
  return true;
}

absl::optional<gfx::PointF> Action::CalculateTouchPosition(
    const gfx::RectF& content_bounds) {
  if (locations_.empty())
    return absl::nullopt;
  DCHECK(current_position_index_ < locations_.size());
  Position* position = locations_[current_position_index_].get();
  const gfx::PointF point = position->CalculatePosition(content_bounds);

  float scale = target_window_->GetHost()->device_scale_factor();

  gfx::PointF root_point = gfx::PointF(point);
  gfx::PointF origin = content_bounds.origin();
  root_point.Offset(origin.x(), origin.y());

  gfx::PointF root_location = gfx::PointF(root_point);
  root_location.Scale(scale);

  VLOG(0) << "Calculate touch position: local position {" << point.ToString()
          << "}, root location {" << root_point.ToString()
          << "}, root location in pixels {" << root_location.ToString() << "}";
  return absl::make_optional(root_location);
}

absl::optional<ui::TouchEvent> Action::GetTouchCancelEvent() {
  if (!touch_id_)
    return absl::nullopt;
  auto touch_event = absl::make_optional<ui::TouchEvent>(
      ui::EventType::ET_TOUCH_CANCELLED, last_touch_root_location_,
      last_touch_root_location_, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, touch_id_.value()));
  ui::Event::DispatcherApi(&*touch_event).set_target(target_window_);
  TouchIdManager::GetInstance()->ReleaseTouchID(touch_id_.value());
  touch_id_ = absl::nullopt;
  return touch_event;
}

bool Action::IsKeyAlreadyPressed(ui::DomCode code) const {
  return keys_pressed_.find(code) != keys_pressed_.end();
}

}  // namespace input_overlay
}  // namespace arc
