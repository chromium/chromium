// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

// The delay between card being fetched and filled card information bubble being
// shown.
constexpr base::TimeDelta kFilledCardInformationBubbleDelay =
    base::Seconds(1.5);

// static
FilledCardInformationBubbleController*
FilledCardInformationBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  FilledCardInformationBubbleControllerImpl::CreateForWebContents(web_contents);
  return FilledCardInformationBubbleControllerImpl::FromWebContents(
      web_contents);
}

// static
FilledCardInformationBubbleController*
FilledCardInformationBubbleController::Get(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  return FilledCardInformationBubbleControllerImpl::FromWebContents(
      web_contents);
}

FilledCardInformationBubbleControllerImpl::
    ~FilledCardInformationBubbleControllerImpl() = default;

void FilledCardInformationBubbleControllerImpl::ShowBubble(
    const FilledCardInformationBubbleOptions& options) {
  // If another bubble is visible, dismiss it and show a new one since the card
  // information can be different.
  if (bubble_view()) {
    HideBubble();
  }

  DCHECK(options.IsValid());
  options_ = options;
  is_user_gesture_ = false;
  should_icon_be_visible_ = true;

  // Delay the showing of the filled card information bubble so that the form
  // filling and the filled card information bubble appearance do not happen at
  // the same time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FilledCardInformationBubbleControllerImpl::Show,
                     weak_ptr_factory_.GetWeakPtr()),
      kFilledCardInformationBubbleDelay);
}

void FilledCardInformationBubbleControllerImpl::ReshowBubble() {
  // If bubble is already visible, return early.
  if (bubble_view()) {
    return;
  }

  is_user_gesture_ = true;
  should_icon_be_visible_ = true;
  Show();
}

AutofillBubbleBase* FilledCardInformationBubbleControllerImpl::GetBubble()
    const {
  return bubble_view();
}

const FilledCardInformationBubbleOptions&
FilledCardInformationBubbleControllerImpl::GetBubbleOptions() const {
  return options_;
}

std::u16string
FilledCardInformationBubbleControllerImpl::GetCardIndicatorLabel() const {
  return options_.filled_card.record_type() ==
                 CreditCard::RecordType::kVirtualCard
             ? l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_VIRTUAL_CARD_LABEL)
             : options_.filled_card.AbbreviatedExpirationDateForDisplay(
                   /*with_prefix=*/false);
}

std::u16string FilledCardInformationBubbleControllerImpl::GetBubbleTitleText()
    const {
  if (IsBnplFlow()) {
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_BNPL_FILLED_CARD_INFORMATION_BUBBLE_TITLE,
        options_.filled_card.CardNameForAutofillDisplay());
  }
  return options_.filled_card.record_type() ==
                 CreditCard::RecordType::kVirtualCard
             ? l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_TITLE_VIRTUAL_CARD)
             : l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_TITLE);
}

std::u16string FilledCardInformationBubbleControllerImpl::GetLearnMoreLinkText()
    const {
  CHECK(!IsBnplFlow());
  return options_.filled_card.record_type() ==
                 CreditCard::RecordType::kVirtualCard
             ? l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_LEARN_MORE_LINK_LABEL_VIRTUAL_CARD)
             : l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_LEARN_MORE_LINK_LABEL);
}

std::u16string
FilledCardInformationBubbleControllerImpl::GetEducationalBodyLabel() const {
  if (IsBnplFlow()) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_BNPL_FILLED_CARD_INFORMATION_BUBBLE_EDUCATIONAL_BODY_LABEL);
  }
  return options_.filled_card.record_type() ==
                 CreditCard::RecordType::kVirtualCard
             ? l10n_util::GetStringFUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_EDUCATIONAL_BODY_LABEL_VIRTUAL_CARD,
                   GetLearnMoreLinkText())
             : l10n_util::GetStringFUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_EDUCATIONAL_BODY_LABEL,
                   GetLearnMoreLinkText());
}

std::u16string
FilledCardInformationBubbleControllerImpl::GetCardNumberFieldLabel() const {
  return options_.filled_card.record_type() ==
                 CreditCard::RecordType::kVirtualCard
             ? l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_CARD_NUMBER_LABEL_VIRTUAL_CARD)
             : l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_CARD_NUMBER_LABEL);
}

