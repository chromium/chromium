// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_option_view_controller.h"

#include <memory>

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace payments {

namespace {

class ShippingOptionItem : public PaymentRequestItemList::Item {
 public:
  ShippingOptionItem(mojom::PaymentShippingOption* shipping_option,
                     PaymentRequestSpec* spec,
                     PaymentRequestState* state,
                     PaymentRequestItemList* parent_list,
                     PaymentRequestDialogView* dialog,
                     bool selected)
      : PaymentRequestItemList::Item(spec,
                                     state,
                                     parent_list,
                                     selected,
                                     /*clickable=*/true,
                                     /*show_edit_button=*/false),
        shipping_option_(shipping_option) {
    Init();
  }
  ~ShippingOptionItem() override {}

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateContentView(
      base::string16* accessible_content) override {
    return CreateShippingOptionLabel(
        shipping_option_,
        spec()->GetFormattedCurrencyAmount(shipping_option_->amount),
        /*emphasize_label=*/true, accessible_content);
  }

  void SelectedStateChanged() override {
    if (selected()) {
      state()->SetSelectedShippingOption(shipping_option_->id);
    }
  }

  base::string16 GetNameForDataType() override {
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_SHIPPING_OPTION_LABEL);
  }

  bool CanBeSelected() override {
    // Shipping options are vetted by the website; they're all OK to select.
    return true;
  }

  void PerformSelectionFallback() override {
    // Since CanBeSelected() is always true, this should never be called.
    NOTREACHED();
  }

  void EditButtonPressed() override {
    // This subclass doesn't display the edit button.
    NOTREACHED();
  }

  mojom::PaymentShippingOption* shipping_option_;

  DISALLOW_COPY_AND_ASSIGN(ShippingOptionItem);
};

}  // namespace

ShippingOptionViewController::ShippingOptionViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      shipping_option_list_(dialog) {
  spec->AddObserver(this);
  for (const auto& option : spec->GetShippingOptions()) {
    shipping_option_list_.AddItem(std::make_unique<ShippingOptionItem>(
        option.get(), spec, state, &shipping_option_list_, dialog,
        option.get() == spec->selected_shipping_option()));
  }
}

ShippingOptionViewController::~ShippingOptionViewController() {
  spec()->RemoveObserver(this);
}

void ShippingOptionViewController::OnSpecUpdated() {
  if (spec()->current_update_reason() ==
      PaymentRequestSpec::UpdateReason::SHIPPING_OPTION) {
    dialog()->GoBack();
  } else {
    UpdateContentView();
  }
}

base::string16 ShippingOptionViewController::GetSheetTitle() {
  return GetShippingOptionSectionString(spec()->shipping_type());
}

void ShippingOptionViewController::FillContentView(views::View* content_view) {
  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view->AddChildView(shipping_option_list_.CreateListView().release());
}

std::unique_ptr<views::View>
ShippingOptionViewController::CreateExtraFooterView() {
  return nullptr;
}

bool ShippingOptionViewController::ShouldShowSecondaryButton() {
  // Do not show the "Cancel Payment" button.
  return false;
}

}  // namespace payments
