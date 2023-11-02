// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_unsynced_credentials_locally_bubble_controller.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::NiceMock;
using testing::ReturnRef;

class SaveUnsyncedCredentialsLocallyBubbleControllerTest
    : public ::testing::Test {
 public:
  SaveUnsyncedCredentialsLocallyBubbleControllerTest() {
    unsynced_credentials_.resize(2);
    unsynced_credentials_[0].username_value = u"user1";
    unsynced_credentials_[0].password_value = u"password1";
    unsynced_credentials_[1].username_value = u"user2";
    unsynced_credentials_[1].password_value = u"password2";
  }
  ~SaveUnsyncedCredentialsLocallyBubbleControllerTest() override = default;

 protected:
  NiceMock<PasswordsModelDelegateMock> model_delegate_mock_;
  std::vector<password_manager::PasswordForm> unsynced_credentials_;
};

TEST_F(SaveUnsyncedCredentialsLocallyBubbleControllerTest,
       ShouldGetCredentialsFromDelegate) {
  EXPECT_CALL(model_delegate_mock_, GetUnsyncedCredentials())
      .WillOnce(ReturnRef(unsynced_credentials_));
  SaveUnsyncedCredentialsLocallyBubbleController controller(
      model_delegate_mock_.AsWeakPtr());
  EXPECT_EQ(controller.unsynced_credentials(), unsynced_credentials_);
}

TEST_F(SaveUnsyncedCredentialsLocallyBubbleControllerTest,
       ShouldSaveSelectedCredentialsInProfileStoreOnSaveButtonClicked) {
  EXPECT_CALL(model_delegate_mock_, GetUnsyncedCredentials())
      .WillOnce(ReturnRef(unsynced_credentials_));
  SaveUnsyncedCredentialsLocallyBubbleController controller(
      model_delegate_mock_.AsWeakPtr());
  EXPECT_CALL(model_delegate_mock_,
              SaveUnsyncedCredentialsInProfileStore(
                  std::vector<password_manager::PasswordForm>{
                      unsynced_credentials_[1]}));
  controller.OnSaveClicked({false, true});
}

TEST_F(SaveUnsyncedCredentialsLocallyBubbleControllerTest,
       ShouldDiscardCredentialsInProfileStoreOnCancelButtonClicked) {
  EXPECT_CALL(model_delegate_mock_, GetUnsyncedCredentials())
      .WillOnce(ReturnRef(unsynced_credentials_));
  SaveUnsyncedCredentialsLocallyBubbleController controller(
      model_delegate_mock_.AsWeakPtr());
  EXPECT_CALL(model_delegate_mock_, DiscardUnsyncedCredentials);
  controller.OnCancelClicked();
}
