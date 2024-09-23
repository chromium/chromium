// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// The delay between card being fetched and manual fallback bubble being shown.
constexpr base::TimeDelta kManualFallbackBubbleDelay = base::Seconds(1.5);

// static
VirtualCardManualFallbackBubbleController*
VirtualCardManualFallbackBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  VirtualCardManualFallbackBubbleControllerImpl::CreateForWebContents(
      web_contents);
  return VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
      web_contents);
}

// static
VirtualCardManualFallbackBubbleController*
VirtualCardManualFallbackBubbleController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
      web_contents);
}

VirtualCardManualFallbackBubbleControllerImpl::
    ~VirtualCardManualFallbackBubbleControllerImpl() = default;

void VirtualCardManualFallbackBubbleControllerImpl::ShowBubble(
    const VirtualCardManualFallbackBubbleOptions& options) {
  // If another bubble is visible, dismiss it and show a new one since the card
  // information can be different.
  if (bubble_view())
    HideBubble();

  DCHECK(options.IsValid());
  options_ = options;
  is_user_gesture_ = false;
  should_icon_be_visible_ = true;

  // Delay the showing of the manual fallback bubble so that the form filling
  // and the manual fallback bubble appearance do not happen at the same time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VirtualCardManualFallbackBubbleControllerImpl::Show,
                     weak_ptr_factory_.GetWeakPtr()),
      kManualFallbackBubbleDelay);
}

void VirtualCardManualFallbackBubbleControllerImpl::ReshowBubble() {
  // If bubble is already visible, return early.
  if (bubble_view())
    return;

  is_user_gesture_ = true;
  should_icon_be_visible_ = true;
  Show();
}

AutofillBubbleBase* VirtualCardManualFallbackBubbleControllerImpl::GetBubble()
    const {
  return bubble_view();
}

const VirtualCardManualFallbackBubbleOptions&
VirtualCardManualFallbackBubbleControllerImpl::GetBubbleOptions() const {
  return options_;
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetVirtualCardIndicatorLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIRTUAL_CARD_LABEL);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetBubbleTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_TITLE);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetLearnMoreLinkText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_LEARN_MORE_LINK_LABEL);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetEducationalBodyLabel() const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_EDUCATIONAL_BODY_LABEL,
      GetLearnMoreLinkText());
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetVirtualCardNumberFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CARD_NUMBER_LABEL);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetExpirationDateFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_EXP_DATE_LABEL);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetCardholderNameFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CARDHOLDER_NAME_LABEL);
}

std::u16string VirtualCardManualFallbackBubbleControllerImpl::GetCvcFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CVC_LABEL);
}

std::u16string VirtualCardManualFallbackBubbleControllerImpl::GetValueForField(
    VirtualCardManualFallbackBubbleField field) const {
  switch (field) {
    case VirtualCardManualFallbackBubbleField::kCardNumber:
      return options_.virtual_card.FullDigitsForDisplay();
    case VirtualCardManualFallbackBubbleField::kExpirationMonth:
      return options_.virtual_card.Expiration2DigitMonthAsString();
    case VirtualCardManualFallbackBubbleField::kExpirationYear:
      return options_.virtual_card.Expiration4DigitYearAsString();
    case VirtualCardManualFallbackBubbleField::kCardholderName:
      return options_.virtual_card.GetRawInfo(CREDIT_CARD_NAME_FULL);
    case VirtualCardManualFallbackBubbleField::kCvc:
      return options_.virtual_card_cvc;
  }
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetFieldButtonTooltip(
    VirtualCardManualFallbackBubbleField field) const {
  return l10n_util::GetStringUTF16(
      clicked_field_ == field
          ? IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_BUTTON_TOOLTIP_CLICKED
          : IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_BUTTON_TOOLTIP_NORMAL);
}

bool VirtualCardManualFallbackBubbleControllerImpl::ShouldIconBeVisible()
    const {
  return should_icon_be_visible_;
}

void VirtualCardManualFallbackBubbleControllerImpl::OnLinkClicked(
    const GURL& url) {
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void VirtualCardManualFallbackBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);

  // Log bubble result according to the closed reason.
  autofill_metrics::VirtualCardManualFallbackBubbleResult metric;
  switch (closed_reason) {
    case PaymentsBubbleClosedReason::kClosed:
      metric = autofill_metrics::VirtualCardManualFallbackBubbleResult::kClosed;
      break;
    case PaymentsBubbleClosedReason::kNotInteracted:
      metric = autofill_metrics::VirtualCardManualFallbackBubbleResult::
          kNotInteracted;
      break;
    default:
      metric =
          autofill_metrics::VirtualCardManualFallbackBubbleResult::kUnknown;
      break;
  }
  autofill_metrics::LogVirtualCardManualFallbackBubbleResultMetric(
      metric, is_user_gesture_);

  UpdatePageActionIcon();
}

