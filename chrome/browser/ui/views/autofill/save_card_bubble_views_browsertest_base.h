// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_BUBBLE_VIEWS_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_BUBBLE_VIEWS_BROWSERTEST_BASE_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/credit_card_save_manager.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {
class ScopedGeolocationOverrider;
}

namespace autofill {

// Base class for any interactive SaveCardBubbleViews browser test that will
// need to show and interact with the offer-to-save bubble.
class SaveCardBubbleViewsBrowserTestBase
    : public InProcessBrowserTest,
      public CreditCardSaveManager::ObserverForTest,
      public SaveCardBubbleControllerImpl::ObserverForTest {
 public:
  // Various events that can be waited on by the DialogEventWaiter.
  enum DialogEvent : int {
    OFFERED_LOCAL_SAVE,
    REQUESTED_UPLOAD_SAVE,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_UPLOAD_CARD_REQUEST,
    RECEIVED_UPLOAD_CARD_RESPONSE,
    STRIKE_CHANGE_COMPLETE,
    BUBBLE_SHOWN,
    BUBBLE_CLOSED
  };

 protected:
  // Test will open a browser window to |test_file_path| (relative to
  // components/test/data/autofill).
  explicit SaveCardBubbleViewsBrowserTestBase(
      const std::string& test_file_path);
  ~SaveCardBubbleViewsBrowserTestBase() override;

  void SetUpOnMainThread() override;

  void NavigateTo(const std::string& file_path);

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferLocalSave() override;
  void OnDecideToRequestUploadSave() override;
  void OnReceivedGetUploadDetailsResponse() override;
  void OnSentUploadCardRequest() override;
  void OnReceivedUploadCardResponse() override;
  void OnCCSMStrikeChangeComplete() override;

  // SaveCardBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override;
  void OnBubbleClosed() override;
  void OnSCBCStrikeChangeComplete() override;

  // BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override;

  // Sets up the ability to sign in the user.
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  // Signs in the user with the provided |full_name|.
  void SignInWithFullName(const std::string& full_name);

  // Will call JavaScript to fill and submit the form in different ways.
  void SubmitForm();
  void FillAndSubmitForm();
  void FillAndSubmitFormWithCardDetailsOnly();
  void FillAndSubmitFormWithoutCvc();
  void FillAndSubmitFormWithInvalidCvc();
  void FillAndSubmitFormWithoutName();
  void FillAndSubmitFormWithConflictingName();
  void FillAndSubmitFormWithoutAddress();
  void FillAndSubmitFormWithConflictingStreetAddress();
  void FillAndSubmitFormWithConflictingPostalCode();

  // For setting up Payments RPCs.
  void SetUploadDetailsRpcPaymentsAccepts();
  void SetUploadDetailsRpcPaymentsDeclines();
  void SetUploadDetailsRpcServerError();
  void SetUploadCardRpcPaymentsFails();

  // Clicks on the given views::View*.
  void ClickOnView(views::View* view);

  // Clicks on the given dialog views::View*.
  void ClickOnDialogView(views::View* view);

  // Clicks on the given dialog views::View* and waits for the dialog to close.
  void ClickOnDialogViewAndWait(views::View* view);

  // Clicks on a view from within the dialog.
  void ClickOnDialogViewWithId(DialogViewId view_id);

  // Clicks on a view from within the dialog and waits for the dialog to close.
  void ClickOnDialogViewWithIdAndWait(DialogViewId view_id);

  // Returns the views::View* that was previously assigned the id |view_id|.
  views::View* FindViewInBubbleById(DialogViewId view_id);

  // Assert that there is a SaveCardBubbleViews bubble open, then click on the
  // [X] button.
  void ClickOnCloseButton();

  // Gets the views::View* instance of the save credit card bubble.
  SaveCardBubbleViews* GetSaveCardBubbleViews();

  // Gets the views::View* instance of the credit card icon.
  SaveCardIconView* GetSaveCardIconView();

  content::WebContents* GetActiveWebContents();

  // Adding an observer to the controller so we know when the sign-in promo
  // shows after the animation.
  void AddEventObserverToController();

  // Reduces the animation time to one millisecond so that the test does not
  // take long.
  void ReduceAnimationTime();

  // Resets the event waiter for a given |event_sequence|.
  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence);
  // Wait for the event(s) passed to ResetEventWaiter*() to occur.
  void WaitForObservedEvent();

  void ReturnToInitialPage();

  network::TestURLLoaderFactory* test_url_loader_factory();

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  const std::string test_file_path_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViewsBrowserTestBase);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_BUBBLE_VIEWS_BROWSERTEST_BASE_H_
