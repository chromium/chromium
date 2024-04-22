// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace autofill {

class AutofillKeyboardAccessoryController;

// The interface that the native view for the Keyboard Accessory implements.
class AutofillKeyboardAccessoryView {
 public:
  // Creates an `AutofillKeyboardAccessoryView`. Returns `nullptr` if the
  // initialization is unsuccessful.
  static std::unique_ptr<AutofillKeyboardAccessoryView> Create(
      base::WeakPtr<AutofillKeyboardAccessoryController> controller);

  virtual ~AutofillKeyboardAccessoryView() = default;

  // Initializes the Java-side of this bridge. Returns true after a successful
  // creation and false otherwise.
  virtual bool Initialize() = 0;

  // Requests to dismiss this view.
  virtual void Hide() = 0;

  // Requests to show this view with the data provided by the controller.
  virtual void Show() = 0;

  // Makes announcement for acessibility.
  virtual void AxAnnounce(const std::u16string& text);

  // Ask to confirm a deletion. Triggers the callback upon the user confirming
  // or declining the deletion. The detection callback parameter specifies
  // whether the deletion was confirmed or declined.
  virtual void ConfirmDeletion(
      const std::u16string& confirmation_title,
      const std::u16string& confirmation_body,
      base::OnceCallback<void(bool)> deletion_callback) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
