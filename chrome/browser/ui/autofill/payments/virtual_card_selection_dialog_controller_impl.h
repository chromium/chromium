// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class VirtualCardSelectionDialog;

// Implementation of the per-tab controller to control the
// VirtualCardSelectionDialog. Lazily initialized when used.
class VirtualCardSelectionDialogControllerImpl
    : public VirtualCardSelectionDialogController,
      public content::WebContentsUserData<
          VirtualCardSelectionDialogControllerImpl> {
 public:
  VirtualCardSelectionDialogControllerImpl(
      const VirtualCardSelectionDialogControllerImpl&) = delete;
  VirtualCardSelectionDialogControllerImpl& operator=(
      const VirtualCardSelectionDialogControllerImpl&) = delete;
  ~VirtualCardSelectionDialogControllerImpl() override;

  void ShowDialog(const std::vector<CreditCard*>& candidates,
                  base::OnceCallback<void(const std::string&)> callback);

  // VirtualCardSelectionDialogController:
  bool IsOkButtonEnabled() override;
  std::u16string GetContentTitle() const override;
  std::u16string GetContentExplanation() const override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  const std::vector<CreditCard*>& GetCardList() const override;
  void OnCardSelected(const std::string& selected_card_id) override;
  void OnOkButtonClicked() override;
  void OnCancelButtonClicked() override;
  void OnDialogClosed() override;

  VirtualCardSelectionDialog* dialog() { return dialog_; }

 protected:
  explicit VirtualCardSelectionDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      VirtualCardSelectionDialogControllerImpl>;

  // Local copy of all the candidate cards.
  std::vector<CreditCard*> candidates_;

  // The identifier of the selected card in the list. When there is more than
  // one card, no card is set to be selected by default.
  std::string selected_card_id_;

  // Callback invoked when a card from the |candidates_| is selected and dialog
  // is accepted. Will pass the |selected_card_id_| as the param.
  base::OnceCallback<void(const std::string&)> callback_;

  raw_ptr<VirtualCardSelectionDialog> dialog_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_CONTROLLER_IMPL_H_
