// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views_browsertest_base.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "content/public/test/browser_test_utils.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/window/dialog_client_view.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif

using base::Bucket;
using testing::ElementsAre;

namespace autofill {

class SaveCardBubbleViewsFullFormBrowserTest
    : public SaveCardBubbleViewsBrowserTestBase {
 protected:
  SaveCardBubbleViewsFullFormBrowserTest()
      : SaveCardBubbleViewsBrowserTestBase(
            "/credit_card_upload_form_address_and_cc.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViewsFullFormBrowserTest);
};

class SaveCardBubbleViewsFullFormWithShippingBrowserTest
    : public SaveCardBubbleViewsBrowserTestBase {
 protected:
  SaveCardBubbleViewsFullFormWithShippingBrowserTest()
      : SaveCardBubbleViewsBrowserTestBase(
            "/credit_card_upload_form_shipping_address.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViewsFullFormWithShippingBrowserTest);
};

// Tests the local save bubble. Ensures that local save appears if the RPC to
// Google Payments fails unexpectedly.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Local_SubmittingFormShowsBubbleIfGetUploadDetailsRpcFails) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcServerError();

  // Submitting the form and having the call to Payments fail should show the
  // local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());
}

// Tests the local save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble) {
  // Disable the sign-in promo.
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Clicking [Save] should accept and close it.
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);

  // No bubble should be showing and no sign-in impression should have been
  // recorded.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_FALSE(GetSaveCardIconView()->visible());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}

// Tests the sign in promo bubble. Ensures that clicking the [Save] button
// on the local save bubble successfully causes the sign in promo to show.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingSaveShowsSigninPromo) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Sign-in promo should be showing and user actions should have recorded
  // impression.
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::SIGN_IN_PROMO_VIEW)->visible());
}
#endif

// Tests the sign in promo bubble. Ensures that the sign-in promo
// is not shown when the user is signed-in and syncing, even if
// the local save bubble is shown.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_NoSigninPromoShowsWhenUserIsSyncing) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Sign the user in.
  SignInWithFullName("John Smith");

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Click [Save] should close the offer-to-save bubble
  // but no sign-in promo should show because user is signed in.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing and no sign-in impression should have been
  // recorded.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}

// Tests the sign in promo bubble. Ensures that signin impression is recorded
// when promo is shown.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_SigninImpressionSigninPromo) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // User actions should have recorded impression.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}
#endif

// Tests the sign in promo bubble. Ensures that signin action is recorded when
// user accepts promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_AcceptingSigninPromo) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Click on [Sign in] button.
  ClickOnDialogView(static_cast<DiceBubbleSyncPromoView*>(
                        FindViewInBubbleById(DialogViewId::SIGN_IN_VIEW))
                        ->GetSigninButtonForTesting());

  // User actions should have recorded impression and click.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromSaveCardBubble"));
}
#endif

// Tests the manage cards bubble. Ensures that it shows up by clicking the
// credit card icon.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingIconShowsManageCards) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Bubble should be showing.
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MANAGE_CARDS_VIEW)->visible());
  histogram_tester.ExpectUniqueSample("Autofill.ManageCardsPrompt.Local",
                                      AutofillMetrics::MANAGE_CARDS_SHOWN, 1);
}

// Tests the manage cards bubble. Ensures that clicking the [Manage cards]
// button redirects properly.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ManageCardsButtonRedirects) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on the redirect button.
  ClickOnDialogViewWithId(DialogViewId::MANAGE_CARDS_BUTTON);

#if defined(OS_CHROMEOS)
  // ChromeOS should have opened up the settings window.
  EXPECT_TRUE(
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile()));
#else
  // Otherwise, another tab should have opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
#endif

  // Metrics should have been recorded correctly.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, 1)));
}

// Tests the manage cards bubble. Ensures that clicking the [Done]
// button closes the bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ManageCardsDoneButtonClosesBubble) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on the [Done] button.
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing now and metrics should be recorded correctly.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_DONE, 1)));
}

// Tests the manage cards bubble. Ensures that sign-in impression is recorded
// correctly.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_SigninImpressionManageCards) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close promo.
  ClickOnCloseButton();

  // Open up Manage Cards prompt.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // User actions should have recorded impression.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromManageCardsBubble"));
}
#endif

