// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
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

  base::WeakPtr<MockDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class UpdateAddressBubbleControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    ::testing::Test::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ChromeAutofillClient::CreateForWebContents(web_contents());
  }

  std::unique_ptr<UpdateAddressBubbleController> CreateController(
      const AutofillProfile& profile,
      const AutofillProfile& original_profile) {
    return std::make_unique<UpdateAddressBubbleController>(
        delegate_.AsWeakPtr(), web_contents(), profile, original_profile);
  }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  MockDelegate delegate_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(UpdateAddressBubbleControllerTest, UpdatingNonAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile original_profile = test::GetFullProfile();
  auto controller = CreateController(profile, original_profile);

  EXPECT_EQ(
      controller->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE));
  EXPECT_TRUE(controller->GetFooterMessage().empty());
}

TEST_F(UpdateAddressBubbleControllerTest, UpdatingAccountAddress) {
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccount);
  AutofillProfile original_profile = test::GetFullProfile();
  test_api(original_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);
  auto controller = CreateController(profile, original_profile);

  EXPECT_EQ(
      controller->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE));
  EXPECT_EQ(
      controller->GetFooterMessage(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE, email));
}

}  // namespace
}  // namespace autofill
