// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_option_view_controller.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
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

class ShippingOptionItem final : public PaymentRequestItemList::Item {
 public:
  ShippingOptionItem(mojom::PaymentShippingOptionPtr shipping_option,
                     base::WeakPtr<PaymentRequestSpec> spec,
                     base::WeakPtr<PaymentRequestState> state,
                     PaymentRequestItemList* parent_list,
                     base::WeakPtr<PaymentRequestDialogView> dialog,
                     bool selected)
      : PaymentRequestItemList::Item(spec,
                                     state,
                                     parent_list,
                                     selected,
                                     /*clickable=*/true,
                                     /*show_edit_button=*/false),
        shipping_option_(std::move(shipping_option)) {
    Init();
  }

  ShippingOptionItem(const ShippingOptionItem&) = delete;
  ShippingOptionItem& operator=(const ShippingOptionItem&) = delete;

  ~ShippingOptionItem() override {}

  base::WeakPtr<PaymentRequestRowView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateContentView(
      std::u16string* accessible_content) override {
    return CreateShippingOptionLabel(
        shipping_option_.get(),
        /*formatted_amount=*/
        spec() ? spec()->GetFormattedCurrencyAmount(shipping_option_->amount)
               : std::u16string(),
        /*emphasize_label=*/true, accessible_content);
  }

  void SelectedStateChanged() override {
    if (selected()) {
      state()->SetSelectedShippingOption(shipping_option_->id);
    }
  }

  std::u16string GetNameForDataType() override {
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

  mojom::PaymentShippingOptionPtr shipping_option_;
  base::WeakPtrFactory<ShippingOptionItem> weak_ptr_factory_{this};
};

}  // namespace

ShippingOptionViewController::ShippingOptionViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      shipping_option_list_(dialog) {
  spec->AddObserver(this);
  for (const auto& option : spec->GetShippingOptions()) {
    shipping_option_list_.AddItem(std::make_unique<ShippingOptionItem>(
        option->Clone(), spec, state, &shipping_option_list_, dialog,
        option.get() == spec->selected_shipping_option()));
  }
}

ShippingOptionViewController::~ShippingOptionViewController() {
  if (spec())
    spec()->RemoveObserver(this);
}

void ShippingOptionViewController::OnSpecUpdated() {
  if (!spec())
    return;

  if (spec()->current_update_reason() ==
      PaymentRequestSpec::UpdateReason::SHIPPING_OPTION) {
    dialog()->GoBack();
  } else {
    UpdateContentView();
  }
}

std::u16string ShippingOptionViewController::GetSheetTitle() {
  return spec() ? GetShippingOptionSectionString(spec()->shipping_type())
                : std::u16string();
}

void ShippingOptionViewController::FillContentView(views::View* content_view) {
  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view->AddChildView(shipping_option_list_.CreateListView().release());
}

std::unique_ptr<views::View>
ShippingOptionViewController::CreateExtraFooterView() {
  return nullptr;
}

bool ShippingOptionViewController::ShouldShowPrimaryButton() {
  return false;
}

bool ShippingOptionViewController::ShouldShowSecondaryButton() {
  // Do not show the "Cancel Payment" button.
  return false;
}

base::WeakPtr<PaymentRequestSheetController>
ShippingOptionViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
