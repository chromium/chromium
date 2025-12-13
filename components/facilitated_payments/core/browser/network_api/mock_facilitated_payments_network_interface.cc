// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"

namespace payments::facilitated {

MockFacilitatedPaymentsNetworkInterface::
    MockFacilitatedPaymentsNetworkInterface(
        signin::IdentityManager& identity_manager,
        autofill::AccountInfoGetter& account_info_getter)
    : FacilitatedPaymentsNetworkInterface(
          /*url_loader_factory=*/nullptr,
          /*identity_manager=*/identity_manager,
          /*account_info_getter=*/account_info_getter,
          /*is_off_the_record=*/false) {}

MockFacilitatedPaymentsNetworkInterface::
    ~MockFacilitatedPaymentsNetworkInterface() = default;

}  // namespace payments::facilitated
