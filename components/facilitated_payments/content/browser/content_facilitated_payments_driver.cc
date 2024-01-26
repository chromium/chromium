// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/functional/callback.h"

namespace payments::facilitated {

class FaciliatedPaymentsManager;

ContentFacilitatedPaymentsDriver::ContentFacilitatedPaymentsDriver()
    : FacilitatedPaymentsDriver(
          std::make_unique<FacilitatedPaymentsManager>(this)) {}

ContentFacilitatedPaymentsDriver::~ContentFacilitatedPaymentsDriver() = default;

void ContentFacilitatedPaymentsDriver::TriggerPixCodeDetection(
    base::OnceCallback<void(bool)> callback) const {}

}  // namespace payments::facilitated