std::u16string
FilledCardInformationBubbleControllerImpl::GetExpirationDateFieldLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_EXP_DATE_LABEL_VIRTUAL_CARD);
}

std::u16string
FilledCardInformationBubbleControllerImpl::GetCardholderNameFieldLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_CARDHOLDER_NAME_LABEL_VIRTUAL_CARD);
}

std::u16string FilledCardInformationBubbleControllerImpl::GetCvcFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_CVC_LABEL_VIRTUAL_CARD);
}

std::u16string FilledCardInformationBubbleControllerImpl::GetValueForField(
    FilledCardInformationBubbleField field) const {
  switch (field) {
    case FilledCardInformationBubbleField::kCardNumber:
      return options_.filled_card.FullDigitsForDisplay();
    case FilledCardInformationBubbleField::kExpirationMonth:
      return options_.filled_card.Expiration2DigitMonthAsString();
    case FilledCardInformationBubbleField::kExpirationYear:
      return options_.filled_card.Expiration4DigitYearAsString();
    case FilledCardInformationBubbleField::kCardholderName:
      return options_.filled_card.GetRawInfo(CREDIT_CARD_NAME_FULL);
    case FilledCardInformationBubbleField::kCvc:
      return options_.cvc;
  }
}

std::u16string FilledCardInformationBubbleControllerImpl::GetFieldButtonTooltip(
    FilledCardInformationBubbleField field) const {
  return l10n_util::GetStringUTF16(
      clicked_field_ == field
          ? IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_BUTTON_TOOLTIP_CLICKED_VIRTUAL_CARD
          : IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_BUTTON_TOOLTIP_NORMAL_VIRTUAL_CARD);
}

bool FilledCardInformationBubbleControllerImpl::ShouldIconBeVisible() const {
  return should_icon_be_visible_;
}

