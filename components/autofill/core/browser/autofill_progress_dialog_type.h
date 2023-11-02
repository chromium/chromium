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
  // Used when authenticating with FIDO.
  // This progress dialog type applies to Android only.
  kAndroidFIDOProgressDialog = 1,
  // Used when unmasking virtual cards.
  kVirtualCardUnmaskProgressDialog = 2
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROGRESS_DIALOG_TYPE_H_
