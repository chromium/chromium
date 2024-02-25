// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/mock_test_payments_network_interface.h"

namespace autofill {

MockTestPaymentsNetworkInterface::MockTestPaymentsNetworkInterface()
    : payments::TestPaymentsNetworkInterface(
          /*url_loader_factory=*/nullptr,
          /*identity_manager=*/nullptr,
          /*personal_data_manager=*/nullptr) {}

MockTestPaymentsNetworkInterface::~MockTestPaymentsNetworkInterface() = default;

}  // namespace autofill
