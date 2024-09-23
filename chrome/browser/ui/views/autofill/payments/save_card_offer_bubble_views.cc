// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_offer_bubble_views.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace autofill {

namespace {

int GetLightModeBannerIdForSaveCard() {
  switch (autofill::GetUpdatedDesktopUiTreatmentArm()) {
    case autofill::UpdatedDesktopUiTreatmentArm::kSecurityFocus:
      return IDR_SAVE_CARD_SECURITY;
    case autofill::UpdatedDesktopUiTreatmentArm::kConvenienceFocus:
      return IDR_SAVE_CARD_CONVENIENCE;
    case autofill::UpdatedDesktopUiTreatmentArm::kEducationFocus:
      return IDR_SAVE_CARD_EDUCATION;
    case autofill::UpdatedDesktopUiTreatmentArm::kDefault:
      return IDR_SAVE_CARD;
  }
}

int GetDarkModeBannerIdForSaveCard() {
  switch (autofill::GetUpdatedDesktopUiTreatmentArm()) {
    case autofill::UpdatedDesktopUiTreatmentArm::kSecurityFocus:
      return IDR_SAVE_CARD_SECURITY_DARK;
    case autofill::UpdatedDesktopUiTreatmentArm::kConvenienceFocus:
      return IDR_SAVE_CARD_CONVENIENCE_DARK;
    case autofill::UpdatedDesktopUiTreatmentArm::kEducationFocus:
      return IDR_SAVE_CARD_EDUCATION_DARK;
    case autofill::UpdatedDesktopUiTreatmentArm::kDefault:
      return IDR_SAVE_CARD_DARK;
  }
}

}  // namespace

SaveCardOfferBubbleViews::SaveCardOfferBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, web_contents, controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerAutofillCard, web_contents, 10000);
}

void SaveCardOfferBubbleViews::Init() {
  SaveCardBubbleViews::Init();

  if (controller() &&
      (controller()->GetBubbleType() == BubbleType::UPLOAD_SAVE ||
       controller()->GetBubbleType() == BubbleType::UPLOAD_IN_PROGRESS) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    loading_row_ = AddChildView(CreateLoadingRow());
    if (controller()->GetBubbleType() == BubbleType::UPLOAD_IN_PROGRESS) {
      ShowThrobber();
    }
  }

  SetExtraView(CreateUploadExplanationView());
}

bool SaveCardOfferBubbleViews::Accept() {
  bool show_throbber =
      controller() &&
      controller()->GetBubbleType() == BubbleType::UPLOAD_SAVE &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSaveCardLoadingAndConfirmation);

  if (show_throbber) {
    ShowThrobber();
  }

  if (controller()) {
    controller()->OnSaveButton(
        {cardholder_name_textfield_ ? cardholder_name_textfield_->GetText()
                                    : std::u16string(),
         month_input_dropdown_
             ? month_input_dropdown_->GetModel()->GetItemAt(
                   month_input_dropdown_->GetSelectedIndex().value())
             : std::u16string(),
         year_input_dropdown_
             ? year_input_dropdown_->GetModel()->GetItemAt(
                   year_input_dropdown_->GetSelectedIndex().value())
             : std::u16string()});
  }

  // If a throbber is shown, don't automatically close the bubble view upon
  // acceptance.
  return !show_throbber;
}

bool SaveCardOfferBubbleViews::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button == ui::mojom::DialogButton::kCancel) {
    return true;
  }

  DCHECK_EQ(ui::mojom::DialogButton::kOk, button);
  if (cardholder_name_textfield_) {
    // Make sure we are not requesting cardholder name and expiration date at
    // the same time.
    DCHECK(!month_input_dropdown_ && !year_input_dropdown_);
    // If requesting the user confirm the name, it cannot be blank.
    std::u16string trimmed_text;
    base::TrimWhitespace(cardholder_name_textfield_->GetText(), base::TRIM_ALL,
                         &trimmed_text);
    return !trimmed_text.empty();
  }
  // If requesting the user select the expiration date, it cannot be unselected
  // or expired.
  if (month_input_dropdown_ || year_input_dropdown_) {
    // Make sure we are not requesting cardholder name and expiration date at
    // the same time.
    DCHECK(!cardholder_name_textfield_);
    int month_value = 0, year_value = 0;
    if (!base::StringToInt(
            month_input_dropdown_->GetModel()->GetItemAt(
                month_input_dropdown_->GetSelectedIndex().value()),
            &month_value) ||
        !base::StringToInt(
            year_input_dropdown_->GetModel()->GetItemAt(
                year_input_dropdown_->GetSelectedIndex().value()),
            &year_value)) {
      return false;
    }
    return IsValidCreditCardExpirationDate(year_value, month_value,
                                           AutofillClock::Now());
  }

  return true;
}

