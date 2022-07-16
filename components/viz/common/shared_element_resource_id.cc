// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/shared_element_resource_id.h"

#include "base/atomic_sequence_num.h"
#include "base/strings/stringprintf.h"

namespace viz {
namespace {

static base::AtomicSequenceNumber s_shared_element_resource_id;

constexpr uint32_t kInvalidId = 0u;

}  // namespace

SharedElementResourceId SharedElementResourceId::Generate() {
  return SharedElementResourceId(s_shared_element_resource_id.GetNext() + 1);
}

SharedElementResourceId::SharedElementResourceId()
    : SharedElementResourceId(kInvalidId) {}

SharedElementResourceId::~SharedElementResourceId() = default;

SharedElementResourceId::SharedElementResourceId(uint32_t id) : id_(id) {}

bool SharedElementResourceId::IsValid() const {
  return id_ != kInvalidId;
}

std::string SharedElementResourceId::ToString() const {
  return base::StringPrintf("SharedElementResourceId : %u", id_);
}

}  // namespace viz
