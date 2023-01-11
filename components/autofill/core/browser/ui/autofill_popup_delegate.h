// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {
class PasswordManagerDriver;
}

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

  // Called when the autofill suggestion indicated by |frontend_id| has been
  // temporarily selected (e.g., hovered).
  // |value| is the suggestion's value, and is usually the main text to be
  // shown. |frontend_id| is the frontend id of the suggestion. |backend_id| is
  // the guid of the backend data model.
  virtual void DidSelectSuggestion(const std::u16string& value,
                                   int frontend_id,
                                   const Suggestion::BackendId& backend_id) = 0;

  // Informs the delegate that a row in the popup has been chosen. |suggestion|
  // is the suggestion that was chosen in the popup. |position| refers to the
  // index of the suggestion in the suggestion list.
  virtual void DidAcceptSuggestion(const Suggestion& suggestion,
                                   int position) = 0;

  // Returns whether the given value can be deleted, and if true,
  // fills out |title| and |body|.
  virtual bool GetDeletionConfirmationText(const std::u16string& value,
                                           int frontend_id,
                                           std::u16string* title,
                                           std::u16string* body) = 0;

  // Delete the described suggestion. Returns true if something was deleted,
  // or false if deletion is not allowed.
  virtual bool RemoveSuggestion(const std::u16string& value,
                                int frontend_id) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;

  // Returns the type of the popup being shown.
  virtual PopupType GetPopupType() const = 0;

  // Returns the associated AutofillDriver.
  virtual absl::variant<AutofillDriver*,
                        password_manager::PasswordManagerDriver*>
  GetDriver() = 0;

  // Returns the ax node id associated with the current web contents' element
  // who has a controller relation to the current autofill popup.
  virtual int32_t GetWebContentsPopupControllerAxId() const = 0;

  // Sets |deletion_callback| to be called from the delegate's destructor.
  // Useful for deleting objects which cannot be owned by the delegate but
  // should not outlive it.
  virtual void RegisterDeletionCallback(
      base::OnceClosure deletion_callback) = 0;

  virtual ~AutofillPopupDelegate() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_
