// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/personal_data_manager_test_util.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace test {
namespace {

class WaitForFinishedPersonalDataManagerObserver
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit WaitForFinishedPersonalDataManagerObserver(
      base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  // autofill::PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override {}
  void OnPersonalDataFinishedProfileTasks() override {
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

void AddAutofillProfile(content::BrowserContext* browser_context,
                        const autofill::AutofillProfile& autofill_profile) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  size_t profile_count = personal_data_manager->GetProfiles().size();

  base::RunLoop data_loop;
  WaitForFinishedPersonalDataManagerObserver personal_data_observer(
      data_loop.QuitClosure());
  personal_data_manager->AddObserver(&personal_data_observer);

  personal_data_manager->AddProfile(autofill_profile);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer);
  EXPECT_EQ(profile_count + 1, personal_data_manager->GetProfiles().size());
}

void AddCreditCard(content::BrowserContext* browser_context,
                   const autofill::CreditCard& card) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  if (card.record_type() != autofill::CreditCard::LOCAL_CARD) {
    personal_data_manager->AddServerCreditCardForTest(
        std::make_unique<autofill::CreditCard>(card));
    return;
  }
  size_t card_count = personal_data_manager->GetCreditCards().size();

  base::RunLoop data_loop;
  WaitForFinishedPersonalDataManagerObserver personal_data_observer(
      data_loop.QuitClosure());
  personal_data_manager->AddObserver(&personal_data_observer);

  personal_data_manager->AddCreditCard(card);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer);
  EXPECT_EQ(card_count + 1, personal_data_manager->GetCreditCards().size());
}

}  // namespace test
}  // namespace payments
