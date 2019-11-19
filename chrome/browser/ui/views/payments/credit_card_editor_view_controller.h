// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/styled_label_listener.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// Credit card editor screen of the Payment Request flow.
class CreditCardEditorViewController : public EditorViewController,
                                       public views::StyledLabelListener {
 public:
  // Does not take ownership of the arguments (except for the |on_edited| and
  // |on_added| callbacks), which should outlive this object. Additionally,
  // |credit_card| could be nullptr if we are adding a card. Else, it's a valid
  // pointer to a card that needs to be updated, and which will outlive this
  // controller.
  CreditCardEditorViewController(
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      BackNavigationType back_navigation,
      int next_ui_tag,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::CreditCard&)> on_added,
      autofill::CreditCard* credit_card,
      bool is_incognito);
  ~CreditCardEditorViewController() override;

  // EditorViewController:
  std::unique_ptr<views::View> CreateHeaderView() override;
  std::unique_ptr<views::View> CreateCustomFieldView(
      autofill::ServerFieldType type,
      views::View** focusable_field,
      bool* valid,
      base::string16* error_message) override;
  std::unique_ptr<views::View> CreateExtraViewForField(
      autofill::ServerFieldType type) override;
  bool IsEditingExistingItem() override;
  std::vector<EditorField> GetFieldDefinitions() override;
  base::string16 GetInitialValueForType(
      autofill::ServerFieldType type) override;
  bool ValidateModelAndSave() override;
  std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) override;
  std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::ServerFieldType& type) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Selects the icon in the UI corresponding to |basic_card_network| with
  // higher opacity. If empty string, selects none of them (all full opacity).
  void SelectBasicCardNetworkIcon(const std::string& basic_card_network);

  // Exposed for validation delegate.
  bool IsValidCreditCardNumber(const base::string16& card_number,
                               base::string16* error_message);

 protected:
  // PaymentRequestSheetController:
  void FillContentView(views::View* content_view) override;
  base::string16 GetSheetTitle() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  class CreditCardValidationDelegate : public ValidationDelegate {
   public:
    // Used to validate |field| type. A reference to the |controller| should
    // outlive this delegate.
    CreditCardValidationDelegate(const EditorField& field,
                                 CreditCardEditorViewController* controller);
    ~CreditCardValidationDelegate() override;

    // ValidationDelegate:
    bool ShouldFormat() override;
    base::string16 Format(const base::string16& text) override;
    bool IsValidTextfield(views::Textfield* textfield,
                          base::string16* error_message) override;
    bool IsValidCombobox(views::Combobox* combobox,
                         base::string16* error_message) override;
    bool TextfieldValueChanged(views::Textfield* textfield,
                               bool was_blurred) override;
    bool ComboboxValueChanged(views::Combobox* combobox) override;
    void ComboboxModelChanged(views::Combobox* combobox) override {}

   private:
    // Validates a specific |value|/|combobox|.
    bool ValidateValue(const base::string16& value,
                       base::string16* error_message);
    bool ValidateCombobox(views::Combobox* combobox,
                          base::string16* error_message);

    EditorField field_;
    // Outlives this class.
    CreditCardEditorViewController* controller_;

    DISALLOW_COPY_AND_ASSIGN(CreditCardValidationDelegate);
  };

  bool GetSheetId(DialogViewID* sheet_id) override;

  // Called when a new address was created to be used as the billing address.
  // The lifespan of |profile| beyond this call is undefined but it's OK, it's
  // simply propagated to the address combobox model.
  void AddAndSelectNewBillingAddress(const autofill::AutofillProfile& profile);

  // Whether the editor is editing a server card (masked).
  bool IsEditingServerCard() const;

  // Called when |credit_card_to_edit_| was successfully edited.
  base::OnceClosure on_edited_;
  // Called when a new card was added. The const reference is short-lived, and
  // the callee should make a copy.
  base::OnceCallback<void(const autofill::CreditCard&)> on_added_;

  // If non-nullptr, a pointer to an object to be edited. Must outlive this
  // controller.
  autofill::CreditCard* credit_card_to_edit_;

  // Keeps track of the card icons currently visible, keyed by basic card
  // network.
  std::map<std::string, views::View*> card_icons_;

  // The value to use for the add billing address button tag.
  int add_billing_address_button_tag_;

  // The list of supported basic card networks.
  std::set<std::string> supported_card_networks_;

  base::WeakPtrFactory<CreditCardEditorViewController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardEditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_
