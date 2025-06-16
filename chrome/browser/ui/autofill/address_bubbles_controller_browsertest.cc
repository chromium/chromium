// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "content/public/test/browser_test.h"

namespace autofill {

using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

class AddressBubblesControllerBrowserTest : public InProcessBrowserTest {
 public:
  AddressBubblesControllerBrowserTest() = default;
  AddressBubblesControllerBrowserTest(
      const AddressBubblesControllerBrowserTest&) = delete;
  AddressBubblesControllerBrowserTest& operator=(
      const AddressBubblesControllerBrowserTest&) = delete;
  ~AddressBubblesControllerBrowserTest() override = default;

 protected:
  raw_ptr<content::WebContents> web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  AddressBubblesController* controller() {
    return AddressBubblesController::FromWebContents(web_contents());
  }
};

IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined,
                  Property(&profile_ref::has_value, false)));
  controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

// This is testing that closing all tabs (which effectively destroys the web
// contents) will trigger the save callback with kIgnored decions if the users
// hasn't interacted with the prompt already.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       WebContentsDestroyedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{}, callback.Get());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  CHECK_EQ(1, tab_strip_model->count());
  // There is only now tab open, so the active web contents, are the
  // controller's web contents.
  content::WebContents* controller_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Now add another tab, and close the controller tab to make sure the window
  // remains open. This should destroy the web contents of the controller and
  // invoke the callback with a decision kIgnored.
  GURL url(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  // Close controller tab.
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      tab_strip_model->GetIndexOfWebContents(controller_web_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
}

// This is testing that the bubble is visible and active when shown.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       BubbleShouldBeVisibleByDefault) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{},
      /*callback=*/base::DoNothing());

  // Bubble is visible and active
  EXPECT_TRUE(controller()->GetBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when a second prompt comes while another prompt is
// shown, the controller will ignore it, and inform the backend that the second
// prompt has been auto declined.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       SecondPromptWillBeAutoDeclinedWhileFirstIsVisible) {
  AutofillProfile profile = test::GetFullProfile();

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{},
      /*callback=*/base::DoNothing());

  // Second prompt should be auto declined.
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{}, callback.Get());
}

// This is testing that when a second prompt comes while another prompt is in
// progress but not shown, the controller will inform the backend that the first
// process is ignored.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       FirstHiddenPromptWillBeIgnoredWhenSecondPromptArrives) {
  AutofillProfile profile = test::GetFullProfile();

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{}, callback.Get());
  controller()->OnBubbleClosed();

  // When second prompt comes, the first one will be ignored.
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*is_migration_to_account=*/{},
      /*callback=*/base::DoNothing());
}

}  // namespace autofill
