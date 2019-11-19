// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/profile_list_view_controller.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

namespace {

enum class ProfileListViewControllerTags : int {
  // The tag for the button that triggers the "add address"
  // flow. Starts at |PAYMENT_REQUEST_COMMON_TAG_MAX| not to conflict
  // with tags common to all views.
  ADD_SHIPPING_ADDRESS_BUTTON = static_cast<int>(
      PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX),
  ADD_CONTACT_BUTTON,
};

class ProfileItem : public PaymentRequestItemList::Item {
 public:
  // Constructs an object owned by |parent_list|, representing one element in
  // the list. |spec| and |state| are the PaymentRequestSpec/State objects that
  // are represented by the current instance of the dialog. |parent_view| points
  // to the controller which owns |parent_list|. |profile| is the
  // AutofillProfile that this specific list item represents. It's a cached
  // profile owned by |state|. |clickable| indicates whether or not this profile
  // can be clicked (i.e., whether it's enabled).
  ProfileItem(autofill::AutofillProfile* profile,
              PaymentRequestSpec* spec,
              PaymentRequestState* state,
              PaymentRequestItemList* parent_list,
              ProfileListViewController* controller,
              PaymentRequestDialogView* dialog,
              bool selected,
              bool clickable)
      : PaymentRequestItemList::Item(spec,
                                     state,
                                     parent_list,
                                     selected,
                                     clickable,
                                     /*show_edit_button=*/true),
        controller_(controller),
        profile_(profile) {
    Init();
  }
  ~ProfileItem() override {}

 private:
  // PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateContentView(
      base::string16* accessible_content) override {
    DCHECK(profile_);
    DCHECK(accessible_content);

    return controller_->GetLabel(profile_, accessible_content);
  }

  void SelectedStateChanged() override {
    if (selected()) {
      controller_->SelectProfile(profile_);
    }
  }

  base::string16 GetNameForDataType() override {
    return controller_->GetSheetTitle();
  }

  bool CanBeSelected() override {
    // In order to be selectable, a profile entry needs to be enabled, and the
    // profile valid according to the controller. If either condition is false,
    // PerformSelectionFallback() is called.
    return clickable() && controller_->IsValidProfile(*profile_);
  }

  void PerformSelectionFallback() override {
    // If enabled, the editor is opened to complete the invalid profile.
    if (clickable())
      controller_->ShowEditor(profile_);
  }

  void EditButtonPressed() override { controller_->ShowEditor(profile_); }

  ProfileListViewController* controller_;
  autofill::AutofillProfile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileItem);
};