void SaveCardOfferBubbleViews::AddedToWidget() {
  SaveCardBubbleViews::AddedToWidget();
  // Set the header image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  int light_mode_banner_id;
  int dark_mode_banner_id;

  switch (controller()->GetBubbleType()) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::UPLOAD_COMPLETED:
      // Updated banner/text pairs are for upload save only.
      light_mode_banner_id = GetLightModeBannerIdForSaveCard();
      dark_mode_banner_id = GetDarkModeBannerIdForSaveCard();
      break;
    case BubbleType::LOCAL_CVC_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
      // CVC bubbles show their own CVC-based banner image.
      light_mode_banner_id = IDR_SAVE_CVC;
      dark_mode_banner_id = IDR_SAVE_CVC_DARK;
      break;
    default:
      light_mode_banner_id = IDR_SAVE_CARD;
      dark_mode_banner_id = IDR_SAVE_CARD_DARK;
  }

  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(light_mode_banner_id),
      *bundle.GetImageSkiaNamed(dark_mode_banner_id),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

void SaveCardOfferBubbleViews::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(cardholder_name_textfield_, sender);
  DialogModelChanged();
}

SaveCardOfferBubbleViews::~SaveCardOfferBubbleViews() = default;

std::unique_ptr<views::View> SaveCardOfferBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view =
      SaveCardBubbleViews::CreateMainContentView();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  view->SetID(controller()->IsUploadSave()
                  ? DialogViewId::MAIN_CONTENT_VIEW_UPLOAD
                  : DialogViewId::MAIN_CONTENT_VIEW_LOCAL);

  // If necessary, add the cardholder name label and textfield to the upload
  // save dialog.
  if (controller()->ShouldRequestNameFromUser()) {
    std::unique_ptr<views::View> cardholder_name_label_row =
        std::make_unique<views::View>();

    // Set up cardholder name label.
    // TODO(jsaul): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right choice
    //              here, but DISTANCE_RELATED_CONTROL_HORIZONTAL gives too much
    //              padding. Make a new Harmony DistanceMetric?
    cardholder_name_label_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            provider->GetDistanceMetric(
                views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
    std::unique_ptr<views::Label> cardholder_name_label =
        std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME),
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_SECONDARY);
    cardholder_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    cardholder_name_label_row->AddChildView(std::move(cardholder_name_label));

    // Prepare the prefilled cardholder name.
    std::u16string prefilled_name =
        base::UTF8ToUTF16(controller()->GetAccountInfo().full_name);

    // Set up cardholder name label tooltip ONLY if the cardholder name
    // textfield will be prefilled and sync transport for Wallet data is not
    // active. Otherwise, this tooltip's info will appear in CreateExtraView()'s
    // tooltip.
    if (!prefilled_name.empty() &&
        !controller()->IsPaymentsSyncTransportEnabledWithoutSyncFeature()) {
      constexpr int kTooltipIconSize = 12;
      std::unique_ptr<views::TooltipIcon> cardholder_name_tooltip =
          std::make_unique<views::TooltipIcon>(
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME_TOOLTIP),
              kTooltipIconSize);
      cardholder_name_tooltip->SetAnchorPointArrow(
          views::BubbleBorder::Arrow::TOP_LEFT);
      cardholder_name_tooltip->SetID(DialogViewId::CARDHOLDER_NAME_TOOLTIP);
      cardholder_name_label_row->AddChildView(
          std::move(cardholder_name_tooltip));
    }

    // Set up cardholder name textfield.
    DCHECK(!cardholder_name_textfield_);
    cardholder_name_textfield_ = new views::Textfield();
    cardholder_name_textfield_->set_controller(this);
    cardholder_name_textfield_->SetID(DialogViewId::CARDHOLDER_NAME_TEXTFIELD);
    cardholder_name_textfield_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME));
    cardholder_name_textfield_->SetTextInputType(
        ui::TextInputType::TEXT_INPUT_TYPE_TEXT);
    cardholder_name_textfield_->SetText(prefilled_name);
    autofill_metrics::LogSaveCardCardholderNamePrefilled(
        !prefilled_name.empty());

    // Add cardholder name elements to a single view, then to the final dialog.
    std::unique_ptr<views::View> cardholder_name_view =
        std::make_unique<views::View>();
    cardholder_name_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    cardholder_name_view->AddChildView(std::move(cardholder_name_label_row));
    cardholder_name_view->AddChildView(cardholder_name_textfield_.get());
    view->AddChildView(std::move(cardholder_name_view));
  }

  if (controller()->ShouldRequestExpirationDateFromUser()) {
    view->AddChildView(CreateRequestExpirationDateView());
  }

  if (std::unique_ptr<views::View> legal_message_view = CreateLegalMessageView()) {
    legal_message_view->SetID(DialogViewId::LEGAL_MESSAGE_VIEW);
    view->AddChildView(std::move(legal_message_view));
  }

  return view;
}

