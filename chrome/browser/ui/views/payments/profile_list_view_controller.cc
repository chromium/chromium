// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/profile_list_view_controller.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

namespace payments {

namespace {

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
              base::WeakPtr<PaymentRequestSpec> spec,
              base::WeakPtr<PaymentRequestState> state,
              PaymentRequestItemList* parent_list,
              base::WeakPtr<ProfileListViewController> controller,
              base::WeakPtr<PaymentRequestDialogView> dialog,
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

  ProfileItem(const ProfileItem&) = delete;
  ProfileItem& operator=(const ProfileItem&) = delete;

  ~ProfileItem() override {}

  base::WeakPtr<PaymentRequestRowView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateContentView(
      std::u16string* accessible_content) override {
    DCHECK(profile_);
    DCHECK(accessible_content);

    return controller_->GetLabel(profile_, accessible_content);
  }

  void SelectedStateChanged() override {
    if (selected()) {
      controller_->SelectProfile(profile_);
    }
  }

  std::u16string GetNameForDataType() override {
    return controller_->GetSheetTitle();
  }

  bool CanBeSelected() override {
    // In order to be selectable, a profile entry needs to be enabled, and the
    // profile valid according to the controller. If either condition is false,
    // PerformSelectionFallback() is called.
    return GetClickable() && controller_->IsValidProfile(*profile_);
  }

  void PerformSelectionFallback() override {
    // If enabled, the editor is opened to complete the invalid profile.
    if (GetClickable())
      controller_->ShowEditor(profile_);
  }

  void EditButtonPressed() override { controller_->ShowEditor(profile_); }

  base::WeakPtr<ProfileListViewController> controller_;
  raw_ptr<autofill::AutofillProfile> profile_;
  base::WeakPtrFactory<ProfileItem> weak_ptr_factory_{this};
};

// The ProfileListViewController subtype for the Shipping address list
// screen of the Payment Request flow.
class ShippingProfileViewController : public ProfileListViewController,
                                      public PaymentRequestSpec::Observer {
 public:
  // The `spec` parameter should not be null
  ShippingProfileViewController(base::WeakPtr<PaymentRequestSpec> spec,
                                base::WeakPtr<PaymentRequestState> state,
                                base::WeakPtr<PaymentRequestDialogView> dialog)
      : ProfileListViewController(spec, state, dialog) {
    DCHECK(spec);
    spec->AddObserver(this);
    PopulateList();
  }

  ShippingProfileViewController(const ShippingProfileViewController&) = delete;
  ShippingProfileViewController& operator=(
      const ShippingProfileViewController&) = delete;

  ~ShippingProfileViewController() override {
    if (spec())
      spec()->RemoveObserver(this);
  }

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile,
      std::u16string* accessible_content) override {
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
    state()->SetSelectedShippingProfile(profile);
  }

  void ShowEditor(autofill::AutofillProfile* profile) override {
    dialog()->ShowShippingAddressEditor(
        BackNavigationType::kPaymentSheet,
        /*on_edited=*/
        base::BindOnce(&PaymentRequestState::SetSelectedShippingProfile,
                       state(), profile),
        /*on_added=*/
        base::BindOnce(&PaymentRequestState::AddAutofillShippingProfile,
                       state(), /*selected=*/true),
        profile);
  }

  autofill::AutofillProfile* GetSelectedProfile() override {
    return state()->selected_shipping_profile();
  }

  bool IsValidProfile(const autofill::AutofillProfile& profile) override {
    return state()->profile_comparator()->IsShippingComplete(&profile);
  }

  std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>
  GetProfiles() override {
    return state()->shipping_profiles();
  }

  DialogViewID GetDialogViewId() override {
    return DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW;
  }

  std::unique_ptr<views::View> CreateHeaderView() override {
    if (!spec() || (!spec()->GetShippingOptions().empty() &&
                    spec()->selected_shipping_option_error().empty())) {
      return nullptr;
    }

    return CreateWarningView(
        spec()->selected_shipping_option_error().empty()
            ? GetShippingAddressSelectorInfoMessage(spec()->shipping_type())
            : spec()->selected_shipping_option_error(),
        !spec()->selected_shipping_option_error().empty());
  }

  std::u16string GetSheetTitle() override {
    return spec() ? GetShippingAddressSectionString(spec()->shipping_type())
                  : std::u16string();
  }

