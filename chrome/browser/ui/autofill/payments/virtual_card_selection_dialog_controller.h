// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

namespace autofill {

class CreditCard;

// An interface that exposes necessary controller functionality to
// VirtualCardSelectionDialogView.
class VirtualCardSelectionDialogController {
 public:
  virtual ~VirtualCardSelectionDialogController() = default;

  virtual bool IsOkButtonEnabled() = 0;

  virtual std::u16string GetContentTitle() const = 0;
  virtual std::u16string GetContentExplanation() const = 0;
  virtual std::u16string GetOkButtonLabel() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;

  virtual const std::vector<CreditCard*>& GetCardList() const = 0;

  virtual void OnCardSelected(const std::string& selected_card_id) = 0;
  virtual void OnOkButtonClicked() = 0;
  virtual void OnCancelButtonClicked() = 0;
  virtual void OnDialogClosed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_CONTROLLER_H_
