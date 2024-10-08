// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

const int kMaxNicknameChars = 25;

}  // namespace

SaveIbanBubbleView::SaveIbanBubbleView(views::View* anchor_view,
                                       content::WebContents* web_contents,
                                       IbanBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller->GetAcceptButtonText());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller->GetDeclineButtonText());
  SetAcceptCallback(base::BindOnce(&SaveIbanBubbleView::OnDialogAccepted,
                                   base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void SaveIbanBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
  AssignIdsToDialogButtonsForTesting();  // IN-TEST
}

void SaveIbanBubbleView::Hide() {
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

void SaveIbanBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          *bundle.GetImageSkiaNamed(IDR_SAVE_CARD),
          *bundle.GetImageSkiaNamed(IDR_SAVE_CARD_DARK),
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));

  if (controller_->IsUploadSave()) {
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  }
}

std::u16string SaveIbanBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveIbanBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void SaveIbanBubbleView::ContentsChanged(views::Textfield* sender,
                                         const std::u16string& new_contents) {
  if (new_contents.length() > kMaxNicknameChars) {
    nickname_textfield_->SetText(new_contents.substr(0, kMaxNicknameChars));
  }
  // Update the IBAN nickname count label to current_length/max_length,
  // e.g. "6/25".
  UpdateNicknameLengthLabel();
}

SaveIbanBubbleView::~SaveIbanBubbleView() = default;

void SaveIbanBubbleView::CreateMainContentView() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  SetID(controller_->IsUploadSave() ? DialogViewId::MAIN_CONTENT_VIEW_UPLOAD
                                    : DialogViewId::MAIN_CONTENT_VIEW_LOCAL);
  SetProperty(views::kMarginsKey, gfx::Insets());

  // If applicable, add the upload explanation label. Appears above the IBAN
  // info.
  std::u16string explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* explanation_label = AddChildView(std::make_unique<views::Label>(
        explanation, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY));
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  const int row_height = views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);

  auto* iban_content = AddChildView(std::make_unique<views::View>());
  auto* iban_layout =
      iban_content->SetLayoutManager(std::make_unique<views::TableLayout>());

  iban_layout
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
      .AddRows(1, views::TableLayout::kFixedSize, row_height * 2)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     ChromeLayoutProvider::Get()->GetDistanceMetric(
                         views::DISTANCE_RELATED_CONTROL_VERTICAL))
      // Add a row for nickname label and the input text field.
      .AddRows(1, views::TableLayout::kFixedSize);

  iban_content->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_LABEL),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));

  views::Label* iban_value =
      iban_content->AddChildView(std::make_unique<views::Label>(
          controller_->GetIban().GetIdentifierStringForAutofillDisplay(
              /*is_value_masked=*/false),
          views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));

  iban_value->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  iban_value->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  iban_value->SetMultiLine(true);

  iban_content->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));

  // Adds view that combines nickname textfield and nickname length count label.
  auto* nickname_input_textfield_view =
      iban_content->AddChildView(std::make_unique<views::BoxLayoutView>());
  nickname_input_textfield_view->SetBorder(
      views::CreateSolidBorder(1, SK_ColorLTGRAY));
  nickname_input_textfield_view->SetInsideBorderInsets(
      views::LayoutProvider::Get()->GetInsetsMetric(
          views::InsetsMetric::INSETS_LABEL_BUTTON));
  nickname_input_textfield_view->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  nickname_input_textfield_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));

  // Adds nickname textfield.
  nickname_textfield_ = nickname_input_textfield_view->AddChildView(
      std::make_unique<views::Textfield>());
  nickname_textfield_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME));
  nickname_textfield_->SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_TEXT);
  nickname_textfield_->set_controller(this);
  nickname_textfield_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PLACEHOLDER));
  nickname_textfield_->SetProperty(views::kBoxLayoutFlexKey,
                                   views::BoxLayoutFlexSpecification());
  nickname_textfield_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  nickname_textfield_->SetBorder(views::NullBorder());

  // Adds nickname length count label.
  // Note: nickname is empty at the prompt.
  nickname_length_label_ = nickname_input_textfield_view->AddChildView(
      std::make_unique<views::Label>(/*text=*/u"", views::style::CONTEXT_LABEL,
                                     views::style::STYLE_SECONDARY));
  nickname_length_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_RIGHT);
  UpdateNicknameLengthLabel();

  if (std::unique_ptr<views::View> legal_message_view =
          CreateLegalMessageView()) {
    legal_message_view->SetID(DialogViewId::LEGAL_MESSAGE_VIEW);
    AddChildView(std::move(legal_message_view));
  }
}

void SaveIbanBubbleView::AssignIdsToDialogButtonsForTesting() {
  auto* ok_button = GetOkButton();
  if (ok_button) {
    ok_button->SetID(DialogViewId::OK_BUTTON);
  }
  auto* cancel_button = GetCancelButton();
  if (cancel_button) {
    cancel_button->SetID(DialogViewId::CANCEL_BUTTON);
  }

  if (nickname_textfield_) {
    nickname_textfield_->SetID(DialogViewId::NICKNAME_TEXTFIELD);
  }
}

void SaveIbanBubbleView::OnDialogAccepted() {
  if (controller_) {
    DCHECK(nickname_textfield_);
    controller_->OnAcceptButton(nickname_textfield_->GetText());
  }
}

void SaveIbanBubbleView::LinkClicked(const GURL& url) {
  if (controller_) {
    controller()->OnLegalMessageLinkClicked(url);
  }
}

void SaveIbanBubbleView::Init() {
  // For server IBAN save, there is an explanation between the title and the
  // controls; use DialogContentType::kText. For local IBAN save, since there is
  // no explanation, use DialogContentType::kControl instead.
  // For server IBANs, there are legal messages before the buttons, so use
  // DialogContentType::kText. For local IBANs, since there is no legal message,
  // use DialogContentType::kControl instead.
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText,
      !controller_->GetLegalMessageLines().empty()
          ? views::DialogContentType::kText
          : views::DialogContentType::kControl));

  CreateMainContentView();
}

std::unique_ptr<views::View> SaveIbanBubbleView::CreateLegalMessageView() {
  const LegalMessageLines message_lines = controller()->GetLegalMessageLines();
  if (message_lines.empty()) {
    return nullptr;
  }

  auto legal_message_view = std::make_unique<views::BoxLayoutView>();
  legal_message_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  legal_message_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));

  legal_message_view->AddChildView(std::make_unique<LegalMessageView>(
      message_lines, base::UTF8ToUTF16(controller()->GetAccountInfo().email),
      GetProfileAvatar(controller()->GetAccountInfo()),
      base::BindRepeating(&SaveIbanBubbleView::LinkClicked,
                          base::Unretained(this))));

  legal_message_view->SetID(DialogViewId::LEGAL_MESSAGE_VIEW);
  return legal_message_view;
}

void SaveIbanBubbleView::UpdateNicknameLengthLabel() {
  nickname_length_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_IBAN_NICKNAME_COUNT_BY,
      base::NumberToString16(nickname_textfield_->GetText().length()),
      base::NumberToString16(kMaxNicknameChars)));
}

BEGIN_METADATA(SaveIbanBubbleView)
END_METADATA

}  // namespace autofill