std::unique_ptr<views::View>
SaveCardOfferBubbleViews::CreateRequestExpirationDateView() {
  auto expiration_date_view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  expiration_date_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  expiration_date_view->SetID(DialogViewId::EXPIRATION_DATE_VIEW);

  // Set up the month and year comboboxes.
  month_input_dropdown_ = new views::Combobox(&month_combobox_model_);
  month_input_dropdown_->SetCallback(base::BindRepeating(
      &SaveCardOfferBubbleViews::DialogModelChanged, base::Unretained(this)));
  month_input_dropdown_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH));
  month_input_dropdown_->SetID(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH);

  const CreditCard& card = controller()->GetCard();
  // Pre-populate expiration date month if it is detected.
  if (card.expiration_month()) {
    month_combobox_model_.SetDefaultIndexByMonth(card.expiration_month());
    month_input_dropdown_->SetSelectedIndex(
        month_combobox_model_.GetDefaultIndex());
  }

  year_input_dropdown_ = new views::Combobox(&year_combobox_model_);
  year_input_dropdown_->SetCallback(base::BindRepeating(
      &SaveCardOfferBubbleViews::DialogModelChanged, base::Unretained(this)));
  year_input_dropdown_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR));
  year_input_dropdown_->SetID(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR);

  // Pre-populate expiration date year if it is not passed.
  if (IsValidCreditCardExpirationYear(card.expiration_year(),
                                      AutofillClock::Now())) {
    year_combobox_model_.SetDefaultIndexByYear(card.expiration_year());
    year_input_dropdown_->SetSelectedIndex(
        year_combobox_model_.GetDefaultIndex());
  }

  auto input_row = std::make_unique<views::View>();
  input_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL)));
  input_row->AddChildView(month_input_dropdown_.get());
  input_row->AddChildView(year_input_dropdown_.get());

  // Set up expiration date label.
  auto expiration_date_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_EXPIRATION_DATE),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  expiration_date_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  expiration_date_view->AddChildView(std::move(expiration_date_label));
  expiration_date_view->AddChildView(std::move(input_row));

  return expiration_date_view;
}

std::unique_ptr<views::View>
SaveCardOfferBubbleViews::CreateUploadExplanationView() {
  // Only show the (i) info icon for upload saves using implicit sync.
  // GetLegalMessageLines() being empty denotes a local save.
  if (controller()->GetLegalMessageLines().empty() ||
      !controller()->IsPaymentsSyncTransportEnabledWithoutSyncFeature()) {
    return nullptr;
  }

  // CreateMainContentView() must happen prior to this so that |prefilled_name|
  // gets populated.
  auto upload_explanation_tooltip = std::make_unique<
      views::TooltipIcon>(l10n_util::GetStringUTF16(
      (cardholder_name_textfield_ &&
       !cardholder_name_textfield_->GetText().empty())
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_AND_CARDHOLDER_NAME_TOOLTIP
          : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_TOOLTIP));
  upload_explanation_tooltip->SetBubbleWidth(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  upload_explanation_tooltip->SetAnchorPointArrow(
      views::BubbleBorder::Arrow::TOP_RIGHT);
  upload_explanation_tooltip->SetID(DialogViewId::UPLOAD_EXPLANATION_TOOLTIP);
  return upload_explanation_tooltip;
}

std::unique_ptr<LegalMessageView>
SaveCardOfferBubbleViews::CreateLegalMessageView() {
  const LegalMessageLines message_lines = controller()->GetLegalMessageLines();

  if (message_lines.empty()) {
    return nullptr;
  }

  LegalMessageView::LinkClickedCallback LegalMessageCallBack =
      base::BindRepeating(&SaveCardOfferBubbleViews::LinkClicked,
                          base::Unretained(this));

  return std::make_unique<LegalMessageView>(
      message_lines, base::UTF8ToUTF16(controller()->GetAccountInfo().email),
      GetProfileAvatar(controller()->GetAccountInfo()), LegalMessageCallBack);
}

std::unique_ptr<views::View> SaveCardOfferBubbleViews::CreateLoadingRow() {
  auto loading_row = std::make_unique<views::BoxLayoutView>();

  // Initialize `loading_row` as hidden because it should only be visible after
  // the user accepts uploading the card.
  loading_row->SetVisible(false);

  loading_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  loading_row->SetInsideBorderInsets(gfx::Insets::TLBR(10, 0, 0, 40));

  loading_throbber_ =
      loading_row->AddChildView(std::make_unique<views::Throbber>());
  loading_throbber_->SetID(DialogViewId::LOADING_THROBBER);

  return loading_row;
}

void SaveCardOfferBubbleViews::LinkClicked(const GURL& url) {
  if (controller())
    controller()->OnLegalMessageLinkClicked(url);
}

void SaveCardOfferBubbleViews::ShowThrobber() {
  if (loading_row_ == nullptr) {
    return;
  }

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetExtraView({nullptr});

  CHECK(loading_throbber_);

  loading_throbber_->Start();
  loading_row_->SetVisible(true);
  loading_throbber_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_LOADING_THROBBER_ACCESSIBLE_NAME));

  DialogModelChanged();
}

BEGIN_METADATA(SaveCardOfferBubbleViews)
END_METADATA

}  // namespace autofill
