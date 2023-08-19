// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::AllOf;
using testing::ElementsAre;
using testing::Field;

const std::string kUrl = "https://test.com";
const std::u16string kUsername = u"username";
const std::u16string kPassword = u"password";
const std::u16string kSenderEmail = u"sender@example.com";
const std::u16string kSenderName = u"Sender Name";

IncomingSharingInvitation CreateIncomingSharingInvitation() {
  IncomingSharingInvitation invitation;
  invitation.url = GURL(kUrl);
  invitation.signon_realm = invitation.url.spec();
  invitation.username_value = kUsername;
  invitation.password_value = kPassword;
  invitation.sender_email = kSenderEmail;
  invitation.sender_display_name = kSenderName;
  return invitation;
}

PasswordForm CreatePasswordForm() {
  PasswordForm form;
  form.url = GURL(kUrl);
  form.signon_realm = form.url.spec();
  form.username_value = kUsername;
  form.password_value = kPassword;
  return form;
}

IncomingSharingInvitation PasswordFormToIncomingSharingInvitation(
    const PasswordForm& form) {
  IncomingSharingInvitation invitation;
  invitation.url = form.url;
  invitation.username_element = form.username_element;
  invitation.username_value = form.username_value;
  invitation.password_element = form.password_element;
  return invitation;
}

}  // namespace

class PasswordReceiverServiceImplTest : public testing::Test {
 public:
  PasswordReceiverServiceImplTest() {
    password_store_ = base::MakeRefCounted<TestPasswordStore>();
    password_store_->Init(/*prefs=*/nullptr,
                          /*affiliated_match_helper=*/nullptr);

    password_receiver_service_ = std::make_unique<PasswordReceiverServiceImpl>(
        /*sync_bridge=*/nullptr, password_store_.get());
  }

  void TearDown() override {
    password_store_->ShutdownOnUIThread();
    testing::Test::TearDown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void AddLoginAndWait(const PasswordForm& form) {
    password_store_->AddLogin(form);
    RunUntilIdle();
  }

  PasswordReceiverService* password_receiver_service() {
    return password_receiver_service_.get();
  }

  TestPasswordStore& password_store() { return *password_store_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> password_store_;
  std::unique_ptr<PasswordReceiverServiceImpl> password_receiver_service_;
};

TEST_F(PasswordReceiverServiceImplTest,
       ShouldAcceptIncomingInvitationWhenStoreIsEmpty) {
  IncomingSharingInvitation invitation = CreateIncomingSharingInvitation();

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  EXPECT_THAT(
      password_store().stored_passwords().at(invitation.url.spec()),
      ElementsAre(AllOf(
          Field(&PasswordForm::signon_realm, GURL(kUrl).spec()),
          Field(&PasswordForm::username_value, kUsername),
          Field(&PasswordForm::password_value, kPassword),
          Field(&PasswordForm::type, PasswordForm::Type::kReceivedViaSharing),
          Field(&PasswordForm::sender_email, kSenderEmail),
          Field(&PasswordForm::sender_name, kSenderName),
          Field(&PasswordForm::sharing_notification_displayed, false))));
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenPasswordAlreadyExists) {
  PasswordForm existing_password = CreatePasswordForm();
  // Mark the password as generated to guarantee that this remains as is and
  // isn't overwritten by a password of type ReceivedViaSharing.
  existing_password.type = PasswordForm::Type::kGenerated;
  existing_password.in_store = PasswordForm::Store::kProfileStore;
  AddLoginAndWait(existing_password);

  // Simulate an incoming invitation for the same stored passwords.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The store should contain the `existing_password` and the
  // incoming invitation is ignored.
  EXPECT_THAT(password_store().stored_passwords().at(invitation.url.spec()),
              ElementsAre(existing_password));
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenConflictingPasswordExists) {
  IncomingSharingInvitation invitation = CreateIncomingSharingInvitation();
  PasswordForm conflicting_password =
      IncomingSharingInvitationToPasswordForm(invitation);
  conflicting_password.password_value = u"AnotherPassword";
  conflicting_password.in_store = PasswordForm::Store::kProfileStore;
  AddLoginAndWait(conflicting_password);

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  EXPECT_THAT(password_store().stored_passwords().at(invitation.url.spec()),
              ElementsAre(conflicting_password));
}

}  // namespace password_manager
