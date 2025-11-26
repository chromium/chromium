// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

class SaveUpdateAddressProfileFlowManagerBrowserTest
    : public AndroidBrowserTest {
 public:
  // Explicitly avoiding the migration logic because the user must be logged in.
  // TODO(crbug.com/40259080): figure out if the user can be logged in from an
  // Android browser test.
  static constexpr SaveUpdateAddressProfilePromptMode kNotMigrationToAccount =
      SaveUpdateAddressProfilePromptMode::kSaveNewProfile;

  SaveUpdateAddressProfileFlowManagerBrowserTest() = default;
  ~SaveUpdateAddressProfileFlowManagerBrowserTest() override = default;

  // AndroidBrowserTest:
  void SetUpOnMainThread() override {
    flow_manager_ = std::make_unique<SaveUpdateAddressProfileFlowManager>();
  }

  void TearDownOnMainThread() override {
    // Destroy `flow_manager_` before WebContents are destroyed.
    flow_manager_.reset();
  }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool IsMessageDisplayed() {
    return flow_manager_->GetMessageControllerForTest()->IsMessageDisplayed();
  }

  bool IsPromptDisplayed() {
    return !!flow_manager_->GetPromptControllerForTest();
  }

  std::unique_ptr<SaveUpdateAddressProfileFlowManager> flow_manager_;
};

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileFlowManagerBrowserTest,
                       TriggerAutoDeclineDecisionIfMessageIsDisplayed) {
  AutofillProfile submitted_profile = test::GetFullProfile();
  AutofillProfile original_profile = test::GetFullProfile2();
  flow_manager_->OfferSave(GetWebContents(), submitted_profile,
                           &original_profile, kNotMigrationToAccount,
                           /*callback=*/base::DoNothing());
  EXPECT_TRUE(IsMessageDisplayed());
  EXPECT_FALSE(IsPromptDisplayed());

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  AutofillProfile another_profile = test::GetFullProfile2();
  EXPECT_CALL(another_save_callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, false)));
  flow_manager_->OfferSave(GetWebContents(), another_profile,
                           /*original_profile=*/nullptr, kNotMigrationToAccount,
                           another_save_callback.Get());
}

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileFlowManagerBrowserTest,
                       TriggerAutoDeclineDecisionIfPromptIsDisplayed) {
  AutofillProfile submitted_profile = test::GetFullProfile();
  AutofillProfile original_profile = test::GetFullProfile2();
  flow_manager_->OfferSave(GetWebContents(), submitted_profile,
                           &original_profile, kNotMigrationToAccount,
                           /*callback=*/base::DoNothing());
  // Proceed with message to prompt.
  flow_manager_->GetMessageControllerForTest()->OnPrimaryActionForTest();
  flow_manager_->GetMessageControllerForTest()->DismissMessageForTest(
      messages::DismissReason::PRIMARY_ACTION);
  EXPECT_FALSE(IsMessageDisplayed());
  EXPECT_TRUE(IsPromptDisplayed());

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  AutofillProfile another_profile = test::GetFullProfile2();
  EXPECT_CALL(another_save_callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, false)));
  flow_manager_->OfferSave(GetWebContents(), another_profile,
                           /*original_profile=*/nullptr, kNotMigrationToAccount,
                           another_save_callback.Get());
}

}  // namespace autofill
