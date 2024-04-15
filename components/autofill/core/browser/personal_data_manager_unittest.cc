// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class PersonalDataManagerTest : public PersonalDataManagerTestBase,
                                public testing::Test {
 protected:
  ~PersonalDataManagerTest() override {
    personal_data_->Shutdown();
    personal_data_.reset();
  }

  void SetUp() override {
    SetUpTest();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        /*use_sync_transport_mode=*/false, personal_data_.get());
  }
  void TearDown() override { TearDownTest(); }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

TEST_F(PersonalDataManagerTest, ChangeCallbackIsTriggeredOnAddedProfile) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run);
  personal_data_->AddChangeCallback(callback.Get());
  PersonalDataChangedWaiter waiter(*personal_data_);
  personal_data_->AddProfile(test::GetFullProfile());
  std::move(waiter).Wait();
}

}  // namespace autofill
