// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include <string>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/ui_util.h"
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

class SaveUpdateAddressProfileBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  SaveUpdateAddressProfileBubbleControllerImplTest() = default;
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    SaveUpdateAddressProfileBubbleControllerImpl::CreateForWebContents(
        web_contents());
  }

  SaveUpdateAddressProfileBubbleControllerImpl* controller() {
    return SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
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

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile));
  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          testing::_));
  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

// This is testing that closing all tabs (which effectively destroys the web
// contents) will trigger the save callback with kIgnored decions if the users
// hasn't interacted with the prompt already.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       WebContentsDestroyedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());

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
  EXPECT_CALL(callback,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  testing::_));
  // Close controller tab.
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      tab_strip_model->GetIndexOfWebContents(controller_web_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
}

// This is testing that when the SaveAddressProfilePromptOptions has the
// show_prompt set to true, the bubble should be visible.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       BubbleShouldBeVisibleWithShowPrompt) {
  AutofillProfile profile = test::GetFullProfile();
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      /*address_profile_save_prompt_callback=*/base::DoNothing());

  // Bubble is visible and active
  EXPECT_TRUE(controller()->GetBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when the SaveAddressProfilePromptOptions has the
// show_prompt set to false, the bubble should be invisible.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       BubbleShouldBeInvisibleWithoutShowPrompt) {
  AutofillProfile profile = test::GetFullProfile();
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = false},
      /*address_profile_save_prompt_callback=*/base::DoNothing());
  // Bubble is invisible but active
  EXPECT_FALSE(controller()->GetBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when a second prompt comes while another prompt is
// shown, the controller will ignore it, and inform the backend that the second
// prompt has been auto declined.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       SecondPromptWillBeAutoDeclinedWhileFirstIsVisible) {
  AutofillProfile profile = test::GetFullProfile();

  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      /*address_profile_save_prompt_callback=*/base::DoNothing());

  // Second prompt should be auto declined.
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
          testing::_));
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());
}

// This is testing that when a second prompt comes while another prompt is in
// progress but not shown, the controller will inform the backend that the first
// process is ignored.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       FirstHiddenPromptWillBeIgnoredWhenSecondPromptArrives) {
  AutofillProfile profile = test::GetFullProfile();

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());
  controller()->OnBubbleClosed();

  // When second prompt comes, the first one will be ignored.
  EXPECT_CALL(callback,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  testing::_));
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      /*address_profile_save_prompt_callback=*/base::DoNothing());
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       SavingNonAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile* original_profile = nullptr;
  controller()->OfferSave(profile, original_profile, {}, base::DoNothing());

  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE));
  EXPECT_NE(controller()->GetHeaderImages(), absl::nullopt);
  EXPECT_TRUE(controller()->GetBodyText().empty());
  EXPECT_EQ(controller()->GetAddressSummary(),
            GetEnvelopeStyleAddress(profile, app_locale(), true, true));
  EXPECT_EQ(controller()->GetProfileEmail(),
            profile.GetInfo(EMAIL_ADDRESS, app_locale()));
  EXPECT_EQ(controller()->GetProfilePhone(),
            profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale()));
  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller()->GetCancelCallbackValue(),
            AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
  EXPECT_TRUE(controller()->GetFooterMessage().empty());
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       UpdatingNonAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile original_profile = test::GetFullProfile();
  controller()->OfferSave(profile, &original_profile, {}, base::DoNothing());

  EXPECT_EQ(
      controller()->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE));
  EXPECT_EQ(controller()->GetHeaderImages(), absl::nullopt);
  EXPECT_TRUE(controller()->GetBodyText().empty());
  EXPECT_TRUE(controller()->GetAddressSummary().empty());
  EXPECT_TRUE(controller()->GetProfileEmail().empty());
  EXPECT_TRUE(controller()->GetProfilePhone().empty());
  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller()->GetCancelCallbackValue(),
            AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
  EXPECT_TRUE(controller()->GetFooterMessage().empty());
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest, SavingAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile* original_profile = nullptr;
  controller()->OfferSave(profile, original_profile, {}, base::DoNothing());
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);

  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE));
  EXPECT_NE(controller()->GetHeaderImages(), absl::nullopt);
  EXPECT_TRUE(controller()->GetBodyText().empty());
  EXPECT_EQ(controller()->GetAddressSummary(),
            GetEnvelopeStyleAddress(profile, app_locale(), true, true));
  EXPECT_EQ(controller()->GetProfileEmail(),
            profile.GetInfo(EMAIL_ADDRESS, app_locale()));
  EXPECT_EQ(controller()->GetProfilePhone(),
            profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale()));
  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller()->GetCancelCallbackValue(),
            AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
  EXPECT_EQ(
      controller()->GetFooterMessage(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE, email));
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       UpdatingAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile original_profile = test::GetFullProfile();
  original_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  controller()->OfferSave(profile, &original_profile, {}, base::DoNothing());
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);

  EXPECT_EQ(
      controller()->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE));
  EXPECT_EQ(controller()->GetHeaderImages(), absl::nullopt);
  EXPECT_TRUE(controller()->GetBodyText().empty());
  EXPECT_TRUE(controller()->GetAddressSummary().empty());
  EXPECT_TRUE(controller()->GetProfileEmail().empty());
  EXPECT_TRUE(controller()->GetProfilePhone().empty());
  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller()->GetCancelCallbackValue(),
            AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
  EXPECT_EQ(
      controller()->GetFooterMessage(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE, email));
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       MigrateIntoAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile* original_profile = nullptr;
  controller()->OfferSave(profile, original_profile,
                          {.is_migration_to_account = true}, base::DoNothing());
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);

  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE));
  EXPECT_NE(controller()->GetHeaderImages(), absl::nullopt);
  EXPECT_EQ(controller()->GetBodyText(),
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE, email));
  EXPECT_FALSE(controller()->GetAddressSummary().empty());
  EXPECT_TRUE(controller()->GetProfileEmail().empty());
  EXPECT_TRUE(controller()->GetProfilePhone().empty());
  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_MIGRATE_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller()->GetCancelCallbackValue(),
            AutofillClient::SaveAddressProfileOfferUserDecision::kNever);
  EXPECT_TRUE(controller()->GetFooterMessage().empty());
}

}  // namespace autofill
