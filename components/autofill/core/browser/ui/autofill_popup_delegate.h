// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/popup_types.h"

namespace autofill {

class AutofillDriver;

// An interface for interaction with AutofillPopupController. Will be notified
// of events by the controller.
class AutofillPopupDelegate {
 public:
  // Called when the Autofill popup is shown.
  virtual void OnPopupShown() = 0;

  // Called when the Autofill popup is hidden.
  virtual void OnPopupHidden() = 0;

  virtual void OnPopupSuppressed() = 0;

  // Called when the autofill suggestion indicated by |identifier| has been
  // temporarily selected (e.g., hovered).
  virtual void DidSelectSuggestion(const base::string16& value,
                                   int identifier) = 0;

  // Inform the delegate that a row in the popup has been chosen.
  virtual void DidAcceptSuggestion(const base::string16& value,
                                   int identifier,
                                   int position) = 0;

  // Returns whether the given value can be deleted, and if true,
  // fills out |title| and |body|.
  virtual bool GetDeletionConfirmationText(const base::string16& value,
                                           int identifier,
                                           base::string16* title,
                                           base::string16* body) = 0;

  // Delete the described suggestion. Returns true if something was deleted,
  // or false if deletion is not allowed.
  virtual bool RemoveSuggestion(const base::string16& value,
                                int identifier) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;

  // Returns the type of the popup being shown.
  virtual PopupType GetPopupType() const = 0;

  // Returns the associated AutofillDriver.
  virtual AutofillDriver* GetAutofillDriver() = 0;

  // Returns the ax node id associated with the current web contents' element
  // who has a controller relation to the current autofill popup.
  virtual int32_t GetWebContentsPopupControllerAxId() const = 0;

  // Sets |deletion_callback| to be called from the delegate's destructor.
  // Useful for deleting objects which cannot be owned by the delegate but
  // should not outlive it.
  virtual void RegisterDeletionCallback(
      base::OnceClosure deletion_callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_
