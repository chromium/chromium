// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
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
    profile_password_store_ = base::MakeRefCounted<TestPasswordStore>();
    profile_password_store_->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);

    account_password_store_ = base::MakeRefCounted<TestPasswordStore>();
    account_password_store_->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);

    password_receiver_service_ = std::make_unique<PasswordReceiverServiceImpl>(
        &pref_service_,
        base::BindRepeating(
            [](syncer::SyncService* sync_service) { return sync_service; },
            &sync_service_),
        /*sync_bridge=*/nullptr, profile_password_store_.get(),
        account_password_store_.get());
  }

  void SetUp() override {
    testing::Test::SetUp();
    // Set the user to be syncing passwords
    CoreAccountInfo account;
    account.email = "user@account.com";
    account.gaia = "user";
    account.account_id = CoreAccountId::FromGaiaId(account.gaia);
    sync_service().SetAccountInfo(account);
    sync_service().SetHasSyncConsent(true);
    sync_service().SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service().SetDisableReasons({});
    sync_service().GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPasswords, true);
  }

  void TearDown() override {
    account_password_store_->ShutdownOnUIThread();
    profile_password_store_->ShutdownOnUIThread();
    testing::Test::TearDown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void AddLoginAndWait(const PasswordForm& form,
                       TestPasswordStore& password_store) {
    password_store.AddLogin(form);
    RunUntilIdle();
  }

  PasswordReceiverService* password_receiver_service() {
    return password_receiver_service_.get();
  }

  TestPasswordStore& profile_password_store() {
    return *profile_password_store_;
  }

  TestPasswordStore& account_password_store() {
    return *account_password_store_;
  }

  TestingPrefServiceSimple& pref_service() { return pref_service_; }
  syncer::TestSyncService& sync_service() { return sync_service_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<TestPasswordStore> profile_password_store_;
  scoped_refptr<TestPasswordStore> account_password_store_;
  std::unique_ptr<PasswordReceiverServiceImpl> password_receiver_service_;
};

TEST_F(PasswordReceiverServiceImplTest,
       ShouldAcceptIncomingInvitationWhenStoreIsEmpty) {
  IncomingSharingInvitation invitation = CreateIncomingSharingInvitation();

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  EXPECT_THAT(
      profile_password_store().stored_passwords().at(invitation.url.spec()),
      ElementsAre(AllOf(
          Field(&PasswordForm::signon_realm, GURL(kUrl).spec()),
          Field(&PasswordForm::username_value, kUsername),
          Field(&PasswordForm::password_value, kPassword),
          Field(&PasswordForm::type, PasswordForm::Type::kReceivedViaSharing),
          Field(&PasswordForm::sender_email, kSenderEmail),
          Field(&PasswordForm::sender_name, kSenderName),
          Field(&PasswordForm::sharing_notification_displayed, false))));

  EXPECT_TRUE(account_password_store().stored_passwords().empty());
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenPasswordAlreadyExists) {
  PasswordForm existing_password = CreatePasswordForm();
  // Mark the password as generated to guarantee that this remains as is and
  // isn't overwritten by a password of type ReceivedViaSharing.
  existing_password.type = PasswordForm::Type::kGenerated;
  existing_password.in_store = PasswordForm::Store::kProfileStore;
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored passwords.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The store should contain the `existing_password` and the
  // incoming invitation is ignored.
  EXPECT_THAT(
      profile_password_store().stored_passwords().at(invitation.url.spec()),
      ElementsAre(existing_password));
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenConflictingPasswordExists) {
  IncomingSharingInvitation invitation = CreateIncomingSharingInvitation();
  PasswordForm conflicting_password =
      IncomingSharingInvitationToPasswordForm(invitation);
  conflicting_password.password_value = u"AnotherPassword";
  conflicting_password.in_store = PasswordForm::Store::kProfileStore;
  AddLoginAndWait(conflicting_password, profile_password_store());

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  EXPECT_THAT(
      profile_password_store().stored_passwords().at(invitation.url.spec()),
      ElementsAre(conflicting_password));
}

TEST_F(
    PasswordReceiverServiceImplTest,
    ShouldAcceptIncomingInvitationInAccountStoreForOptedInAccountStoreUsers) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return;
  }

  ASSERT_TRUE(profile_password_store().stored_passwords().empty());
  ASSERT_TRUE(account_password_store().stored_passwords().empty());

  // Setup an account store user:
  sync_service().SetHasSyncConsent(false);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  pref_service().registry()->RegisterDictionaryPref(
      password_manager::prefs::kAccountStoragePerAccountSettings);
  features_util::OptInToAccountStorage(&pref_service(), &sync_service());
#else
  sync_service().GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

  password_receiver_service()->ProcessIncomingSharingInvitation(
      CreateIncomingSharingInvitation());

  RunUntilIdle();

  EXPECT_TRUE(profile_password_store().stored_passwords().empty());
  EXPECT_EQ(1U, account_password_store().stored_passwords().size());
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldNotAcceptIncomingInvitationForNonOptedInAccountStoreUsers) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return;
  }

  ASSERT_TRUE(profile_password_store().stored_passwords().empty());
  ASSERT_TRUE(account_password_store().stored_passwords().empty());

  // Setup a signed-in user that opted-out from using the account store:
  sync_service().SetHasSyncConsent(false);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  pref_service().registry()->RegisterDictionaryPref(
      password_manager::prefs::kAccountStoragePerAccountSettings);
  features_util::OptOutOfAccountStorageAndClearSettings(&pref_service(),
                                                        &sync_service());
#else
  sync_service().GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

  password_receiver_service()->ProcessIncomingSharingInvitation(
      CreateIncomingSharingInvitation());

  RunUntilIdle();

  EXPECT_TRUE(profile_password_store().stored_passwords().empty());
  EXPECT_TRUE(account_password_store().stored_passwords().empty());
}

}  // namespace password_manager
