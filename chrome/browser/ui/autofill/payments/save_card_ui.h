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

  // Credit card upload is in progress.
  UPLOAD_IN_PROGRESS,

  // Credit card upload is completed.
  UPLOAD_COMPLETED,

  // The manage cards bubble when bubble is reshown after
  // icon is clicked.
  MANAGE_CARDS,

  // There is no bubble to show anymore. This also
  // indicates that the icon should not be visible.
  INACTIVE
};

// The active treatment arm of the AutofillUpstreamUpdatedUi feature.
enum class UpdatedDesktopUiTreatmentArm {
  // Experiment not active.
  kDefault,
  // Security-focused messaging and imagery.
  kSecurityFocus,
  // Convenience-focused messaging and imagery.
  kConvenienceFocus,
  // Education-focused messaging and imagery.
  kEducationFocus
};

// Returns the active UpdatedDesktopUiTreatmentArm based on the user's current
// Finch configuration.
UpdatedDesktopUiTreatmentArm GetUpdatedDesktopUiTreatmentArm();

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_UI_H_
