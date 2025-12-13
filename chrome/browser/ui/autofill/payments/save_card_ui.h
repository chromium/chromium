// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_

namespace autofill {

// The type of save card bubble to show.
enum class PaymentsBubbleType {
  // Save prompt when the user is saving locally.
  kLocalSave,

  // Save prompt for saving CVC locally to an existing local card.
  kLocalCvcSave,

  // Save prompt when uploading a card to Google Payments.
  kUploadSave,

  // Save prompt for uploading CVC to the Sync server for an existing server
  // card.
  kUploadCvcSave,

  // Credit card upload is in progress.
  kUploadInProgress,

  // Credit card upload is completed.
  kUploadComplete,

  // The manage cards bubble when bubble is reshown after
  // icon is clicked.
  kManageCards,

  // There is no bubble to show anymore. This also
  // indicates that the icon should not be visible.
  kInactive
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_
