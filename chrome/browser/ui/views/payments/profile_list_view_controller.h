// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PROFILE_LIST_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PROFILE_LIST_VIEW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_item_list.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

namespace autofill {
class AutofillProfile;
}

namespace views {
class Button;
class View;
}

namespace payments {

enum class DialogViewID;
class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// This base class encapsulates common view logic for contexts which display
// a list of profiles and allow exactly one of them to be selected.
class ProfileListViewController : public PaymentRequestSheetController {
 public:
  ~ProfileListViewController() override;

  // Creates a controller which lists and allows selection of profiles
  // for shipping address.
  static std::unique_ptr<ProfileListViewController>
  GetShippingProfileViewController(PaymentRequestSpec* spec,
                                   PaymentRequestState* state,
                                   PaymentRequestDialogView* dialog);

  // Creates a controller which lists and allows selection of profiles
  // for contact info.
  static std::unique_ptr<ProfileListViewController>
  GetContactProfileViewController(PaymentRequestSpec* spec,
                                  PaymentRequestState* state,
                                  PaymentRequestDialogView* dialog);

  // PaymentRequestSheetController:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Returns a representation of the given profile appropriate for display
  // in this context. Populates |accessible_string|, which shouldn't be null,
  // with the screen reader string representing the returned label.
  virtual std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile,
      base::string16* accessible_string) = 0;

  virtual void SelectProfile(autofill::AutofillProfile* profile) = 0;

  // Shows an editor for modifying |profile|, or for creating a new profile
  // if |profile| is null.
  virtual void ShowEditor(autofill::AutofillProfile* profile) = 0;

  virtual autofill::AutofillProfile* GetSelectedProfile() = 0;

  virtual bool IsValidProfile(const autofill::AutofillProfile& profile) = 0;

  // Whether |profile| should be displayed in an enabled state and selectable.
  virtual bool IsEnabled(autofill::AutofillProfile* profile);

 protected:
  // Does not take ownership of the arguments, which should outlive this object.
  ProfileListViewController(PaymentRequestSpec* spec,
                            PaymentRequestState* state,
                            PaymentRequestDialogView* dialog);

  // Returns the profiles cached by |request| which are appropriate for display
  // in this context.
  virtual std::vector<autofill::AutofillProfile*> GetProfiles() = 0;

  virtual DialogViewID GetDialogViewId() = 0;

  // Subclasses may choose to provide a header view to go on top of the item
  // list view.
  virtual std::unique_ptr<views::View> CreateHeaderView();

  void PopulateList();

  // PaymentRequestSheetController:
  void FillContentView(views::View* content_view) override;

 private:
  std::unique_ptr<views::Button> CreateRow(autofill::AutofillProfile* profile);
  PaymentRequestItemList list_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PROFILE_LIST_VIEW_CONTROLLER_H_
