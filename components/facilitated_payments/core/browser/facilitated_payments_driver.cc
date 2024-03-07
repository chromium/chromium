// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <utility>

namespace payments::facilitated {

FacilitatedPaymentsDriver::FacilitatedPaymentsDriver(
    std::unique_ptr<FacilitatedPaymentsManager> manager)
    : manager_(std::move(manager)) {}

FacilitatedPaymentsDriver::~FacilitatedPaymentsDriver() = default;

void FacilitatedPaymentsDriver::DidFinishNavigation() const {
  manager_->Reset();
}

void FacilitatedPaymentsDriver::DidFinishLoad(
    const GURL& url,
    ukm::SourceId ukm_source_id) const {
  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url, ukm_source_id);
}

}  // namespace payments::facilitated