void VirtualCardManualFallbackBubbleControllerImpl::OnFieldClicked(
    VirtualCardManualFallbackBubbleField field) {
  clicked_field_ = field;
  LogVirtualCardManualFallbackBubbleFieldClicked(field);
  // Strip the whitespaces that were added to the card number for legibility.
  UpdateClipboard(field == VirtualCardManualFallbackBubbleField::kCardNumber
                      ? StripCardNumberSeparators(GetValueForField(field))
                      : GetValueForField(field));
}

void VirtualCardManualFallbackBubbleControllerImpl::UpdateClipboard(
    const std::u16string& text) const {
  // TODO(crbug.com/40176273): Add metrics for user interaction with manual
  // fallback bubble UI elements.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(text);
}

void VirtualCardManualFallbackBubbleControllerImpl::
    LogVirtualCardManualFallbackBubbleFieldClicked(
        VirtualCardManualFallbackBubbleField field) const {
  autofill_metrics::VirtualCardManualFallbackBubbleFieldClicked metric;
  switch (field) {
    case VirtualCardManualFallbackBubbleField::kCardNumber:
      metric = autofill_metrics::VirtualCardManualFallbackBubbleFieldClicked::
          kCardNumber;
      break;
    case VirtualCardManualFallbackBubbleField::kExpirationMonth:
      metric = autofill_metrics::VirtualCardManualFallbackBubbleFieldClicked::
          kExpirationMonth;
      break;
    case VirtualCardManualFallbackBubbleField::kExpirationYear:
      metric = autofill_metrics::VirtualCardManualFallbackBubbleFieldClicked::
          kExpirationYear;
      break;
    case VirtualCardManualFallbackBubbleField::kCardholderName:
      metric = autofill_metrics::VirtualCardManualFallbackBubbleFieldClicked::
          kCardholderName;
      break;
    case VirtualCardManualFallbackBubbleField::kCvc:
      metric =
          autofill_metrics::VirtualCardManualFallbackBubbleFieldClicked::kCVC;
      break;
  }
  autofill_metrics::LogVirtualCardManualFallbackBubbleFieldClicked(metric);
}

VirtualCardManualFallbackBubbleControllerImpl::
    VirtualCardManualFallbackBubbleControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<
          VirtualCardManualFallbackBubbleControllerImpl>(*web_contents) {}

void VirtualCardManualFallbackBubbleControllerImpl::PrimaryPageChanged(
    content::Page& page) {
  should_icon_be_visible_ = false;
  bubble_has_been_shown_ = false;
  UpdatePageActionIcon();
  HideBubble();
}

void VirtualCardManualFallbackBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  // If the bubble hasn't been shown yet due to changing the tab during
  // kManualFallbackBubbleDelay, show the bubble after switching back
  // to the tab.
  if (visibility == content::Visibility::VISIBLE && !bubble_has_been_shown_ &&
      should_icon_be_visible_) {
    Show();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubble();
  }
}

PageActionIconType
VirtualCardManualFallbackBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kVirtualCardManualFallback;
}

void VirtualCardManualFallbackBubbleControllerImpl::DoShowBubble() {
  if (!IsWebContentsActive())
    return;

  // Cancel the posted task. This would be useful for cases where the user
  // clicks the icon during the delay.
  weak_ptr_factory_.InvalidateWeakPtrs();

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowVirtualCardManualFallbackBubble(
                          web_contents(), this, is_user_gesture_));
  DCHECK(bubble_view());
  bubble_has_been_shown_ = true;

  autofill_metrics::LogVirtualCardManualFallbackBubbleShown(is_user_gesture_);

  if (observer_for_test_)
    observer_for_test_->OnBubbleShown();
}

bool VirtualCardManualFallbackBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser)
    return false;

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

void VirtualCardManualFallbackBubbleControllerImpl::SetEventObserverForTesting(
    ObserverForTest* observer_for_test) {
  observer_for_test_ = observer_for_test;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VirtualCardManualFallbackBubbleControllerImpl);

}  // namespace autofill
