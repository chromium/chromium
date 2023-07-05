// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_load_status.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

PolicyLoadStatusSampler::PolicyLoadStatusSampler() {
  Add(POLICY_LOAD_STATUS_STARTED);
}

PolicyLoadStatusSampler::~PolicyLoadStatusSampler() {}

void PolicyLoadStatusSampler::Add(PolicyLoadStatus status) {
  status_bits_[status] = true;
}

}  // namespace policy