void FilledCardInformationBubbleControllerImpl::OnLinkClicked() {
  web_contents()->OpenURL(
      content::OpenURLParams(GetLearnMoreUrl(), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void FilledCardInformationBubbleControllerImpl::OnBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  set_bubble_view(nullptr);

  // Log bubble result according to the closed reason.
  autofill_metrics::FilledCardInformationBubbleResult metric;
  switch (closed_reason) {
    case PaymentsUiClosedReason::kClosed:
      metric = autofill_metrics::FilledCardInformationBubbleResult::kClosed;
      break;
    case PaymentsUiClosedReason::kNotInteracted:
      metric =
          autofill_metrics::FilledCardInformationBubbleResult::kNotInteracted;
      break;
    default:
      metric = autofill_metrics::FilledCardInformationBubbleResult::kUnknown;
      break;
  }
  autofill_metrics::LogFilledCardInformationBubbleResultMetric(
      metric, is_user_gesture_);

  UpdatePageActionIcon();
}

void FilledCardInformationBubbleControllerImpl::OnFieldClicked(
    FilledCardInformationBubbleField field) {
  clicked_field_ = field;
  LogFilledCardInformationBubbleFieldClicked(field);
  // Strip the whitespaces that were added to the card number for legibility.
  UpdateClipboard(field == FilledCardInformationBubbleField::kCardNumber
                      ? StripCardNumberSeparators(GetValueForField(field))
                      : GetValueForField(field));
}

bool FilledCardInformationBubbleControllerImpl::ShouldShowGooglePayIconInTitle()
    const {
  return !IsBnplFlow();
}

std::u16string
FilledCardInformationBubbleControllerImpl::GetMaskedCardNameForDescriptionView()
    const {
  if (IsBnplFlow()) {
    return BnplIssuerIdToDisplayName(
        ConvertToBnplIssuerIdEnum(options_.filled_card.issuer_id()));
  }

  return options_.masked_card_name;
}

std::pair<ui::ImageModel, std::optional<ui::ImageModel>>
FilledCardInformationBubbleControllerImpl::GetCardImageForDescriptionView()
    const {
  if (!IsBnplFlow()) {
    return {ui::ImageModel::FromImage(options_.card_image), std::nullopt};
  }
  switch (ConvertToBnplIssuerIdEnum(options_.filled_card.issuer_id())) {
    case BnplIssuer::IssuerId::kBnplAffirm:
      return {ui::ImageModel::FromResourceId(IDR_AUTOFILL_AFFIRM_LINKED),
              ui::ImageModel::FromResourceId(IDR_AUTOFILL_AFFIRM_LINKED_DARK)};
    case BnplIssuer::IssuerId::kBnplZip:
      return {ui::ImageModel::FromResourceId(IDR_AUTOFILL_ZIP_LINKED),
              ui::ImageModel::FromResourceId(IDR_AUTOFILL_ZIP_LINKED_DARK)};
    // TODO(crbug.com/408268581): Handle Afterpay issuer enum value when adding
    // Afterpay to the BNPL flow.
    case BnplIssuer::IssuerId::kBnplAfterpay:
      return {ui::ImageModel::FromImage(options_.card_image), std::nullopt};
  }
  NOTREACHED();
}

bool FilledCardInformationBubbleControllerImpl::
    EducationalBodyHasLearnMoreLink() const {
  return !IsBnplFlow();
}

void FilledCardInformationBubbleControllerImpl::UpdateClipboard(
    const std::u16string& text) const {
  // TODO(crbug.com/40176273): Add metrics for user interaction with manual
  // fallback bubble UI elements.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(text);
}

void FilledCardInformationBubbleControllerImpl::
    LogFilledCardInformationBubbleFieldClicked(
        FilledCardInformationBubbleField field) const {
  autofill_metrics::FilledCardInformationBubbleFieldClicked metric;
  switch (field) {
    case FilledCardInformationBubbleField::kCardNumber:
      metric = autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kCardNumber;
      break;
    case FilledCardInformationBubbleField::kExpirationMonth:
      metric = autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kExpirationMonth;
      break;
    case FilledCardInformationBubbleField::kExpirationYear:
      metric = autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kExpirationYear;
      break;
    case FilledCardInformationBubbleField::kCardholderName:
      metric = autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kCardholderName;
      break;
    case FilledCardInformationBubbleField::kCvc:
      metric = autofill_metrics::FilledCardInformationBubbleFieldClicked::kCVC;
      break;
  }
  autofill_metrics::LogFilledCardInformationBubbleFieldClicked(metric);
}

FilledCardInformationBubbleControllerImpl::
    FilledCardInformationBubbleControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<FilledCardInformationBubbleControllerImpl>(
          *web_contents) {}

void FilledCardInformationBubbleControllerImpl::PrimaryPageChanged(
    content::Page& page) {
  should_icon_be_visible_ = false;
  bubble_has_been_shown_ = false;
  UpdatePageActionIcon();
  HideBubble();
}

void FilledCardInformationBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  // If the bubble hasn't been shown yet due to changing the tab during
  // kFilledCardInformationBubbleDelay, show the bubble after switching back
  // to the tab.
  if (visibility == content::Visibility::VISIBLE && !bubble_has_been_shown_ &&
      should_icon_be_visible_) {
    Show();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubble();
  }
}

PageActionIconType
FilledCardInformationBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kFilledCardInformation;
}

void FilledCardInformationBubbleControllerImpl::DoShowBubble() {
  if (!IsWebContentsActive()) {
    return;
  }

  // Cancel the posted task. This would be useful for cases where the user
  // clicks the icon during the delay.
  weak_ptr_factory_.InvalidateWeakPtrs();

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowFilledCardInformationBubble(web_contents(), this,
                                                        is_user_gesture_));
  DCHECK(bubble_view());
  bubble_has_been_shown_ = true;

  autofill_metrics::LogFilledCardInformationBubbleShown(is_user_gesture_);

  if (observer_for_test_) {
    observer_for_test_->OnBubbleShown();
  }
}

bool FilledCardInformationBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser) {
    return false;
  }

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

void FilledCardInformationBubbleControllerImpl::SetEventObserverForTesting(
    ObserverForTest* observer_for_test) {
  observer_for_test_ = observer_for_test;
}

GURL FilledCardInformationBubbleControllerImpl::GetLearnMoreUrl() const {
  return IsBnplFlow()
             ? autofill::payments::GetBnplTermsUrl(
                   ConvertToBnplIssuerIdEnum(options_.filled_card.issuer_id()))
             : autofill::payments::GetVirtualCardEnrollmentSupportUrl();
}

bool FilledCardInformationBubbleControllerImpl::IsBnplFlow() const {
  return options_.filled_card.is_bnpl_card();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FilledCardInformationBubbleControllerImpl);

}  // namespace autofill
