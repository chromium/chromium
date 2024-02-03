// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_

#include <optional>
#include <string>

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "url/gurl.h"

namespace autofill::payments {

// Interface for objects that manage popup-related redirect flows for payments
// autofill, with different implementations meant to handle different operating
// systems.
class PaymentsWindowManager {
 public:
  // The contextual data required for the VCN 3DS flow.
  struct Vcn3dsContext {
    CreditCard card;
    std::string context_token;
    GURL url;
  };

  virtual ~PaymentsWindowManager() = default;

  // Initiates the VCN 3DS auth flow. All fields in `context` must be valid and
  // non-empty.
  virtual void InitVcn3dsAuthentication(Vcn3dsContext context) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
