// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class PersonalDataManagerTest : public PersonalDataManagerTestBase,
                                public testing::Test {
 protected:
  PersonalDataManagerTest() = default;

  ~PersonalDataManagerTest() override {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
  }

  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }
  void TearDown() override { TearDownTest(); }

  void ResetPersonalDataManager(bool use_sync_transport_mode = false) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        use_sync_transport_mode, personal_data_.get());
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

TEST_F(PersonalDataManagerTest, IsCountryEligibleForAccountStorage) {
  EXPECT_TRUE(personal_data_->IsCountryEligibleForAccountStorage("AT"));
  EXPECT_FALSE(personal_data_->IsCountryEligibleForAccountStorage("IR"));
}

TEST_F(PersonalDataManagerTest, ChangeCallbackIsTriggeredOnAddedProfile) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run);

  personal_data_->AddChangeCallback(callback.Get());
  AddProfileToPersonalDataManager(test::GetFullProfile());
}

}  // namespace autofill
