// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_

namespace autofill::payments {

// Interface for objects that define a strategy for handling a BNPL autofill
// flow with different implementations meant to handle different operating
// systems. Created lazily in the PaymentsAutofillClient when it is needed.
class BnplStrategy {
 public:
  virtual ~BnplStrategy();
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_
