// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

class MockDelegate : public AddressBubbleControllerDelegate {
 public:
  MockDelegate() = default;
  MockDelegate(MockDelegate&) = delete;
  MockDelegate& operator=(MockDelegate&) = delete;
  ~MockDelegate() = default;

  MOCK_METHOD(void,
              OnUserDecision,
              (AutofillClient::AddressPromptUserDecision decision,
               base::optional_ref<const AutofillProfile> profile),
              (override));
  MOCK_METHOD(void,
              ShowEditor,
              (const AutofillProfile&,
               const std::u16string&,
               const std::u16string&,
               bool),
              (override));
  MOCK_METHOD(void, OnBubbleClosed, (), (override));

  base::WeakPtr<AddressBubbleControllerDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class SaveAddressBubbleControllerTest : public ::testing::Test {
 public:
  SaveAddressBubbleControllerTest() = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ChromeAutofillClient::CreateForWebContents(web_contents());
  }

  std::unique_ptr<SaveAddressBubbleController> CreateController(
      const AutofillProfile& profile,
      AutofillClient::SaveAddressBubbleType save_address_bubble_type) {
    return std::make_unique<SaveAddressBubbleController>(
        delegate_.GetWeakPtr(), web_contents(), profile,
        save_address_bubble_type);
  }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

  const std::string& app_locale() const {
    return g_browser_process->GetFeatures()
        ->application_locale_storage()
        ->Get();
  }

 private:
  MockDelegate delegate_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(SaveAddressBubbleControllerTest, SavingNonAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  auto controller =
      CreateController(profile, AutofillClient::SaveAddressBubbleType::kSave);

  EXPECT_EQ(controller->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE));
  EXPECT_NE(controller->GetHeaderImages(), std::nullopt);
  EXPECT_TRUE(controller->GetBodyText().empty());
  EXPECT_EQ(
      controller->GetAddressSummary(),
      GetEnvelopeStyleAddress(profile, app_locale(), /*include_recipient=*/true,
                              /*include_country=*/true));
  EXPECT_EQ(controller->GetProfileEmail(),
            profile.GetInfo(EMAIL_ADDRESS, app_locale()));
  EXPECT_EQ(
      controller->GetProfilePhone(),
      autofill::i18n::GetFormattedPhoneNumberForDisplay(profile, app_locale()));
  EXPECT_EQ(controller->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller->GetCancelCallbackValue(),
            AutofillClient::AddressPromptUserDecision::kDeclined);
  EXPECT_TRUE(controller->GetFooterMessage().empty());
}

TEST_F(SaveAddressBubbleControllerTest, SavingAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccount);
  auto controller =
      CreateController(profile, AutofillClient::SaveAddressBubbleType::kSave);
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);

  EXPECT_EQ(controller->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE));
  EXPECT_NE(controller->GetHeaderImages(), std::nullopt);
  EXPECT_TRUE(controller->GetBodyText().empty());
  EXPECT_EQ(
      controller->GetAddressSummary(),
      GetEnvelopeStyleAddress(profile, app_locale(), /*include_recipient=*/true,
                              /*include_country=*/true));
  EXPECT_EQ(controller->GetProfileEmail(),
            profile.GetInfo(EMAIL_ADDRESS, app_locale()));
  EXPECT_EQ(
      controller->GetProfilePhone(),
      autofill::i18n::GetFormattedPhoneNumberForDisplay(profile, app_locale()));
  EXPECT_EQ(controller->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller->GetCancelCallbackValue(),
            AutofillClient::AddressPromptUserDecision::kDeclined);
  EXPECT_EQ(
      controller->GetFooterMessage(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE, email));
}

TEST_F(SaveAddressBubbleControllerTest, MigrateIntoAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  auto controller = CreateController(
      profile, AutofillClient::SaveAddressBubbleType::kMigrateToAccount);
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);

  EXPECT_EQ(controller->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE));
  EXPECT_NE(controller->GetHeaderImages(), std::nullopt);
  EXPECT_EQ(controller->GetBodyText(),
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE, email));
  EXPECT_FALSE(controller->GetAddressSummary().empty());
  EXPECT_TRUE(controller->GetProfileEmail().empty());
  EXPECT_TRUE(controller->GetProfilePhone().empty());
  EXPECT_EQ(controller->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_MIGRATE_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
  EXPECT_EQ(controller->GetCancelCallbackValue(),
            AutofillClient::AddressPromptUserDecision::kNever);
  EXPECT_TRUE(controller->GetFooterMessage().empty());
}

TEST_F(SaveAddressBubbleControllerTest, CombiningAccountsTypes) {
  const AutofillProfile merged_profile = test::SupersetProfileOf(
      {test::AccountNameEmailProfile(),
       test::OnlyAddressProfile(AutofillProfile::RecordType::kAccountHome)},
      app_locale(), AutofillProfile::RecordType::kAccount);
  std::unique_ptr<SaveAddressBubbleController> controller = CreateController(
      merged_profile,
      AutofillClient::SaveAddressBubbleType::kHomeWorkNameEmailMerge);

  EXPECT_EQ(
      controller->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_NAME_EMAIL_HOME_WORK_MERGE_PROMPT_TITLE));
  EXPECT_EQ(
      controller->GetOkButtonLabel(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_NAME_EMAIL_HOME_WORK_MERGE_OK_BUTTON_LABEL));
  EXPECT_EQ(
      controller->GetNegativeButtonLabel(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_NAME_EMAIL_HOME_WORK_MERGE_CANCEL_BUTTON_LABEL));
  EXPECT_EQ(controller->GetFooterMessage(),
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
                base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                                      web_contents()->GetBrowserContext())
                                      ->email)));
  EXPECT_NE(controller->GetHeaderImages(), std::nullopt);
  EXPECT_TRUE(controller->GetBodyText().empty());

  EXPECT_EQ(controller->GetAddressSummary(),
            GetEnvelopeStyleAddress(merged_profile, app_locale(),
                                    /*include_recipient=*/true,
                                    /*include_country=*/true));
  EXPECT_EQ(controller->GetProfileEmail(),
            merged_profile.GetInfo(EMAIL_ADDRESS, app_locale()));

  EXPECT_EQ(controller->GetCancelCallbackValue(),
            AutofillClient::AddressPromptUserDecision::kDeclined);
}

}  // namespace
}  // namespace autofill