// The ProfileListViewController subtype for the Shipping address list
// screen of the Payment Request flow.
class ShippingProfileViewController : public ProfileListViewController,
                                      public PaymentRequestSpec::Observer {
 public:
  ShippingProfileViewController(PaymentRequestSpec* spec,
                                PaymentRequestState* state,
                                PaymentRequestDialogView* dialog)
      : ProfileListViewController(spec, state, dialog) {
    spec->AddObserver(this);
    PopulateList();
  }
  ~ShippingProfileViewController() override { spec()->RemoveObserver(this); }

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile,
      base::string16* accessible_content) override {
    return GetShippingAddressLabelWithMissingInfo(
        AddressStyleType::DETAILED, state()->GetApplicationLocale(), *profile,
        *(state()->profile_comparator()), accessible_content,
        /*enabled=*/IsEnabled(profile));
  }

  void SelectProfile(autofill::AutofillProfile* profile) override {
    // This will trigger a merchant update as well as a full spinner on top
    // of the profile list. When the spec comes back updated (in OnSpecUpdated),
    // the decision will be made to either stay on this screen or go back to the
    // payment sheet.
    state()->SetSelectedShippingProfile(
        profile, PaymentRequestState::SectionSelectionStatus::kSelected);
  }

  void ShowEditor(autofill::AutofillProfile* profile) override {
    dialog()->ShowShippingAddressEditor(
        BackNavigationType::kPaymentSheet,
        /*on_edited=*/
        base::BindOnce(
            &PaymentRequestState::SetSelectedShippingProfile,
            state()->AsWeakPtr(), profile,
            PaymentRequestState::SectionSelectionStatus::kEditedSelected),
        /*on_added=*/
        base::BindOnce(&PaymentRequestState::AddAutofillShippingProfile,
                       state()->AsWeakPtr(), /*selected=*/true),
        profile);
  }

  autofill::AutofillProfile* GetSelectedProfile() override {
    return state()->selected_shipping_profile();
  }

  bool IsValidProfile(const autofill::AutofillProfile& profile) override {
    return state()->profile_comparator()->IsShippingComplete(&profile);
  }

  std::vector<autofill::AutofillProfile*> GetProfiles() override {
    return state()->shipping_profiles();
  }

  DialogViewID GetDialogViewId() override {
    return DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW;
  }

  std::unique_ptr<views::View> CreateHeaderView() override {
    if (!spec()->GetShippingOptions().empty() &&
        spec()->selected_shipping_option_error().empty()) {
      return nullptr;
    }

    return CreateWarningView(
        spec()->selected_shipping_option_error().empty()
            ? GetShippingAddressSelectorInfoMessage(spec()->shipping_type())
            : spec()->selected_shipping_option_error(),
        !spec()->selected_shipping_option_error().empty());
  }

  base::string16 GetSheetTitle() override {
    return GetShippingAddressSectionString(spec()->shipping_type());
  }

  base::string16 GetSecondaryButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_ADDRESS);
  }

  int GetSecondaryButtonTag() override {
    return static_cast<int>(
        ProfileListViewControllerTags::ADD_SHIPPING_ADDRESS_BUTTON);
  }

  int GetSecondaryButtonId() override {
    return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_SHIPPING_BUTTON);
  }

  bool IsEnabled(autofill::AutofillProfile* profile) override {
    // If selected_shipping_option_error_profile() is null, then no error is
    // reported by the merchant and all items are enabled. If it is not null and
    // equal to |profile|, then |profile| should be disabled.
    return !state()->selected_shipping_option_error_profile() ||
           profile != state()->selected_shipping_option_error_profile();
  }

 private:
  void OnSpecUpdated() override {
    // If there's an error, stay on this screen so the user can select a
    // different address. Otherwise, go back to the payment sheet.
    if (spec()->current_update_reason() ==
        PaymentRequestSpec::UpdateReason::SHIPPING_ADDRESS) {
      if (!state()->selected_shipping_option_error_profile()) {
        dialog()->GoBack();
      } else {
        // The error profile is known, refresh the view to display it correctly.
        PopulateList();
        UpdateContentView();
        if (spec()->has_shipping_address_error())
          ShowEditor(state()->selected_shipping_option_error_profile());
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ShippingProfileViewController);
};

class ContactProfileViewController : public ProfileListViewController {
 public:
  ContactProfileViewController(PaymentRequestSpec* spec,
                               PaymentRequestState* state,
                               PaymentRequestDialogView* dialog)
      : ProfileListViewController(spec, state, dialog) {
    PopulateList();
  }
  ~ContactProfileViewController() override {}

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile,
      base::string16* accessible_content) override {
    return GetContactInfoLabel(
        AddressStyleType::DETAILED, state()->GetApplicationLocale(), *profile,
        *spec(), *(state()->profile_comparator()), accessible_content);
  }

  void SelectProfile(autofill::AutofillProfile* profile) override {
    state()->SetSelectedContactProfile(
        profile, PaymentRequestState::SectionSelectionStatus::kSelected);
    dialog()->GoBack();
  }

  void ShowEditor(autofill::AutofillProfile* profile) override {
    dialog()->ShowContactInfoEditor(
        BackNavigationType::kPaymentSheet,
        /*on_edited=*/
        base::BindOnce(
            &PaymentRequestState::SetSelectedContactProfile,
            state()->AsWeakPtr(), profile,
            PaymentRequestState::SectionSelectionStatus::kEditedSelected),
        /*on_added=*/
        base::BindOnce(&PaymentRequestState::AddAutofillContactProfile,
                       state()->AsWeakPtr(), /*selected=*/true),
        profile);
  }

  autofill::AutofillProfile* GetSelectedProfile() override {
    return state()->selected_contact_profile();
  }

  bool IsValidProfile(const autofill::AutofillProfile& profile) override {
    return state()->profile_comparator()->IsContactInfoComplete(&profile);
  }

  std::vector<autofill::AutofillProfile*> GetProfiles() override {
    return state()->contact_profiles();
  }

  DialogViewID GetDialogViewId() override {
    return DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW;
  }

  base::string16 GetSheetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME);
  }

  base::string16 GetSecondaryButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_CONTACT);
  }

  int GetSecondaryButtonTag() override {
    return static_cast<int>(ProfileListViewControllerTags::ADD_CONTACT_BUTTON);
  }

  int GetSecondaryButtonId() override {
    return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CONTACT_BUTTON);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactProfileViewController);
};

}  // namespace

// static
std::unique_ptr<ProfileListViewController>
ProfileListViewController::GetShippingProfileViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog) {
  return std::make_unique<ShippingProfileViewController>(spec, state, dialog);
}

// static
std::unique_ptr<ProfileListViewController>
ProfileListViewController::GetContactProfileViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog) {
  return std::make_unique<ContactProfileViewController>(spec, state, dialog);
}

ProfileListViewController::ProfileListViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog), list_(dialog) {}

ProfileListViewController::~ProfileListViewController() {}

bool ProfileListViewController::IsEnabled(autofill::AutofillProfile* profile) {
  return true;
}

std::unique_ptr<views::View> ProfileListViewController::CreateHeaderView() {
  return nullptr;
}

void ProfileListViewController::PopulateList() {
  autofill::AutofillProfile* selected_profile = GetSelectedProfile();

  list_.Clear();

  for (auto* profile : GetProfiles()) {
    list_.AddItem(std::make_unique<ProfileItem>(
        profile, spec(), state(), &list_, this, dialog(),
        profile == selected_profile, IsEnabled(profile)));
  }
}

void ProfileListViewController::FillContentView(views::View* content_view) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  content_view->SetLayoutManager(std::move(layout));
  std::unique_ptr<views::View> header_view = CreateHeaderView();
  if (header_view)
    content_view->AddChildView(header_view.release());
  std::unique_ptr<views::View> list_view = list_.CreateListView();
  list_view->SetID(static_cast<int>(GetDialogViewId()));
  content_view->AddChildView(list_view.release());
}

void ProfileListViewController::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender->tag() == GetSecondaryButtonTag())
    ShowEditor(nullptr);
  else
    PaymentRequestSheetController::ButtonPressed(sender, event);
}

}  // namespace payments
