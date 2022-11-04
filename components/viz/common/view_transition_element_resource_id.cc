// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/view_transition_element_resource_id.h"

#include "base/atomic_sequence_num.h"
#include "base/strings/stringprintf.h"

namespace viz {
namespace {

static base::AtomicSequenceNumber s_view_transition_element_resource_id;

constexpr uint32_t kInvalidId = 0u;

}  // namespace

ViewTransitionElementResourceId ViewTransitionElementResourceId::Generate() {
  return ViewTransitionElementResourceId(
      s_view_transition_element_resource_id.GetNext() + 1);
}

ViewTransitionElementResourceId::ViewTransitionElementResourceId()
    : ViewTransitionElementResourceId(kInvalidId) {}

ViewTransitionElementResourceId::~ViewTransitionElementResourceId() = default;

ViewTransitionElementResourceId::ViewTransitionElementResourceId(uint32_t id)
    : id_(id) {}

bool ViewTransitionElementResourceId::IsValid() const {
  return id_ != kInvalidId;
}

std::string ViewTransitionElementResourceId::ToString() const {
  return base::StringPrintf("ViewTransitionElementResourceId : %u", id_);
}

}  // namespace viz
