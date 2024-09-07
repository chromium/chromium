// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_address_util.h"
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


class AddNewAddressBubbleControllerTest : public ::testing::Test {
 public:
  AddNewAddressBubbleControllerTest() = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    autofill_client()
        ->GetPersonalDataManager()
        ->test_address_data_manager()
        .SetAutofillProfileEnabled(true);
  }

  std::unique_ptr<AddNewAddressBubbleController> CreateController() {
    return std::make_unique<AddNewAddressBubbleController>(
        web_contents(), delegate_.GetWeakPtr());
  }

 protected:
  MockDelegate& delegate() { return delegate_; }
  content::WebContents* web_contents() { return web_contents_.get(); }
  TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

 private:
  MockDelegate delegate_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

TEST_F(AddNewAddressBubbleControllerTest, SavingIntoChrome) {
  autofill_client()
      ->GetPersonalDataManager()
      ->test_address_data_manager()
      .SetIsEligibleForAddressAccountStorage(false);

  std::unique_ptr<AddNewAddressBubbleController> controller =
      CreateController();

  EXPECT_EQ(controller->GetBodyText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ADD_NEW_ADDRESS_INTO_CHROME_PROMPT_BODY_TEXT));
  EXPECT_CALL(
      delegate(),
      ShowEditor(
          ::testing::Property(&AutofillProfile::record_type,
                              AutofillProfile::RecordType::kLocalOrSyncable),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_ADDRESS_EDITOR_TITLE),
          std::u16string(),
          /*is_editing_existing_address=*/false));
  controller->OnAddButtonClicked();
}

TEST_F(AddNewAddressBubbleControllerTest, SavingIntoAccount) {
  autofill_client()
      ->GetPersonalDataManager()
      ->test_address_data_manager()
      .SetIsEligibleForAddressAccountStorage(true);

  std::unique_ptr<AddNewAddressBubbleController> controller =
      CreateController();
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);

  EXPECT_EQ(controller->GetBodyText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ADD_NEW_ADDRESS_INTO_ACCOUNT_PROMPT_BODY_TEXT));
  EXPECT_EQ(
      controller->GetFooterMessage(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE, email));
  EXPECT_CALL(
      delegate(),
      ShowEditor(
          ::testing::Property(&AutofillProfile::record_type,
                              AutofillProfile::RecordType::kAccount),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_ADDRESS_EDITOR_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE, email),
          /*is_editing_existing_address=*/false));
  controller->OnAddButtonClicked();
}

}  // namespace
}  // namespace autofill
