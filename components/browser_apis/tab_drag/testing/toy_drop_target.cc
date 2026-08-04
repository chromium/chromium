// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_drop_target.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/types/expected.h"

namespace tabs_api {

ToyDropTarget::ToyDropTarget() = default;
ToyDropTarget::~ToyDropTarget() = default;

void ToyDropTarget::OnDragEntered(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& local_point,
    int32_t tab_original_offset_x) {
  events_.push_back(
      {ReceivedEvent::Type::kEntered, source_tab_ids, local_point});
}

void ToyDropTarget::OnDrag(const gfx::Point& local_point) {
  events_.push_back({ReceivedEvent::Type::kDrag, {}, local_point});
}

void ToyDropTarget::OnDragLeave() {
  events_.push_back({ReceivedEvent::Type::kLeave, {}, gfx::Point()});
}

void ToyDropTarget::OnDrop(const std::vector<tabs_api::NodeId>& source_tab_ids,
                           const gfx::Point& local_point) {
  events_.push_back({ReceivedEvent::Type::kDrop, source_tab_ids, local_point});
}

void ToyDropTarget::OnDragCancelled() {
  events_.push_back({ReceivedEvent::Type::kCancelled, {}, gfx::Point()});
}

void ToyDropTarget::ClearEvents() {
  events_.clear();
}

}  // namespace tabs_api
