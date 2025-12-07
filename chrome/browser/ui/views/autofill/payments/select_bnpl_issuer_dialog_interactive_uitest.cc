// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/select_bnpl_issuer_dialog.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::autofill::autofill_metrics::SelectBnplIssuerDialogResult;

namespace {
constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests.";
}  // namespace

class SelectBnplIssuerDialogInteractiveUiTest : public InteractiveBrowserTest {
 public:
  SelectBnplIssuerDialogInteractiveUiTest() = default;
  SelectBnplIssuerDialogInteractiveUiTest(
      const SelectBnplIssuerDialogInteractiveUiTest&) = delete;
  SelectBnplIssuerDialogInteractiveUiTest& operator=(
      const SelectBnplIssuerDialogInteractiveUiTest&) = delete;
  ~SelectBnplIssuerDialogInteractiveUiTest() override = default;

  void TearDownOnMainThread() override {
    controller_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  InteractiveBrowserTestApi::MultiStep InvokeUiAndWaitForShow(
      std::vector<BnplIssuerContext> issuer_contexts,
      bool has_seen_ai_terms = false) {
    controller_ = std::make_unique<SelectBnplIssuerDialogControllerImpl>(
        autofill_client_injector_[web_contents()]->GetPaymentsAutofillClient());
    return Steps(
        ObserveState(
            views::test::kCurrentFocusedViewId,
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()),
        Do([this, issuer_contexts, has_seen_ai_terms]() {
          controller_->ShowDialog(
              base::BindOnce(&CreateAndShowBnplIssuerSelectionDialog,
                             controller_->GetWeakPtr(),
                             base::Unretained(web_contents()),
                             has_seen_ai_terms),
              std::move(issuer_contexts),
              /*app_locale=*/"en-US", accept_callback_.Get(),
              cancel_callback_.Get());
        }),
        InAnyContext(WaitForShow(views::DialogClientView::kTopViewId)));
  }

  BnplIssuerContext GetTestBnplIssuerContext(
      IssuerId issuer_id,
      BnplIssuerEligibilityForPage eligibility) {
    return BnplIssuerContext(test::GetTestLinkedBnplIssuer(issuer_id),
                             eligibility);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<SelectBnplIssuerDialogControllerImpl> controller_;
  base::MockOnceCallback<void(BnplIssuer)> accept_callback_;
  base::MockOnceClosure cancel_callback_;
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableAiBasedAmountExtraction};

 protected:
  PrefService& GetPrefs() {
    return *autofill_client_injector_[web_contents()]->GetPrefs();
  }

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest, InvokeUi) {
  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InAnyContext(
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(views::DialogClientView::kTopViewId,
                     /*screenshot_name=*/"select_bnpl_issuer_dialog",
                     /*baseline_cl=*/"6397923")));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       InvokeUi_BnplSelectionDialogShownLogged) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InSameContext(Steps(Check([&histogram_tester]() {
        return histogram_tester.GetBucketCount(
                   "Autofill.Bnpl.SelectionDialogShown",
                   /*sample=*/true) == 1;
      }))));
}

// Ensures the throbber is shown when `has_seen_ai_terms` is true.
IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       InvokeUi_WithThrobber) {
  RunTestSequence(
      // Invoke the dialog with `has_seen_ai_terms` set to true.
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)},
          /*has_seen_ai_terms=*/true),
      InAnyContext(
          // Verify the throbber is visible.
          WaitForShow(SelectBnplIssuerDialog::kThrobberId),
          // Verify the BNPL issuer view is NOT visible.
          EnsureNotPresent(SelectBnplIssuerDialog::kBnplIssuerView)));
}

// Ensures the throbber is dismissed and the issuer view is shown after
// UpdateWithIssuers is called.
IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       ThrobberDismissed_ShowIssuers) {
  RunTestSequence(
      // Start with the throbber showing.
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)},
          /*has_seen_ai_terms=*/true),
      InAnyContext(
          // Confirm throbber is showing and issuer view is hidden.
          WaitForShow(SelectBnplIssuerDialog::kThrobberId),
          EnsureNotPresent(SelectBnplIssuerDialog::kBnplIssuerView),
          // Simulate the completion of fetching issuer data.
          Do([this]() {
            controller_->UpdateDialogWithIssuers(
                {GetTestBnplIssuerContext(
                     IssuerId::kBnplAffirm,
                     BnplIssuerEligibilityForPage::kIsEligible),
                 GetTestBnplIssuerContext(
                     IssuerId::kBnplZip,
                     BnplIssuerEligibilityForPage::
                         kNotEligibleIssuerDoesNotSupportMerchant)});
          }),
          // Verify the throbber is now hidden.
          WaitForHide(SelectBnplIssuerDialog::kThrobberId),
          // Verify the BNPL issuer view is now visible.
          WaitForShow(SelectBnplIssuerDialog::kBnplIssuerView),
          // Ensure the dialog remains visible after the update.
          WaitForShow(views::DialogClientView::kTopViewId)));
}

