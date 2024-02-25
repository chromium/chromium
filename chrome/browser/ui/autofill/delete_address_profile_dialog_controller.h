// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_H_

#include <string>

namespace autofill {

// Interface that exposes controller functionality to DeleteAddressProfileView
// dialog.
class DeleteAddressProfileDialogController {
 public:
  virtual ~DeleteAddressProfileDialogController() = default;

  virtual std::u16string GetTitle() const = 0;
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetDeclineButtonText() const = 0;
  virtual std::u16string GetDeleteConfirmationText() const = 0;

  virtual void OnAccepted() = 0;
  virtual void OnCanceled() = 0;
  virtual void OnClosed() = 0;
  // Called when the dialog is destroyed, regardless of the reason, see
  // ui/base/models/dialog_model.h.
  virtual void OnDialogDestroying() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_H_
