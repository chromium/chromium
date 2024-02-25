// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_window_manager.h"

namespace autofill::payments {

PaymentsWindowManager::Vcn3dsAuthenticationResponse::
    Vcn3dsAuthenticationResponse() = default;
PaymentsWindowManager::Vcn3dsAuthenticationResponse::
    Vcn3dsAuthenticationResponse(
        const PaymentsWindowManager::Vcn3dsAuthenticationResponse&) = default;
PaymentsWindowManager::Vcn3dsAuthenticationResponse::
    Vcn3dsAuthenticationResponse(
        PaymentsWindowManager::Vcn3dsAuthenticationResponse&&) = default;
PaymentsWindowManager::Vcn3dsAuthenticationResponse&
PaymentsWindowManager::Vcn3dsAuthenticationResponse::operator=(
    const PaymentsWindowManager::Vcn3dsAuthenticationResponse&) = default;
PaymentsWindowManager::Vcn3dsAuthenticationResponse&
PaymentsWindowManager::Vcn3dsAuthenticationResponse::operator=(
    PaymentsWindowManager::Vcn3dsAuthenticationResponse&&) = default;
PaymentsWindowManager::Vcn3dsAuthenticationResponse::
    ~Vcn3dsAuthenticationResponse() = default;

PaymentsWindowManager::Vcn3dsContext::Vcn3dsContext() = default;
PaymentsWindowManager::Vcn3dsContext::Vcn3dsContext(
    PaymentsWindowManager::Vcn3dsContext&&) = default;
PaymentsWindowManager::Vcn3dsContext&
PaymentsWindowManager::Vcn3dsContext::operator=(
    PaymentsWindowManager::Vcn3dsContext&&) = default;
PaymentsWindowManager::Vcn3dsContext::~Vcn3dsContext() = default;

}  // namespace autofill::payments