// Ensures the throbber is shown after selecting an eligible BNPL issuer.
IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       EligibleBnplIssuerSelected) {
  std::string enabled_bnpl_issuer_hover_button_name =
      "Enabled BNPL Issuer hover button";
  EXPECT_CALL(accept_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(
          {{GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                     BnplIssuerEligibilityForPage::kIsEligible),
            GetTestBnplIssuerContext(
                IssuerId::kBnplZip,
                BnplIssuerEligibilityForPage::
                    kNotEligibleIssuerDoesNotSupportMerchant)}}),
      InAnyContext(
          NameViewRelative(SelectBnplIssuerDialog::kBnplIssuerView,
                           enabled_bnpl_issuer_hover_button_name,
                           [](views::View* hover_button_container) {
                             return hover_button_container->children()[0].get();
                           }),
          PressButton(enabled_bnpl_issuer_hover_button_name),
          WaitForShow(SelectBnplIssuerDialog::kThrobberId)));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       IneligibleBnplIssuerSelected) {
  std::string disabled_bnpl_issuer_hover_button_name =
      "Disabled BNPL Issuer hover button";
  EXPECT_CALL(accept_callback_, Run).Times(0);
  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InAnyContext(
          NameViewRelative(SelectBnplIssuerDialog::kBnplIssuerView,
                           disabled_bnpl_issuer_hover_button_name,
                           [](views::View* hover_button_container) {
                             // The 2nd issuer in the list is the one that will
                             // have a disabled state.
                             return hover_button_container->children()[1].get();
                           }),
          PressButton(disabled_bnpl_issuer_hover_button_name,
                      InputType::kMouse),
          EnsureNotPresent(SelectBnplIssuerDialog::kThrobberId)));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       DialogDeclined) {
  EXPECT_CALL(cancel_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InAnyContext(PressButton(views::DialogClientView::kCancelButtonElementId),
                   WaitForHide(views::DialogClientView::kTopViewId)));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       DialogAcceptedThenDeclined) {
  std::string enabled_bnpl_issuer_hover_button_name =
      "Enabled BNPL Issuer hover button";
  EXPECT_CALL(accept_callback_, Run);
  EXPECT_CALL(cancel_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InAnyContext(
          NameViewRelative(SelectBnplIssuerDialog::kBnplIssuerView,
                           enabled_bnpl_issuer_hover_button_name,
                           [](views::View* hover_button_container) {
                             return hover_button_container->children()[0].get();
                           }),
          PressButton(enabled_bnpl_issuer_hover_button_name),
          WaitForShow(SelectBnplIssuerDialog::kThrobberId),
          PressButton(views::DialogClientView::kCancelButtonElementId),
          WaitForHide(views::DialogClientView::kTopViewId)));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest, EscKeyPress) {
  EXPECT_CALL(cancel_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InAnyContext(
// Dialogs are already in focus for Mac builds and focusing again causes a
// button click.
#if !BUILDFLAG(IS_MAC)
          // Focus on an element in the dialog as the dialog's `kTopViewId`
          // view can't be focused on with `RequestFocus()`.
          WithView(views::DialogClientView::kCancelButtonElementId,
                   [](views::View* view) { view->RequestFocus(); }),
          WaitForState(views::test::kCurrentFocusedViewId,
                       views::DialogClientView::kCancelButtonElementId),
#endif
          SendAccelerator(views::DialogClientView::kTopViewId,
                          ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
          WaitForHide(views::DialogClientView::kTopViewId)));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       IssuerSelectedLogged) {
  base::HistogramTester histogram_tester;
  std::string enabled_bnpl_issuer_hover_button_name =
      "Enabled BNPL Issuer hover button";

  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InSameContext(Steps(
          NameViewRelative(SelectBnplIssuerDialog::kBnplIssuerView,
                           enabled_bnpl_issuer_hover_button_name,
                           [](views::View* hover_button_container) {
                             return hover_button_container->children()[0].get();
                           }),
          PressButton(enabled_bnpl_issuer_hover_button_name),
          WaitForShow(SelectBnplIssuerDialog::kThrobberId))));

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SelectionDialogResult",
      SelectBnplIssuerDialogResult::kIssuerSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SelectionDialogIssuerSelected", IssuerId::kBnplAffirm,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       CancelEventLogged) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InSameContext(
          Steps(PressButton(views::DialogClientView::kCancelButtonElementId),
                WaitForHide(views::DialogClientView::kTopViewId))));

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SelectionDialogResult",
      SelectBnplIssuerDialogResult::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       AccessibleWindowTitleIsSet) {
  const std::u16string expected_title =
      u"Choose a pay later provider. Google Pay logo";

  RunTestSequence(
      InvokeUiAndWaitForShow(
          {GetTestBnplIssuerContext(IssuerId::kBnplAffirm,
                                    BnplIssuerEligibilityForPage::kIsEligible),
           GetTestBnplIssuerContext(
               IssuerId::kBnplZip,
               BnplIssuerEligibilityForPage::
                   kNotEligibleIssuerDoesNotSupportMerchant)}),
      InSameContext(WithView(
          views::DialogClientView::kTopViewId,
          [expected_title](views::View* view) {
            views::Widget* widget = view->GetWidget();
            ASSERT_NE(widget, nullptr);
            EXPECT_EQ(widget->widget_delegate()->GetAccessibleWindowTitle(),
                      expected_title);
          })));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       AiNoteShownBoldOnFooterWhenNotSeen) {
  GetPrefs().SetBoolean(prefs::kAutofillAmountExtractionAiTermsSeen, false);

  RunTestSequence(
      InvokeUiAndWaitForShow({GetTestBnplIssuerContext(
          IssuerId::kBnplAffirm, BnplIssuerEligibilityForPage::kIsEligible)}),
      InSameContext(WithView(
          SelectBnplIssuerDialog::kFootnoteViewId, [](views::View* view) {
            ASSERT_TRUE(view);
            ASSERT_GT(view->children().size(), 0);
            views::StyledLabel* styled_label =
                views::AsViewClass<views::StyledLabel>(view->children()[0]);
            std::u16string bold_text = u"";

            // The bold text can be distributed in multiple children. Concat
            // bold text from all the children to get the full sentence.
            for (auto child : styled_label->children()) {
              views::Label* label_child =
                  views::AsViewClass<views::Label>(child);
              if (label_child != nullptr &&
                  label_child->GetTextStyle() ==
                      views::style::TextStyle::STYLE_EMPHASIZED) {
                if (bold_text != u"") {
                  bold_text.append(u" ");
                }
                bold_text.append(label_child->GetText());
              }
            }
            std::u16string expected_bold_text = l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_FOR_AI_AMOUNT_EXTRACTION_NOTE);
            // Verify the bold text matches the expected AI term.
            EXPECT_EQ(expected_bold_text, bold_text);
          })));
}

