// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_

namespace autofill {

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
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_
