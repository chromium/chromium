// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/view_transition_element_resource_id.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace viz {

ViewTransitionElementResourceId::ViewTransitionElementResourceId() = default;
ViewTransitionElementResourceId::~ViewTransitionElementResourceId() = default;

ViewTransitionElementResourceId::ViewTransitionElementResourceId(
    const TransitionId& transition_id,
    uint32_t local_id)
    : transition_id_(transition_id), local_id_(local_id) {
  CHECK_NE(local_id, kInvalidLocalId);
  CHECK(!transition_id.is_empty());
}

bool ViewTransitionElementResourceId::IsValid() const {
  return local_id_ != kInvalidLocalId;
}

std::string ViewTransitionElementResourceId::ToString() const {
  return base::StringPrintf(
      "ViewTransitionElementResourceId : %u [transition: %s]", local_id_,
      transition_id_.ToString().c_str());
}

}  // namespace viz
