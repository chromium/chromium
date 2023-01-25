// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_iban_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestSaveIbanBubbleControllerImpl : public SaveIbanBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestSaveIbanBubbleControllerImpl>(web_contents));
  }

  explicit TestSaveIbanBubbleControllerImpl(content::WebContents* web_contents)
      : SaveIbanBubbleControllerImpl(web_contents) {}
};

class SaveIbanBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  SaveIbanBubbleControllerImplTest() = default;
  SaveIbanBubbleControllerImplTest(SaveIbanBubbleControllerImplTest&) = delete;
  SaveIbanBubbleControllerImplTest& operator=(
      SaveIbanBubbleControllerImplTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestSaveIbanBubbleControllerImpl::CreateForTesting(web_contents);
  }

  void ShowLocalBubble(const IBAN& iban) {
    controller()->OfferLocalSave(
        iban, /*should_show_prompt=*/true,
        base::BindOnce(&SaveIbanBubbleControllerImplTest::LocalSaveIBANCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void ClickSaveButton(const std::u16string& nickname) {
    controller()->OnSaveButton(nickname);
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
  }

  std::u16string saved_nickname() { return saved_nickname_; }

 protected:
  TestSaveIbanBubbleControllerImpl* controller() {
    return static_cast<TestSaveIbanBubbleControllerImpl*>(
        TestSaveIbanBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  void LocalSaveIBANCallback(
      AutofillClient::SaveIBANOfferUserDecision user_decision,
      const absl::optional<std::u16string>& nickname) {
    saved_nickname_ = nickname.value_or(u"");
  }

  std::u16string saved_nickname_;
  base::WeakPtrFactory<SaveIbanBubbleControllerImplTest> weak_ptr_factory_{
      this};
};

TEST_F(SaveIbanBubbleControllerImplTest, LocalIbanSavedSuccessfully) {
  std::u16string nickname = u"My doctor's IBAN";
  IBAN iban = autofill::test::GetIBAN();
  ShowLocalBubble(iban);
  ClickSaveButton(nickname);

  EXPECT_EQ(nickname, saved_nickname());
}

}  // namespace autofill
