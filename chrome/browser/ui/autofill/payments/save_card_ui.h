// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_

namespace autofill {

// The type of save card bubble to show.
enum class BubbleType {
  // Save prompt when the user is saving locally.
  LOCAL_SAVE,

  // Save prompt for saving CVC locally to an existing local card.
  LOCAL_CVC_SAVE,

  // Save prompt when uploading a card to Google Payments.
  UPLOAD_SAVE,

  // Save prompt for uploading CVC to the Sync server for an existing server
  // card.
  UPLOAD_CVC_SAVE,

  // Credit card upload is in progress. No bubble visible but show the credit
  // card icon with the loading indicator animation.
  UPLOAD_IN_PROGRESS,

  // The manage cards bubble when bubble is reshown after
  // icon is clicked.
  MANAGE_CARDS,

  // The failure bubble when credit card uploading failed.
  FAILURE,

  // There is no bubble to show anymore. This also
  // indicates that the icon should not be visible.
  INACTIVE
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_
