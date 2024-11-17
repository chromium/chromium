// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"

namespace payments::facilitated {

MockFacilitatedPaymentsApiClient::MockFacilitatedPaymentsApiClient() = default;

MockFacilitatedPaymentsApiClient::~MockFacilitatedPaymentsApiClient() = default;

std::unique_ptr<FacilitatedPaymentsApiClient>
MockFacilitatedPaymentsApiClient::CreateApiClient() {
  return std::make_unique<MockFacilitatedPaymentsApiClient>();
}

}  // namespace payments::facilitated
