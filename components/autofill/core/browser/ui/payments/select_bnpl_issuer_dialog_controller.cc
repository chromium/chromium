// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"

#include <utility>

namespace autofill::payments {

BnplIssuerContext::BnplIssuerContext() = default;

BnplIssuerContext::BnplIssuerContext(BnplIssuer issuer,
                                     BnplIssuerEligibilityForPage eligibility)
    : issuer(std::move(issuer)), eligibility(std::move(eligibility)) {}

BnplIssuerContext::BnplIssuerContext(const BnplIssuerContext& other) = default;

BnplIssuerContext::BnplIssuerContext(BnplIssuerContext&&) = default;

BnplIssuerContext& BnplIssuerContext::operator=(
    const BnplIssuerContext& other) = default;

BnplIssuerContext& BnplIssuerContext::operator=(BnplIssuerContext&&) = default;

BnplIssuerContext::~BnplIssuerContext() = default;

bool BnplIssuerContext::operator==(const BnplIssuerContext&) const = default;

}  // namespace autofill::payments
