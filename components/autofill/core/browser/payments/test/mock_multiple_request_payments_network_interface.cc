// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"

#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill::payments {

MockMultipleRequestPaymentsNetworkInterface::
    MockMultipleRequestPaymentsNetworkInterface(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        signin::IdentityManager& identity_manager)
    : MultipleRequestPaymentsNetworkInterface(url_loader_factory,
                                              identity_manager,
                                              /*is_off_the_record=*/false) {}

MockMultipleRequestPaymentsNetworkInterface::
    ~MockMultipleRequestPaymentsNetworkInterface() = default;

}  // namespace autofill::payments