// Tests the Manage Cards bubble. Ensures that signin action is recorded when
// user accepts footnote promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_AcceptingFootnotePromoManageCards) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close promo.
  ClickOnCloseButton();

  // Open up Manage Cards prompt.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on [Sign in] button in footnote.
  ClickOnDialogView(static_cast<DiceBubbleSyncPromoView*>(
                        FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW))
                        ->GetSigninButtonForTesting());

  // User actions should have recorded impression and click.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromManageCardsBubble"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Signin_FromManageCardsBubble"));
}
#endif

// Tests the local save bubble. Ensures that the Harmony version of the bubble
// does not have a [No thanks] button (it has an [X] Close button instead.)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ShouldNotHaveNoThanksButton) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Assert that the cancel button cannot be found.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON));
}

// Tests the local save bubble. Ensures that the bubble behaves correctly if
// dismissed and then immediately torn down (e.g. by closing browser window)
// before the asynchronous close completes. Regression test for
// https://crbug.com/842577 .
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_SynchronousCloseAfterAsynchronousClose) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();

  SaveCardBubbleViews* bubble = GetSaveCardBubbleViews();
  EXPECT_TRUE(bubble);
  views::Widget* bubble_widget = bubble->GetWidget();
  EXPECT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());
  bubble->Hide();
  EXPECT_FALSE(bubble_widget->IsVisible());

  // The bubble is immediately hidden, but it can still receive events here.
  // Simulate an OS event arriving to destroy the Widget.
  bubble_widget->CloseNow();
  // |bubble| and |bubble_widget| now point to deleted objects.

  // Simulate closing the browser window.
  browser()->tab_strip_model()->CloseAllTabs();

  // Process the asynchronous close (which should do nothing).
  base::RunLoop().RunUntilIdle();
}

// Tests the upload save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away and sends an UploadCardRequest RPC
// to Google Payments.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ClickingSaveClosesBubble) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking [Save] should accept and close it, then send an UploadCardRequest
  // to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// Tests the upload save bubble. Ensures that the Harmony version of the bubble
// does not have a [No thanks] button (it has an [X] Close button instead.)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ShouldNotHaveNoThanksButton) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Assert that the cancel button cannot be found.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON));
}

// Tests the upload save bubble. Ensures that clicking the top-right [X] close
// button successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ClickingCloseClosesBubble) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();
}

// Tests the upload save bubble. Ensures that the bubble does not surface the
// cardholder name textfield if it is not needed.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ShouldNotRequestCardholderNameInHappyPath) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Assert that cardholder name was not explicitly requested in the bubble.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithMissingNamesRequestsCardholderNameIfExpOn) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is conflicting.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormWithShippingBrowserTest,
    Upload_SubmittingFormWithConflictingNamesRequestsCardholderNameIfExpOn) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submit first shipping address form with a conflicting name.
  FillAndSubmitFormWithConflictingName();

  // Submitting the second form should still show the upload save bubble and
  // legal footer, along with a textfield requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that if the cardholder name textfield
