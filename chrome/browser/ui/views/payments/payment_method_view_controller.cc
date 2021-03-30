// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/autofill_payment_app.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/vector_icons.h"

namespace payments {

namespace {

class PaymentMethodListItem : public PaymentRequestItemList::Item {
 public:
  // Does not take ownership of |app|, which should not be null and should
  // outlive this object. |list| is the PaymentRequestItemList object that will
  // own this.
  PaymentMethodListItem(base::WeakPtr<PaymentApp> app,
                        base::WeakPtr<PaymentRequestSpec> spec,
                        base::WeakPtr<PaymentRequestState> state,
                        PaymentRequestItemList* list,
                        base::WeakPtr<PaymentRequestDialogView> dialog,
                        bool selected)
      : PaymentRequestItemList::Item(
            spec,
            state,
            list,
            selected,
            /*clickable=*/true,
            /*show_edit_button=*/app->type() == PaymentApp::Type::AUTOFILL),
        app_(app),
        dialog_(dialog) {
    Init();
  }
  ~PaymentMethodListItem() override {}

 private:
  void ShowEditor() {
    if (!app_)
      return;

    switch (app_->type()) {
      case PaymentApp::Type::AUTOFILL:
        // Since we are a list item, we only care about the on_edited callback.
        dialog_->ShowCreditCardEditor(
            BackNavigationType::kPaymentSheet,
            /*on_edited=*/
            base::BindOnce(&PaymentRequestState::SetSelectedApp, state(), app_),
            /*on_added=*/
            base::OnceCallback<void(const autofill::CreditCard&)>(),
            static_cast<AutofillPaymentApp*>(app_.get())->credit_card());
        return;
      case PaymentApp::Type::UNDEFINED:
        // Intentionally fall through.
      case PaymentApp::Type::NATIVE_MOBILE_APP:
        // Intentionally fall through.
      case PaymentApp::Type::SERVICE_WORKER_APP:
        // Intentionally fall through.
      case PaymentApp::Type::INTERNAL:
        // We cannot edit these types of payment apps.
        return;
    }
    NOTREACHED();
  }

  // PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateExtraView() override {
    return app_ ? CreateAppIconView(app_->icon_resource_id(),
                                    app_->icon_bitmap(), app_->GetLabel())
                : nullptr;
  }

  std::unique_ptr<views::View> CreateContentView(
      std::u16string* accessible_content) override {
    DCHECK(accessible_content);
    auto card_info_container = std::make_unique<views::View>();
    if (!app_)
      return card_info_container;

    card_info_container->SetCanProcessEventsWithinSubtree(false);

    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(kPaymentRequestRowVerticalInsets, 0));
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    card_info_container->SetLayoutManager(std::move(box_layout));

    std::u16string label = app_->GetLabel();
    if (!label.empty())
      card_info_container->AddChildView(new views::Label(label));
    std::u16string sublabel = app_->GetSublabel();
    if (!sublabel.empty())
      card_info_container->AddChildView(new views::Label(sublabel));
    std::u16string missing_info;
    if (!app_->IsCompleteForPayment()) {
      missing_info = app_->GetMissingInfoLabel();
      auto missing_info_label = std::make_unique<views::Label>(
          missing_info, CONTEXT_DIALOG_BODY_TEXT_SMALL);
      missing_info_label->SetEnabledColor(
          missing_info_label->GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_LinkEnabled));
      card_info_container->AddChildView(missing_info_label.release());
    }

    *accessible_content = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_PROFILE_LABELS_ACCESSIBLE_FORMAT, label, sublabel,
        missing_info);

    return card_info_container;
  }

  void SelectedStateChanged() override {
    if (app_ && selected()) {
      state()->SetSelectedApp(app_);
      dialog_->GoBack();
    }
  }

  std::u16string GetNameForDataType() override {
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL);
  }

  bool CanBeSelected() override {
    // If an app can't be selected because it's not complete,
    // PerformSelectionFallback is called, where the app can be made complete.
    // This applies only to AutofillPaymentApp, each one of which is a credit
    // card, so PerformSelectionFallback will open the card editor.
    return app_ && app_->IsCompleteForPayment();
  }

  void PerformSelectionFallback() override { ShowEditor(); }

  void EditButtonPressed() override { ShowEditor(); }

  base::WeakPtr<PaymentApp> app_;
  base::WeakPtr<PaymentRequestDialogView> dialog_;

  DISALLOW_COPY_AND_ASSIGN(PaymentMethodListItem);
};

}  // namespace

PaymentMethodViewController::PaymentMethodViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      payment_method_list_(dialog),
      enable_add_card_(!state->is_retry_called() &&
                       spec->supports_basic_card()) {
  const std::vector<std::unique_ptr<PaymentApp>>& available_apps =
      state->available_apps();
  for (const auto& app : available_apps) {
    auto item = std::make_unique<PaymentMethodListItem>(
        app->AsWeakPtr(), spec, state, &payment_method_list_, dialog,
        app.get() == state->selected_app());
    payment_method_list_.AddItem(std::move(item));
  }
}

PaymentMethodViewController::~PaymentMethodViewController() {}

std::u16string PaymentMethodViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME);
}

void PaymentMethodViewController::FillContentView(views::View* content_view) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  content_view->SetLayoutManager(std::move(layout));

  std::unique_ptr<views::View> list_view =
      payment_method_list_.CreateListView();
  list_view->SetID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  content_view->AddChildView(list_view.release());
}

bool PaymentMethodViewController::ShouldShowPrimaryButton() {
  return false;
}

bool PaymentMethodViewController::ShouldShowSecondaryButton() {
  return enable_add_card_;
}

std::u16string PaymentMethodViewController::GetSecondaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_CARD);
}

PaymentRequestSheetController::ButtonCallback
PaymentMethodViewController::GetSecondaryButtonCallback() {
  return base::BindRepeating(
      &PaymentRequestDialogView::ShowCreditCardEditor, dialog(),
      BackNavigationType::kPaymentSheet, base::RepeatingClosure(),
      base::BindRepeating(&PaymentRequestState::AddAutofillPaymentApp, state(),
                          true),
      nullptr);
}

int PaymentMethodViewController::GetSecondaryButtonId() {
  return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON);
}

}  // namespace payments
