// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace payments {

namespace {

class PaymentMethodListItem final : public PaymentRequestItemList::Item {
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
      : PaymentRequestItemList::Item(spec,
                                     state,
                                     list,
                                     selected,
                                     /*clickable=*/true,
                                     /*show_edit_button=*/false),
        app_(app),
        dialog_(dialog) {
    Init();
  }

  PaymentMethodListItem(const PaymentMethodListItem&) = delete;
  PaymentMethodListItem& operator=(const PaymentMethodListItem&) = delete;

  ~PaymentMethodListItem() override {}

  base::WeakPtr<PaymentRequestRowView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
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
        gfx::Insets::VH(kPaymentRequestRowVerticalInsets, 0));
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    card_info_container->SetLayoutManager(std::move(box_layout));

    std::u16string label_str = app_->GetLabel();
    if (!label_str.empty())
      card_info_container->AddChildView(new views::Label(label_str));
    std::u16string sublabel = app_->GetSublabel();
    if (!sublabel.empty())
      card_info_container->AddChildView(new views::Label(sublabel));
    std::u16string missing_info;
    if (!app_->IsCompleteForPayment()) {
      missing_info = app_->GetMissingInfoLabel();
      views::Label* const label =
          card_info_container->AddChildView(std::make_unique<views::Label>(
              missing_info, CONTEXT_DIALOG_BODY_TEXT_SMALL));
      views::SetCascadingColorProviderColor(
          label, views::kCascadingLabelEnabledColor, ui::kColorLinkForeground);
    }

    *accessible_content = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_PROFILE_LABELS_ACCESSIBLE_FORMAT, label_str, sublabel,
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

  void PerformSelectionFallback() override {}

  void EditButtonPressed() override {}

  base::WeakPtr<PaymentApp> app_;
  base::WeakPtr<PaymentRequestDialogView> dialog_;
  base::WeakPtrFactory<PaymentMethodListItem> weak_ptr_factory_{this};
};

}  // namespace

PaymentMethodViewController::PaymentMethodViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      payment_method_list_(dialog) {
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
  return false;
}

std::u16string PaymentMethodViewController::GetSecondaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_CARD);
}

int PaymentMethodViewController::GetSecondaryButtonId() {
  return static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON);
}

base::WeakPtr<PaymentRequestSheetController>
PaymentMethodViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
