// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_load_status.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

namespace {

const char kHistogramName[] = "Enterprise.PolicyLoadStatus";

}  // namespace

PolicyLoadStatusSampler::PolicyLoadStatusSampler() {
  Add(POLICY_LOAD_STATUS_STARTED);
}

PolicyLoadStatusSampler::~PolicyLoadStatusSampler() {}

void PolicyLoadStatusSampler::Add(PolicyLoadStatus status) {
  status_bits_[status] = true;
}

PolicyLoadStatusUmaReporter::PolicyLoadStatusUmaReporter() {}

PolicyLoadStatusUmaReporter::~PolicyLoadStatusUmaReporter() {
  base::HistogramBase* histogram(base::LinearHistogram::FactoryGet(
      kHistogramName, 1, POLICY_LOAD_STATUS_SIZE, POLICY_LOAD_STATUS_SIZE + 1,
      base::Histogram::kUmaTargetedHistogramFlag));

  for (int i = 0; i < POLICY_LOAD_STATUS_SIZE; ++i) {
    if (GetStatusSet()[i])
      histogram->Add(i);
  }
}

}  // namespace policy
