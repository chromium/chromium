// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include "base/functional/callback_helpers.h"

namespace payments::facilitated {

FacilitatedPaymentsManager::FacilitatedPaymentsManager(
    FacilitatedPaymentsDriver* driver)
    : driver_(*driver) {}

FacilitatedPaymentsManager::~FacilitatedPaymentsManager() = default;

void FacilitatedPaymentsManager::DidFinishLoad(const GURL& url) const {
  if (!ShouldDetectPixCode(url)) {
    return;
  }
  driver_->TriggerPixCodeDetection(
      base::BindOnce(&FacilitatedPaymentsManager::ProcessPixCodeDetectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool FacilitatedPaymentsManager::ShouldDetectPixCode(const GURL& url) const {
  return true;
}

void FacilitatedPaymentsManager::ProcessPixCodeDetectionResult(
    bool pix_code_found) const {}

}  // namespace payments::facilitated
