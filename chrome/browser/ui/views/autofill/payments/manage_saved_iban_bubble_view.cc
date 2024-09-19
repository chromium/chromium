// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

ManageSavedIbanBubbleView::ManageSavedIbanBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    IbanBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller->GetAcceptButtonText());
  SetExtraView(
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              [](ManageSavedIbanBubbleView* bubble) {
                bubble->controller()->OnManageSavedIbanExtraButtonClicked();
              },
              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_SAVED_PAYMENT_METHODS)))
      ->SetID(autofill::DialogViewId::MANAGE_IBANS_BUTTON);
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void ManageSavedIbanBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
  AssignIdsToDialogButtons();
}

void ManageSavedIbanBubbleView::Hide() {
  CloseBubble();

  // If `controller_` is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out `controller_`'s reference to `this`. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

std::u16string ManageSavedIbanBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void ManageSavedIbanBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

ManageSavedIbanBubbleView::~ManageSavedIbanBubbleView() = default;

void ManageSavedIbanBubbleView::AssignIdsToDialogButtons() {
  auto* ok_button = GetOkButton();
  if (ok_button) {
    ok_button->SetID(DialogViewId::OK_BUTTON);
  }
  auto* cancel_button = GetCancelButton();
  if (cancel_button) {
    cancel_button->SetID(DialogViewId::CANCEL_BUTTON);
  }
  if (nickname_label_) {
    nickname_label_->SetID(DialogViewId::NICKNAME_LABEL);
  }
}

void ManageSavedIbanBubbleView::Init() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetID(DialogViewId::MAIN_CONTENT_VIEW_LOCAL);
  SetProperty(views::kMarginsKey, gfx::Insets());
  const int row_height = views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);
  views::TableLayout* layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(
          views::TableLayout::kFixedSize,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kFixed, 0, 0)
      // Add a row for IBAN label and the value of IBAN. It might happen that
      // the revealed IBAN value is too long to fit in a single line while the
      // obscured IBAN value can fit in one line, so fix the height to fit both
      // cases so toggling visibility does not change the bubble's overall
      // height.
      .AddRows(1, views::TableLayout::kFixedSize, row_height * 2);

  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_LABEL),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));

  views::Label* iban_value = AddChildView(std::make_unique<views::Label>(
      controller_->GetIban().GetIdentifierStringForAutofillDisplay(
          /*is_value_masked=*/false),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));

  iban_value->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  iban_value->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  iban_value->SetMultiLine(true);

  // Nickname label row will be added if a nickname was saved in the IBAN save
  // bubble, which is displayed previously in the flow.
  if (!controller_->GetIban().nickname().empty()) {
    layout
        ->AddPaddingRow(views::TableLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL))
        // Add a row for the nickname label.
        .AddRows(1, views::TableLayout::kFixedSize);
    AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
    // TODO(crbug.com/40233611): Revisit how the nickname will be shown if it's
    // too long.
    nickname_label_ = AddChildView(std::make_unique<views::Label>(
        controller_->GetIban().nickname(),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
    nickname_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
}

BEGIN_METADATA(ManageSavedIbanBubbleView)
END_METADATA

}  // namespace autofill
