// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_PROGRESS_UI_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_PROGRESS_UI_TYPE_H_

namespace autofill {

// The type of autofill progress UI to show.
enum class AutofillProgressUiType {
  // Unspecified progress UI type.
  kUnspecified = 0,
  kVirtualCardUnmaskProgressUi,
  // Used when conducting a risk-based check for masked server card.
  kServerCardUnmaskProgressUi,
  // Used when unmasking server IBANs.
  kServerIbanUnmaskProgressUi,
  // Used in the VCN 3DS authentication flow after closure of the pop-up, while
  // a Payments server call is being made to fetch the resulting virtual card.
  k3dsFetchVcnProgressUi,
  // Used when unmasking a card info retrieval enrolled card.
  kCardInfoRetrievalEnrolledUnmaskProgressUi,
  // Used when fetching VCN details during a BNPL transaction.
  kBnplFetchVcnProgressUi,
  // Used when extracting the checkout amount during a BNPL transaction.
  kBnplAmountExtractionProgressUi,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_PROGRESS_UI_TYPE_H_