// is empty, the user is not allowed to click [Save] and close the dialog.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SaveButtonIsDisabledIfNoCardholderNameAndCardholderNameRequested) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clearing out the cardholder name textfield should disable the [Save]
  // button.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(base::ASCIIToUTF16(""));
  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  // Setting a cardholder name should enable the [Save] button.
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("John Smith"));
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_NORMAL);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, filling it and clicking [Save] closes the dialog.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_EnteringCardholderNameAndClickingSaveClosesBubbleIfCardholderNameRequested) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Entering a cardholder name and clicking [Save] should accept and close
  // the bubble, then send an UploadCardRequest to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("John Smith"));
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance for both regular save UMA and
  // the ".RequestingCardholderName" subhistogram.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, it is prefilled with the name from the user's Google Account.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_RequestedCardholderNameTextfieldIsPrefilledWithFocusName) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Sign the user in.
  SignInWithFullName("John Smith");

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  base::HistogramTester histogram_tester;
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be prefilled with the name on the user's Google
  // Account, and UMA should have logged its value's existence. Because the
  // textfield has a value, the tooltip explaining that the name came from the
  // user's Google Account should also be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_EQ(cardholder_name_textfield->text(),
            base::ASCIIToUTF16("John Smith"));
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", true, 1);
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested but the name on the user's Google Account is unable to be fetched
// for any reason, the textfield is left blank.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_RequestedCardholderNameTextfieldIsNotPrefilledWithFocusNameIfMissing) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Don't sign the user in. In this case, the user's Account cannot be fetched
  // and their name is not available.

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  base::HistogramTester histogram_tester;
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be blank, and UMA should have logged its value's
  // absence. Because the textfield is blank, the tooltip explaining that the
  // name came from the user's Google Account should NOT be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_TRUE(cardholder_name_textfield->text().empty());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", false, 1);
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog without changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_CardholderNameRequested_SubmittingPrefilledValueLogsUneditedMetric) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Sign the user in.
  SignInWithFullName("John Smith");

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clicking [Save] should accept and close the bubble, logging that the name
  // was not edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", false, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog after changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_CardholderNameRequested_SubmittingChangedValueLogsEditedMetric) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Sign the user in.
  SignInWithFullName("John Smith");

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Changing the name then clicking [Save] should accept and close the bubble,
  // logging that the name was edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("Jane Doe"));
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", true, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested but the AutofillUpstreamBlankCardholderNameField experiment is
// active, the textfield is NOT prefilled even though the user's Google Account
// name is available.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_CardholderNameNotPrefilledIfBlankNameExperimentEnabled) {
  // Enable the EditableCardholderName and BlankCardholderNameField experiments.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstreamEditableCardholderName,
       features::kAutofillUpstreamBlankCardholderNameField},
      // Disabled
      {});

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Sign the user in.
  SignInWithFullName("John Smith");

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  base::HistogramTester histogram_tester;
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be blank, and UMA should have logged its value's
  // absence. Because the textfield is blank, the tooltip explaining that the
  // name came from the user's Google Account should NOT be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_TRUE(cardholder_name_textfield->text().empty());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", false, 1);
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// TODO(jsaul): Only *part* of the legal message StyledLabel is clickable, and
//              the NOTREACHED() in SaveCardBubbleViews::StyledLabelLinkClicked
//              prevents us from being able to click it unless we know the exact
//              gfx::Range of the link. When/if that can be worked around,
//              create an Upload_ClickingTosLinkClosesBubble test.

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and offers to upload save the card if Payments allows it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Logic_CanOfferToSaveEvenIfNothingFoundIfPaymentsAccepts) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form, even with only card number and expiration date, should
  // start the flow of asking Payments if Chrome should offer to save the card
  // to Google. In this case, Payments says yes, and the offer to save bubble
  // should be shown.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithCardDetailsOnly();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and still does not surface the offer to upload save dialog
// if Payments declines it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Logic_ShouldNotOfferToSaveIfNothingFoundAndPaymentsDeclines) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form, even with only card number and expiration date, should
  // start the flow of asking Payments if Chrome should offer to save the card
  // to Google. In this case, Payments says no, so the offer to save bubble
  // should not be shown.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithCardDetailsOnly();
  WaitForObservedEvent();
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if CVC is not detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfCvcNotFound) {
  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though CVC is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitFormWithoutCvc();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if the detected CVC is invalid.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfInvalidCvcFound) {
  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the provided
  // CVC is invalid.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitFormWithInvalidCvc();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if address/cardholder name is not
// detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfNameNotFound) {
  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though name is
  // missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting names are
// detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormWithShippingBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfNamesConflict) {
  // Submit first shipping address form with a conflicting name.
  FillAndSubmitFormWithConflictingName();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the name
  // conflicts with the previous form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if billing address is not detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfAddressNotFound) {
  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though billing address
  // is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitFormWithoutAddress();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting billing address
// postal codes are detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormWithShippingBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfPostalCodesConflict) {
  // Submit first shipping address form with a conflicting postal code.
  FillAndSubmitFormWithConflictingPostalCode();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the postal code
  // conflicts with the previous form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
}

// Tests UMA logging for the upload save bubble. Ensures that if the user
// declines upload, Autofill.UploadAcceptedCardOrigin is not logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_DecliningUploadDoesNotLogUserAcceptedCardOriginUMA) {
  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Ensure that UMA was logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadOfferedCardOrigin",
      AutofillMetrics::OFFERING_UPLOAD_OF_NEW_CARD, 1);
  histogram_tester.ExpectTotalCount("Autofill.UploadAcceptedCardOrigin", 0);
}

}  // namespace autofill
