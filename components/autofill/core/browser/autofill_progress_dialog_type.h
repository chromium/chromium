// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_

namespace autofill {

// TODO(crbug.com/430575808): Rename this to `AutofillProgressUiType` as it will
// not be a "dialog" on mobile.
// The type of autofill progress dialog to show.
enum class AutofillProgressDialogType {
  // Unspecified progress dialog type.
  kUnspecified = 0,
  kVirtualCardUnmaskProgressDialog,
  // Used when conducting a risk-based check for masked server card.
  kServerCardUnmaskProgressDialog,
  // Used when unmasking server IBANs.
  kServerIbanUnmaskProgressDialog,
  // Used in the VCN 3DS authentication flow after closure of the pop-up, while
  // a Payments server call is being made to fetch the resulting virtual card.
  k3dsFetchVcnProgressDialog,
  // Used when unmasking a card info retrieval enrolled card.
  kCardInfoRetrievalEnrolledUnmaskProgressDialog,
  // Used when fetching VCN details during a BNPL transaction.
  kBnplFetchVcnProgressDialog,
  // Used when extracting the checkout amount during a BNPL transaction.
  kBnplAmountExtractionProgressUi,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_