  std::u16string GetSecondaryButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_ADDRESS);
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
    if (!spec())
      return;

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

  base::WeakPtrFactory<ShippingProfileViewController> weak_ptr_factory_{this};
};

class ContactProfileViewController : public ProfileListViewController {
 public:
  // The `spec` parameter should not be null.
  ContactProfileViewController(base::WeakPtr<PaymentRequestSpec> spec,
                               base::WeakPtr<PaymentRequestState> state,
                               base::WeakPtr<PaymentRequestDialogView> dialog)
      : ProfileListViewController(spec, state, dialog) {
    DCHECK(spec);
    PopulateList();
  }

  ContactProfileViewController(const ContactProfileViewController&) = delete;
  ContactProfileViewController& operator=(const ContactProfileViewController&) =
      delete;

  ~ContactProfileViewController() override {}

 protected:
  // ProfileListViewController:
  std::unique_ptr<views::View> GetLabel(
      autofill::AutofillProfile* profile,
      std::u16string* accessible_content) override {
    DCHECK(profile);
    return GetContactInfoLabel(
        AddressStyleType::DETAILED, state()->GetApplicationLocale(), *profile,
        /*request_payer_name=*/spec() && spec()->request_payer_name(),
        /*request_payer_email=*/spec() && spec()->request_payer_email(),
        /*request_payer_phone=*/spec() && spec()->request_payer_phone(),
        *(state()->profile_comparator()), accessible_content);
  }

  void SelectProfile(autofill::AutofillProfile* profile) override {
    state()->SetSelectedContactProfile(profile);
    dialog()->GoBack();
  }

  void ShowEditor(autofill::AutofillProfile* profile) override {
    dialog()->ShowContactInfoEditor(
        BackNavigationType::kPaymentSheet,
        /*on_edited=*/
        base::BindOnce(&PaymentRequestState::SetSelectedContactProfile, state(),
                       profile),
        /*on_added=*/
        base::BindOnce(&PaymentRequestState::AddAutofillContactProfile, state(),
                       /*selected=*/true),
        profile);
  }

  autofill::AutofillProfile* GetSelectedProfile() override {
    return state()->selected_contact_profile();
  }

  bool IsValidProfile(const autofill::AutofillProfile& profile) override {
    return state()->profile_comparator()->IsContactInfoComplete(&profile);
  }

  std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>
  GetProfiles() override {
    return state()->contact_profiles();
  }

  DialogViewID GetDialogViewId() override {
    return DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW;
  }

  std::u16string GetSheetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME);
  }

  std::u16string GetSecondaryButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_CONTACT);
  }

  int GetSecondaryButtonId() override {
    return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CONTACT_BUTTON);
  }
};

}  // namespace

// static
std::unique_ptr<ProfileListViewController>
ProfileListViewController::GetShippingProfileViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog) {
  return std::make_unique<ShippingProfileViewController>(spec, state, dialog);
}

// static
std::unique_ptr<ProfileListViewController>
ProfileListViewController::GetContactProfileViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog) {
  return std::make_unique<ContactProfileViewController>(spec, state, dialog);
}

ProfileListViewController::ProfileListViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog), list_(dialog) {}

ProfileListViewController::~ProfileListViewController() {}

bool ProfileListViewController::IsEnabled(autofill::AutofillProfile* profile) {
  return true;
}

std::unique_ptr<views::View> ProfileListViewController::CreateHeaderView() {
  return nullptr;
}

void ProfileListViewController::PopulateList() {
  if (!spec())
    return;

  autofill::AutofillProfile* selected_profile = GetSelectedProfile();

  list_.Clear();

  for (autofill::AutofillProfile* profile : GetProfiles()) {
    list_.AddItem(std::make_unique<ProfileItem>(
        profile, spec(), state(), &list_, weak_ptr_factory_.GetWeakPtr(),
        dialog(), profile == selected_profile, IsEnabled(profile)));
  }
}

bool ProfileListViewController::ShouldShowPrimaryButton() {
  return false;
}

PaymentRequestSheetController::ButtonCallback
ProfileListViewController::GetSecondaryButtonCallback() {
  return base::BindRepeating(
      &ProfileListViewController::OnCreateNewProfileButtonClicked,
      base::Unretained(this));
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

base::WeakPtr<PaymentRequestSheetController>
ProfileListViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProfileListViewController::OnCreateNewProfileButtonClicked(
    const ui::Event& event) {
  // nullptr means 'create a new profile'
  ShowEditor(nullptr);
}

}  // namespace payments
