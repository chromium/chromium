// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(BubbleType)
enum class BubbleType {
  // Denotes the save/update address bubble.
  kSaveUpdateAddress = 0,
  // Denotes bubble for saving a new IBAN.
  kSaveIban = 1,
  // Denotes bubble for saving/updating a credit card.
  kSaveUpdateCard = 2,
  // Denotes bubble for saving/updating autofill ai data.
  kSaveUpdateAutofillAi = 3,
  // Denotes bubble for virtual card enrollment confirmation.
  kVirtualCardEnrollConfirmation = 4,
  // Denotes bubble for mandatory reauth types.
  kMandatoryReauth = 5,
  // Denotes bubble for offer notifications.
  kOfferNotification = 6,
  // Denotes bubble for filled card information.
  kFilledCardInformation = 7,
  // Denotes password related bubbles.
  kPassword = 8,
  // Denotes bubble for walletable pass detection consent.
  kWalletablePassConsent = 9,
  // Denotes bubble for walletable pass save.
  kWalletablePassSave = 10,
  kMaxValue = kWalletablePassSave
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillBubbleType)

// This class serves as the base for all bubble controllers, which manage the
// logic and state of an Autofill bubble.
class BubbleControllerBase {
 public:
  virtual ~BubbleControllerBase() = default;

  // Instructs the controller to show the bubble view.
  virtual void ShowBubble() = 0;

  // Instructs the controller to hide the bubble view.
  // TODO(crbug.com/432429605): `initiated_by_bubble_manager` can be removed,
  // instead use BubbleManager's `HasPendingBubbleOfSameType` to determine if
  // the bubble is still alive.
  virtual void HideBubble(bool initiated_by_bubble_manager) = 0;

  // Instructs the controller that its pending request to show has been
  // discarded and will not be shown. This can happen on timeout or teardown.
  virtual void OnBubbleDiscarded() = 0;

  // Returns the corresponding `BubbleType` for the controller.
  virtual BubbleType GetBubbleType() const = 0;

  // Returns true if the bubble is currently visible.
  virtual bool IsShowingBubble() const = 0;

  // Returns true if the mouse is currently inside the bubble view.
  virtual bool IsMouseHovered() const = 0;

  // Returns false if the bubble should not be queued and shown again later
  // (e.g. after being preempted). This is the case for bubbles that are
  // time-sensitive or whose state is cleared upon closing.
  virtual bool CanBeReshown() const = 0;

  // Subclasses need to implement this method so that the resulting weak
  // pointers are invalidated as soon as the derived class is destroyed.
  virtual base::WeakPtr<BubbleControllerBase>
  GetBubbleControllerBaseWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
