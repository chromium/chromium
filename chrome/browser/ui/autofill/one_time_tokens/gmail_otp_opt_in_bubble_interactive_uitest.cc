// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/one_time_tokens/gmail_otp_opt_in_bubble_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/one_time_tokens/gmail_otp_opt_in_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace autofill {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTabId);
constexpr char kLearnMoreLinkName[] = "LearnMoreLink";
constexpr std::u16string_view kTestEmail = u"elisa.g.beckett@gmail.com";

class OptInResultObserver
    : public ui::test::StateObserver<std::optional<GmailOtpOptInResult>> {
 public:
  base::OnceCallback<void(GmailOtpOptInResult)> GetCallback() {
    return base::BindOnce(&OptInResultObserver::SetResult,
                          weak_ptr_factory_.GetWeakPtr());
  }

 private:
  void SetResult(GmailOtpOptInResult result) {
    OnStateObserverStateChanged(result);
  }

  base::WeakPtrFactory<OptInResultObserver> weak_ptr_factory_{this};
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(OptInResultObserver, kOptInResultState);

class GmailOtpOptInBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  GmailOtpOptInBubbleInteractiveUiTest() = default;

 protected:
  ChromeAutofillClient* autofill_client() const {
    return ChromeAutofillClient::FromWebContentsForTesting(
        browser()->GetActiveTabInterface()->GetContents());
  }

  GmailOtpOptInBubbleController* controller() const {
    return GmailOtpOptInBubbleController::From(
        *browser()->GetActiveTabInterface());
  }

  MultiStep ShowBubble() {
    auto observer = std::make_unique<OptInResultObserver>();
    base::OnceCallback<void(GmailOtpOptInResult)> callback =
        observer->GetCallback();
    return Steps(
        ObserveState(kOptInResultState, std::move(observer)),
        CheckResult([this]() { return controller()->IsShowingBubble(); },
                    false),
        Do([this, callback = std::move(callback)]() mutable {
          autofill_client()->ShowGmailOtpOptInBubble(std::u16string(kTestEmail),
                                                     std::move(callback));
        }),
        CheckResult([this]() { return controller()->IsShowingBubble(); }, true),
        CheckState(kOptInResultState, std::nullopt));
  }

  MultiStep EnsureClosedWithResult(GmailOtpOptInResult expected_result) {
    return Steps(
        WaitForState(kOptInResultState, expected_result),
        CheckResult([this]() { return controller()->IsShowingBubble(); },
                    false));
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillShowGmailOtpSuggestions};
};

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest,
                       ShowBubbleAndVerifyElementsAndAnchoring) {
  auto get_accessible_name = [](views::View* view) {
    return view->GetViewAccessibility().GetCachedName();
  };

  RunTestSequence(
      ShowBubble(), WaitForShow(GmailOtpOptInBubbleView::kTurnOnButtonId),
      EnsurePresent(GmailOtpOptInBubbleView::kNoThanksButtonId),
      EnsurePresent(GmailOtpOptInBubbleView::kCloseButtonId),
      EnsurePresent(GmailOtpOptInBubbleView::kDescriptionLabelId),
      NameChildViewByType<views::Link>(
          GmailOtpOptInBubbleView::kDescriptionLabelId, kLearnMoreLinkName),
      CheckViewProperty(GmailOtpOptInBubbleView::kTurnOnButtonId,
                        &views::View::GetEnabled, true),
      CheckViewProperty(GmailOtpOptInBubbleView::kNoThanksButtonId,
                        &views::View::GetEnabled, true),
      CheckViewProperty(GmailOtpOptInBubbleView::kCloseButtonId,
                        &views::View::GetEnabled, true),
      CheckViewProperty(GmailOtpOptInBubbleView::kTurnOnButtonId,
                        &views::LabelButton::GetText,
                        l10n_util::GetStringUTF16(
                            IDS_AUTOFILL_GMAIL_OTP_OPT_IN_TURN_ON_BUTTON)),
      CheckViewProperty(GmailOtpOptInBubbleView::kNoThanksButtonId,
                        &views::LabelButton::GetText,
                        l10n_util::GetStringUTF16(
                            IDS_AUTOFILL_GMAIL_OTP_OPT_IN_NO_THANKS_BUTTON)),
      CheckViewProperty(
          GmailOtpOptInBubbleView::kDescriptionLabelId,
          &views::StyledLabel::GetText,
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_GMAIL_OTP_OPT_IN_DESCRIPTION,
              std::u16string(kTestEmail),
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_GMAIL_OTP_OPT_IN_LEARN_MORE_LINK))),
      CheckView(GmailOtpOptInBubbleView::kTurnOnButtonId, get_accessible_name,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_GMAIL_OTP_OPT_IN_TURN_ON_BUTTON)),
      CheckView(GmailOtpOptInBubbleView::kNoThanksButtonId, get_accessible_name,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_GMAIL_OTP_OPT_IN_NO_THANKS_BUTTON)),
      CheckView(GmailOtpOptInBubbleView::kCloseButtonId, get_accessible_name,
                l10n_util::GetStringUTF16(IDS_APP_CLOSE)),
      CheckView(kLearnMoreLinkName, get_accessible_name,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_GMAIL_OTP_OPT_IN_LEARN_MORE_A11Y_NAME)),
      CheckView(
          GmailOtpOptInBubbleView::kTurnOnButtonId,
          [](views::View* turn_on_button) -> views::View* {
            views::BubbleDialogDelegate* bubble_delegate =
                turn_on_button->GetWidget()
                    ->widget_delegate()
                    ->AsBubbleDialogDelegate();
            return bubble_delegate ? bubble_delegate->GetAnchorView() : nullptr;
          },
          BrowserView::GetBrowserViewForBrowser(browser())
              ->GetLocationBarView()));
}

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest, AcceptBubble) {
  RunTestSequence(ShowBubble(),
                  WaitForShow(GmailOtpOptInBubbleView::kTurnOnButtonId),
                  PressButton(GmailOtpOptInBubbleView::kTurnOnButtonId),
                  WaitForHide(GmailOtpOptInBubbleView::kTurnOnButtonId),
                  EnsureClosedWithResult(GmailOtpOptInResult::kAccepted));
}

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest, DeclineBubble) {
  RunTestSequence(ShowBubble(),
                  WaitForShow(GmailOtpOptInBubbleView::kNoThanksButtonId),
                  PressButton(GmailOtpOptInBubbleView::kNoThanksButtonId),
                  WaitForHide(GmailOtpOptInBubbleView::kNoThanksButtonId),
                  EnsureClosedWithResult(GmailOtpOptInResult::kDeclined));
}

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest, CloseBubble) {
  RunTestSequence(ShowBubble(),
                  WaitForShow(GmailOtpOptInBubbleView::kCloseButtonId),
                  PressButton(GmailOtpOptInBubbleView::kCloseButtonId),
                  WaitForHide(GmailOtpOptInBubbleView::kCloseButtonId),
                  EnsureClosedWithResult(GmailOtpOptInResult::kClosed));
}

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest,
                       EscapeKeyClosesBubble) {
  RunTestSequence(
      ShowBubble(), WaitForShow(GmailOtpOptInBubbleView::kTurnOnButtonId),
      ActivateSurface(GmailOtpOptInBubbleView::kTurnOnButtonId),
      SendAccelerator(GmailOtpOptInBubbleView::kTurnOnButtonId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForHide(GmailOtpOptInBubbleView::kTurnOnButtonId),
      EnsureClosedWithResult(GmailOtpOptInResult::kClosed));
}

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest,
                       LearnMoreLinkOpensSettingsAndReshowsOnTabSwitch) {
  auto is_showing_bubble = [initial_controller = controller()]() {
    return initial_controller->IsShowingBubble();
  };

  RunTestSequence(
      ShowBubble(), WaitForShow(GmailOtpOptInBubbleView::kDescriptionLabelId),
      InstrumentNextTab(kSettingsTabId),
      NameChildViewByType<views::Link>(
          GmailOtpOptInBubbleView::kDescriptionLabelId, kLearnMoreLinkName),
      DoDefaultAction(kLearnMoreLinkName),
      WaitForHide(GmailOtpOptInBubbleView::kTurnOnButtonId),
      WaitForWebContentsReady(
          kSettingsTabId, chrome::GetSettingsUrl(chrome::kContactInfoSubPage)),
      CheckResult(is_showing_bubble, false),
      CheckState(kOptInResultState, std::nullopt),
      SelectTab(kTabStripElementId, 0),
      WaitForShow(GmailOtpOptInBubbleView::kTurnOnButtonId),
      CheckResult(is_showing_bubble, true),
      CheckState(kOptInResultState, std::nullopt));
}

IN_PROC_BROWSER_TEST_F(GmailOtpOptInBubbleInteractiveUiTest,
                       TabClosureDiscardsBubble) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTabId, GURL(url::kAboutBlankURL)), ShowBubble(),
      WaitForShow(GmailOtpOptInBubbleView::kTurnOnButtonId), Do([this]() {
        browser()->GetTabStripModel()->CloseWebContentsAt(
            browser()->GetTabStripModel()->active_index(),
            TabCloseTypes::CLOSE_NONE);
      }),
      WaitForHide(GmailOtpOptInBubbleView::kTurnOnButtonId),
      WaitForState(kOptInResultState, std::optional<GmailOtpOptInResult>(
                                          GmailOtpOptInResult::kDiscarded)));
}

}  // namespace
}  // namespace autofill
