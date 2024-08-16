// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

class AddressBubblesControllerTest
    : public BrowserWithTestWindowTest {
 public:
  AddressBubblesControllerTest() = default;
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
  }

  AddressBubblesController* controller() {
    return AddressBubblesController::FromWebContents(
        web_contents());
  }

 protected:
  raw_ptr<content::WebContents> web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const std::string& app_locale() const {
    return g_browser_process->GetApplicationLocale();
  }
};

TEST_F(AddressBubblesControllerTest,
       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt);
}

TEST_F(AddressBubblesControllerTest,
       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined,
                  Property(&profile_ref::has_value, false)));
  controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

// This is testing that closing all tabs (which effectively destroys the web
// contents) will trigger the save callback with kIgnored decions if the users
// hasn't interacted with the prompt already.
TEST_F(AddressBubblesControllerTest,
       WebContentsDestroyedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{}, callback.Get());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // There is only now tab open, so the active web contents, are the
  // controller's web contents.
  content::WebContents* controller_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Now add another tab, and close the controller tab to make sure the window
  // remains open. This should destroy the web contents of the controller and
  // invoke the callback with a decision kIgnored.
  AddTab(browser(), GURL("http://foo.com/"));
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
TEST_F(AddressBubblesControllerTest, BubbleShouldBeVisibleByDefault) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{},
      /*callback=*/base::DoNothing());

  // Bubble is visible and active
  EXPECT_TRUE(controller()->GetBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when a second prompt comes while another prompt is
// shown, the controller will ignore it, and inform the backend that the second
// prompt has been auto declined.
TEST_F(AddressBubblesControllerTest,
       SecondPromptWillBeAutoDeclinedWhileFirstIsVisible) {
  AutofillProfile profile = test::GetFullProfile();

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{},
      /*callback=*/base::DoNothing());

  // Second prompt should be auto declined.
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{}, callback.Get());
}

// This is testing that when a second prompt comes while another prompt is in
// progress but not shown, the controller will inform the backend that the first
// process is ignored.
TEST_F(AddressBubblesControllerTest,
       FirstHiddenPromptWillBeIgnoredWhenSecondPromptArrives) {
  AutofillProfile profile = test::GetFullProfile();

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{}, callback.Get());
  controller()->OnBubbleClosed();

  // When second prompt comes, the first one will be ignored.
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, /*original_profile=*/nullptr,
      /*options=*/{},
      /*callback=*/base::DoNothing());
}

}  // namespace autofill
