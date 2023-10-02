// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
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
  invitation.signon_realm = form.signon_realm;
  invitation.username_element = form.username_element;
  invitation.username_value = form.username_value;
  invitation.password_element = form.password_element;
  invitation.password_value = form.password_value;
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
        /*sync_bridge=*/nullptr, profile_password_store_.get(),
        account_password_store_.get());
    password_receiver_service_->OnSyncServiceInitialized(&sync_service_);
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
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kInvitationAutoApproved,
      1);
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenPasswordAlreadyExists) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kCredentialsExistWithSamePassword,
      1);
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenConflictingPasswordExists) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndDifferentPassword,
      1);
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
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kNoPasswordStore,
      1);
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldRecordWhenSharedPasswordAlreadyExistsWithDifferentPassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.password_value = u"another_password";

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kCredentialsExistWithDifferentPassword,
      1);
}

TEST_F(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromSameSenderWithSamePassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.sender_email = existing_password.sender_email;

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndSamePassword,
      1);
}

TEST_F(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromDifferentSenderWithSamePassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.sender_email = u"another_user@example.com";

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithDifferentSenderAndSamePassword,
      1);
}

TEST_F(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromSameSenderWithDifferentPassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.sender_email = existing_password.sender_email;
  invitation.password_value = u"another_password";

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndDifferentPassword,
      1);
}

TEST_F(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromDifferentSenderWithDifferentPassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.sender_email = u"another_user@example.com";
  invitation.password_value = u"another_password";

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithDifferentSenderAndDifferentPassword,
      1);
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldIgnorePasswordUpdatesFromSameSenderWhenAutoApproveDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutoApproveSharedPasswordUpdatesFromSameSender);

  base::HistogramTester histogram_tester;
  const std::u16string kNewPassword = u"new_password";
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password from the same sender.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.sender_email = existing_password.sender_email;
  invitation.password_value = kNewPassword;

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The password value should remain kPassword.
  EXPECT_THAT(
      profile_password_store().stored_passwords().at(invitation.url.spec()),
      ElementsAre(AllOf(Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kPassword),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing))));
}

TEST_F(PasswordReceiverServiceImplTest,
       ShouldAcceptPasswordUpdatesFromSameSenderWhenAutoApproveEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutoApproveSharedPasswordUpdatesFromSameSender);

  base::HistogramTester histogram_tester;
  const std::u16string kNewPassword = u"new_password";
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password from the same sender.
  IncomingSharingInvitation invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.sender_email = existing_password.sender_email;
  invitation.password_value = kNewPassword;

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The password value should have been updated to kNewPassword.
  EXPECT_THAT(
      profile_password_store().stored_passwords().at(invitation.url.spec()),
      ElementsAre(AllOf(Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kNewPassword),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing))));
}

}  // namespace password_manager
