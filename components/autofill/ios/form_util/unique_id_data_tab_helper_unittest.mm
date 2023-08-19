// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"

#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// Test fixture for UniqueIDDataTabHelper class.
class UniqueIDDataTabHelperTest : public PlatformTest {
 protected:
  web::FakeWebState first_web_state_;
  web::FakeWebState second_web_state_;
};

// Tests that a renderer ID is returned for a WebState, and rendered ID's are
// different for different WebStates if they were once set differently.
TEST_F(UniqueIDDataTabHelperTest, UniqueIdentifiers) {
  UniqueIDDataTabHelper::CreateForWebState(&first_web_state_);
  UniqueIDDataTabHelper::CreateForWebState(&second_web_state_);

  uint32_t first_available_unique_id =
      UniqueIDDataTabHelper::FromWebState(&first_web_state_)
          ->GetNextAvailableRendererID();
  uint32_t second_available_unique_id =
      UniqueIDDataTabHelper::FromWebState(&second_web_state_)
          ->GetNextAvailableRendererID();

  EXPECT_EQ(first_available_unique_id, 1U);
  EXPECT_EQ(first_available_unique_id, second_available_unique_id);

  UniqueIDDataTabHelper::FromWebState(&second_web_state_)
      ->SetNextAvailableRendererID(10U);
  UniqueIDDataTabHelper::FromWebState(&second_web_state_)
      ->SetNextAvailableRendererID(20U);

  first_available_unique_id =
      UniqueIDDataTabHelper::FromWebState(&first_web_state_)
          ->GetNextAvailableRendererID();
  second_available_unique_id =
      UniqueIDDataTabHelper::FromWebState(&second_web_state_)
          ->GetNextAvailableRendererID();

  EXPECT_NE(first_available_unique_id, second_available_unique_id);
}

// Tests that a renderer ID is stable across successive calls.
TEST_F(UniqueIDDataTabHelperTest, StableAcrossCalls) {
  UniqueIDDataTabHelper::CreateForWebState(&first_web_state_);
  UniqueIDDataTabHelper* tab_helper =
      UniqueIDDataTabHelper::FromWebState(&first_web_state_);
  tab_helper->SetNextAvailableRendererID(10U);

  const uint32_t first_call_available_unique_id =
      tab_helper->GetNextAvailableRendererID();
  const uint32_t second_call_available_unique_id =
      tab_helper->GetNextAvailableRendererID();

  EXPECT_EQ(first_call_available_unique_id, second_call_available_unique_id);
}
