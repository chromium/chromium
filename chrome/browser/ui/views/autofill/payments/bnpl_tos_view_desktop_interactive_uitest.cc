// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_view_desktop.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_modifiers.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {
using autofill_metrics::BnplTosDialogResult;

namespace {
constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests.";
}  // namespace

class BnplTosViewDesktopInteractiveUiTest : public InteractiveBrowserTest {
 public:
  BnplTosViewDesktopInteractiveUiTest() = default;
  BnplTosViewDesktopInteractiveUiTest(
      const BnplTosViewDesktopInteractiveUiTest&) = delete;
  BnplTosViewDesktopInteractiveUiTest& operator=(
      const BnplTosViewDesktopInteractiveUiTest&) = delete;
  ~BnplTosViewDesktopInteractiveUiTest() override = default;

  InteractiveBrowserTestApi::MultiStep InvokeUiAndWaitForShow(
      BnplIssuer::IssuerId bnpl_issuer_id) {
    return Steps(
        ObserveState(
            views::test::kCurrentFocusedViewId,
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()),
        Do([this, bnpl_issuer_id]() {
          payments::BnplTosModel model;
          model.issuer = BnplIssuer(
              /*instrument_id=*/std::nullopt, bnpl_issuer_id,
              std::vector<BnplIssuer::EligiblePriceRange>{});
          LegalMessageLine::Parse(
              base::JSONReader::Read(
                  "{ \"line\" : [ { \"template\": \"This is a legal message "
                  "with"
                  "{0}.\", \"template_parameter\": [ { \"display_text\": "
                  "\"a link\", \"url\": \"http://www.example.com/\" "
                  "} ] }] }",
                  base::JSON_PARSE_CHROMIUM_EXTENSIONS)
                  ->GetDict(),
              &model.legal_message_lines, true);

          ContentAutofillClient::FromWebContents(web_contents())
              ->GetPaymentsAutofillClient()
              ->GetBnplUiDelegate()
              ->ShowBnplTosUi(std::move(model), accept_callback_.Get(),
                              cancel_callback_.Get());
        }),
        InAnyContext(WaitForShow(views::DialogClientView::kTopViewId)));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::MockOnceClosure accept_callback_;
  base::MockOnceClosure cancel_callback_;
};

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, InvokeUi) {
  RunTestSequence(InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
                  InAnyContext(SetOnIncompatibleAction(
                                   OnIncompatibleAction::kIgnoreAndContinue,
                                   kSuppressedScreenshotError),
                               Screenshot(views::DialogClientView::kTopViewId,
                                          /*screenshot_name=*/"bnpl_tos",
                                          /*baseline_cl=*/"6318763")));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, DialogAccepted) {
  EXPECT_CALL(accept_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InAnyContext(PressButton(views::DialogClientView::kOkButtonElementId),
                   WaitForShow(BnplTosDialog::kThrobberId)));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, DialogShownLogged) {
  base::HistogramTester histogram_tester;
  RunTestSequence(InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
                  InSameContext(Check([&histogram_tester]() {
                    return histogram_tester.GetBucketCount(
                               "Autofill.Bnpl.TosDialogShown.Affirm",
                               /*sample=*/true) == 1;
                  })));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       DialogAcceptedTwice) {
  EXPECT_CALL(accept_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InAnyContext(PressButton(views::DialogClientView::kOkButtonElementId),
                   WaitForShow(BnplTosDialog::kThrobberId),
                   PressButton(views::DialogClientView::kOkButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, DialogDeclined) {
  EXPECT_CALL(cancel_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InAnyContext(PressButton(views::DialogClientView::kCancelButtonElementId),
                   WaitForHide(views::DialogClientView::kTopViewId)));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       DialogAcceptedThenDeclined) {
  EXPECT_CALL(accept_callback_, Run);
  EXPECT_CALL(cancel_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InAnyContext(PressButton(views::DialogClientView::kOkButtonElementId),
                   WaitForShow(BnplTosDialog::kThrobberId)),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(views::DialogClientView::kTopViewId));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, EscKeyPress) {
  EXPECT_CALL(cancel_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InAnyContext(
// Dialogs are already in focus for Mac builds and focusing again causes a
// button click.
#if !BUILDFLAG(IS_MAC)
          // Focus on an element in the dialog as the dialog's `kTopViewId`
          // view can't be focused on with `RequestFocus()`.
          WithView(views::DialogClientView::kOkButtonElementId,
                   [](views::View* view) { view->RequestFocus(); }),
          WaitForState(views::test::kCurrentFocusedViewId,
                       views::DialogClientView::kOkButtonElementId),
#endif
          SendAccelerator(views::DialogClientView::kTopViewId,
                          ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
          WaitForHide(views::DialogClientView::kTopViewId)));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       DialogLoggedWithAcceptButtonClicked) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InSameContext(Steps(
          PressButton(views::DialogClientView::kOkButtonElementId),
          WaitForShow(BnplTosDialog::kThrobberId), Check([&histogram_tester]() {
            return histogram_tester.GetBucketCount(
                       "Autofill.Bnpl.TosDialogResult.Affirm",
                       BnplTosDialogResult::kAcceptButtonClicked) == 1;
          }))));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       DialogLoggedWithCancelButtonClicked) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InSameContext(
          Steps(PressButton(views::DialogClientView::kCancelButtonElementId),
                WaitForHide(views::DialogClientView::kTopViewId),
                Check([&histogram_tester]() {
                  return histogram_tester.GetBucketCount(
                             "Autofill.Bnpl.TosDialogResult.Affirm",
                             BnplTosDialogResult::kCancelButtonClicked) == 1;
                }))));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       AccessibleWindowTitleIsSet_Affirm) {
  const std::u16string expected_title =
      u"Link account and pay with Affirm? Google Pay, Affirm logo";

  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),
      InSameContext(WithView(
          views::DialogClientView::kTopViewId,
          [expected_title](views::View* view) {
            views::Widget* widget = view->GetWidget();
            ASSERT_NE(widget, nullptr);
            EXPECT_EQ(widget->widget_delegate()->GetAccessibleWindowTitle(),
                      expected_title);
          })));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       AccessibleWindowTitleIsSet_Zip) {
  const std::u16string expected_title =
      u"Link account and pay with Zip? Google Pay, Zip logo";

  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplZip),
      InSameContext(WithView(
          views::DialogClientView::kTopViewId,
          [expected_title](views::View* view) {
            views::Widget* widget = view->GetWidget();
            ASSERT_NE(widget, nullptr);
            EXPECT_EQ(widget->widget_delegate()->GetAccessibleWindowTitle(),
                      expected_title);
          })));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       AccessibleWindowTitleIsSet_Klarna) {
  const std::u16string expected_title =
      u"Link account and pay with Klarna? Google Pay, Klarna logo";

  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplKlarna),
      InSameContext(WithView(
          views::DialogClientView::kTopViewId,
          [expected_title](views::View* view) {
            views::Widget* widget = view->GetWidget();
            ASSERT_NE(widget, nullptr);
            EXPECT_EQ(widget->widget_delegate()->GetAccessibleWindowTitle(),
                      expected_title);
          })));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       AccessibleWindowTitleIsSet_AfterPay) {
  const std::u16string expected_title =
      u"Link account and pay with AfterPay? Google Pay, AfterPay logo";

  RunTestSequence(
      InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAfterpay),
      InSameContext(WithView(
          views::DialogClientView::kTopViewId,
          [expected_title](views::View* view) {
            views::Widget* widget = view->GetWidget();
            ASSERT_NE(widget, nullptr);
            EXPECT_EQ(widget->widget_delegate()->GetAccessibleWindowTitle(),
                      expected_title);
          })));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest,
                       DialogResultLoggedWhenTabClosed) {
  base::HistogramTester histogram_tester;

  RunTestSequence(InvokeUiAndWaitForShow(BnplIssuer::IssuerId::kBnplAffirm),

                  // Close the active tab.
                  Do([this]() {
                    browser()->tab_strip_model()->CloseWebContentsAt(
                        browser()->tab_strip_model()->active_index(),
                        TabCloseTypes::CLOSE_USER_GESTURE);
                  }),

                  Check([&histogram_tester]() {
                    return histogram_tester.GetBucketCount(
                               "Autofill.Bnpl.TosDialogResult.Affirm",
                               BnplTosDialogResult::kTabOrBrowserClosed) == 1;
                  }));
}

}  // namespace autofill