IN_PROC_BROWSER_TEST_F(SelectBnplIssuerDialogInteractiveUiTest,
                       AiNoteShownNotBoldOnFooterWhenSeen) {
  GetPrefs().SetBoolean(prefs::kAutofillAmountExtractionAiTermsSeen, true);

  RunTestSequence(
      InvokeUiAndWaitForShow({GetTestBnplIssuerContext(
          IssuerId::kBnplAffirm, BnplIssuerEligibilityForPage::kIsEligible)}),
      InSameContext(WithView(
          SelectBnplIssuerDialog::kFootnoteViewId, [](views::View* view) {
            ASSERT_TRUE(view);
            ASSERT_GT(view->children().size(), 0);
            views::StyledLabel* styled_label =
                views::AsViewClass<views::StyledLabel>(view->children()[0]);

            std::u16string ai_note_text = l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_FOR_AI_AMOUNT_EXTRACTION_NOTE);

            // Verify the AI note text is present in the footnote.
            EXPECT_TRUE(base::Contains(styled_label->GetText(), ai_note_text));

            // Verify that none of the parts of the styled label that make up
            // the AI note are bold.
            for (const auto& child : styled_label->children()) {
              const views::Label* label_child =
                  views::AsViewClass<views::Label>(child);
              if (label_child &&
                  base::Contains(ai_note_text, label_child->GetText())) {
                EXPECT_NE(label_child->GetTextStyle(),
                          views::style::TextStyle::STYLE_EMPHASIZED);
              }
            }
          })));
}

}  // namespace autofill::payments
