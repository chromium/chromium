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
    const blink::ViewTransitionToken& transition_token,
    uint32_t local_id)
    : transition_token_(transition_token), local_id_(local_id) {
  CHECK_NE(local_id, kInvalidLocalId);
}

bool operator==(const ViewTransitionElementResourceId& lhs,
                const ViewTransitionElementResourceId& rhs) {
  if (!lhs.IsValid()) {
    return !rhs.IsValid();
  }

  return lhs.local_id_ == rhs.local_id_ &&
         lhs.transition_token_ == rhs.transition_token_;
}

bool ViewTransitionElementResourceId::IsValid() const {
  return local_id_ != kInvalidLocalId;
}

std::string ViewTransitionElementResourceId::ToString() const {
  return base::StringPrintf(
      "ViewTransitionElementResourceId : %u [transition: %s]", local_id_,
      transition_token_ ? transition_token_->ToString().c_str() : "invalid");
}

}  // namespace viz
