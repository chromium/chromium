// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_IBAN_UI_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_IBAN_UI_H_

namespace autofill {

// The type of save IBAN bubble to show.
enum class IbanBubbleType {
  // Save prompt when the user is saving locally.
  kLocalSave = 0,

  // There is no bubble to show anymore. This also indicates that the icon
  // should not be visible.
  kInactive = 1
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_IBAN_UI_H_
