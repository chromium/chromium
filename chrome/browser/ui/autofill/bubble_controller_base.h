// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

enum class BubbleType {
  // Denotes the save/update address bubble.
  kSaveUpdateAddress,
  // Denotes bubble for saving a new IBAN.
  kSaveIban,
  // Denotes bubble for saving/updating a credit card.
  kSaveUpdateCard,
  // Denotes bubble for saving/updating autofill ai data.
  kSaveUpdateAutofillAi,
  // Denotes bubble for virtual card enrollment confirmation.
  kVirtualCardEnrollConfirmation,
  // Denotes bubble for mandatory reauth types.
  kMandatoryReauth,
  // Denotes bubble for offer notifications.
  kOfferNotification,
  // Denotes bubble for filled card information.
  kFilledCardInformation,
  // Denotes password related bubbles.
  kPassword
};

// This class serves as the base for all bubble controllers, which manage the
// logic and state of an Autofill bubble.
class BubbleControllerBase {
 public:
  virtual ~BubbleControllerBase() = default;

  // Instructs the controller to show the bubble view.
  virtual void ShowBubble() = 0;

  // Instructs the controller to hide the bubble view.
  virtual void HideBubble() = 0;

  // Returns the corresponding `BubbleType` for the controller.
  virtual BubbleType GetBubbleType() const = 0;

  // Returns true if the bubble is currently visible.
  virtual bool IsShowingBubble() const = 0;

  // Returns true if the mouse is currently inside the bubble view.
  virtual bool IsMouseHovered() const = 0;

  // Subclasses need to implement this method so that the resulting weak
  // pointers are invalidated as soon as the derived class is destroyed.
  virtual base::WeakPtr<BubbleControllerBase>
  GetBubbleControllerBaseWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
