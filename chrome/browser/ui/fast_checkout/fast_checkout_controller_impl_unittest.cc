// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include <memory>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

struct MockFastCheckoutView : FastCheckoutView {
  MockFastCheckoutView() = default;
  ~MockFastCheckoutView() override = default;

  MOCK_METHOD(void,
              Show,
              (const std::vector<autofill::AutofillProfile*>&,
               const std::vector<autofill::CreditCard*>&),
              (override));
};

struct MockFastCheckoutImplDelegate : FastCheckoutControllerImpl::Delegate {
  MockFastCheckoutImplDelegate() = default;
  ~MockFastCheckoutImplDelegate() override = default;

  MOCK_METHOD(void,
              OnOptionsSelected,
              (std::unique_ptr<AutofillProfile>, std::unique_ptr<CreditCard>),
              (override));

  MOCK_METHOD(void, OnDismiss, (), (override));
};

class TestFastCheckoutControllerImpl : public FastCheckoutControllerImpl {
 public:
  TestFastCheckoutControllerImpl(
      content::WebContents* web_contents,
      Delegate* delegate,
      FastCheckoutView* view,
      autofill::PersonalDataManager* personal_manager)
      : FastCheckoutControllerImpl(web_contents, delegate),
        view_(view),
        personal_manager_(personal_manager) {}
  ~TestFastCheckoutControllerImpl() override = default;

  FastCheckoutView* GetOrCreateView() override { return view_; }
  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return personal_manager_;
  }

 private:
  FastCheckoutView* view_;
  autofill::PersonalDataManager* personal_manager_;
};

}  // namespace

class FastCheckoutControllerImplTest : public ChromeRenderViewHostTestHarness {
 protected:
  FastCheckoutControllerImplTest() = default;
  ~FastCheckoutControllerImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    fast_checkout_controller_ =
        std::make_unique<TestFastCheckoutControllerImpl>(
            web_contents(), &delegate_, &mock_view_,
            &test_personal_data_manager_);

    test_personal_data_manager_.SetAutofillProfileEnabled(true);
    test_personal_data_manager_.SetAutofillCreditCardEnabled(true);
    test_personal_data_manager_.SetAutofillWalletImportEnabled(true);
  }

  MockFastCheckoutView mock_view_;
  MockFastCheckoutImplDelegate delegate_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  // The object to be tested.
  std::unique_ptr<TestFastCheckoutControllerImpl> fast_checkout_controller_;
};

TEST_F(FastCheckoutControllerImplTest, Show) {
  AutofillProfile profile1 = autofill::test::GetFullProfile();
  AutofillProfile profile2 = autofill::test::GetFullProfile2();
  test_personal_data_manager_.AddProfile(profile1);
  test_personal_data_manager_.AddProfile(profile2);

  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  test_personal_data_manager_.AddCreditCard(credit_card1);
  test_personal_data_manager_.AddCreditCard(credit_card2);

  EXPECT_CALL(
      mock_view_,
      Show(UnorderedElementsAre(Pointee(profile1), Pointee(profile2)),
           UnorderedElementsAre(Pointee(credit_card1), Pointee(credit_card2))));

  fast_checkout_controller_->Show();
}

TEST_F(FastCheckoutControllerImplTest, OnOptionsSelected) {
  std::unique_ptr<AutofillProfile> profile =
      std::make_unique<AutofillProfile>(autofill::test::GetFullProfile());
  raw_ptr<AutofillProfile> profile_ptr = profile.get();

  std::unique_ptr<CreditCard> credit_card =
      std::make_unique<CreditCard>(autofill::test::GetCreditCard());
  raw_ptr<CreditCard> credit_card_ptr = credit_card.get();

  EXPECT_CALL(delegate_, OnOptionsSelected(Pointee(Eq(*profile_ptr)),
                                           Pointee(Eq(*credit_card_ptr))));

  fast_checkout_controller_->OnOptionsSelected(std::move(profile),
                                               std::move(credit_card));
}

TEST_F(FastCheckoutControllerImplTest, OnDismiss) {
  EXPECT_CALL(delegate_, OnDismiss);
  fast_checkout_controller_->OnDismiss();
}
