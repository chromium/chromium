// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
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
using testing::Bool;
using testing::ElementsAre;
using testing::Field;

const std::string kUrl = "https://www.test.com";
const std::string kPslMatchUrl = "https://m.test.com";
const std::u16string kUsername = u"username";
const std::u16string kPassword = u"password";
const std::u16string kSenderEmail = u"sender@example.com";
const std::u16string kSenderName = u"Sender Name";
const std::string kSenderProfileImagerUrl = "https://sender.com/avatar";

// Creates an invitation that represents only one password
sync_pb::IncomingPasswordSharingInvitationSpecifics
CreateLegacyIncomingSharingInvitation() {
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation;
  sync_pb::PasswordSharingInvitationData::PasswordData* password_data =
      invitation.mutable_client_only_unencrypted_data()
          ->mutable_password_data();
  password_data->set_origin(kUrl);
  password_data->set_signon_realm(kUrl);
  password_data->set_username_value(base::UTF16ToUTF8(kUsername));
  password_data->set_password_value(base::UTF16ToUTF8(kPassword));

  sync_pb::UserDisplayInfo* sender_info =
      invitation.mutable_sender_info()->mutable_user_display_info();
  sender_info->set_email(base::UTF16ToUTF8(kSenderEmail));
  sender_info->set_display_name(base::UTF16ToUTF8(kSenderName));
  sender_info->set_profile_image_url(kSenderProfileImagerUrl);
  return invitation;
}

// Creates an invitation that represents a group of passwords.
sync_pb::IncomingPasswordSharingInvitationSpecifics
CreateModernIncomingSharingInvitation() {
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation;
  sync_pb::PasswordSharingInvitationData::PasswordGroupData*
      password_group_data = invitation.mutable_client_only_unencrypted_data()
                                ->mutable_password_group_data();
  password_group_data->set_username_value(base::UTF16ToUTF8(kUsername));
  password_group_data->set_password_value(base::UTF16ToUTF8(kPassword));

  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      element_data = password_group_data->add_element_data();
  element_data->set_origin(kUrl);
  element_data->set_signon_realm(kUrl);

  sync_pb::UserDisplayInfo* sender_info =
      invitation.mutable_sender_info()->mutable_user_display_info();
  sender_info->set_email(base::UTF16ToUTF8(kSenderEmail));
  sender_info->set_display_name(base::UTF16ToUTF8(kSenderName));
  sender_info->set_profile_image_url(kSenderProfileImagerUrl);
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

sync_pb::IncomingPasswordSharingInvitationSpecifics
PasswordFormToLegacyIncomingSharingInvitation(const PasswordForm& form) {
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation;
  sync_pb::PasswordSharingInvitationData::PasswordData* password_data =
      invitation.mutable_client_only_unencrypted_data()
          ->mutable_password_data();
  password_data->set_origin(form.url.spec());
  password_data->set_signon_realm(form.signon_realm);
  password_data->set_username_element(base::UTF16ToUTF8(form.username_element));
  password_data->set_username_value(base::UTF16ToUTF8(form.username_value));
  password_data->set_password_element(base::UTF16ToUTF8(form.password_element));
  password_data->set_password_value(base::UTF16ToUTF8(form.password_value));
  return invitation;
}

sync_pb::IncomingPasswordSharingInvitationSpecifics
PasswordFormToModernIncomingSharingInvitation(const PasswordForm& form) {
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation;
  sync_pb::PasswordSharingInvitationData::PasswordGroupData*
      password_group_data = invitation.mutable_client_only_unencrypted_data()
                                ->mutable_password_group_data();
  password_group_data->set_username_value(
      base::UTF16ToUTF8(form.username_value));
  password_group_data->set_password_value(
      base::UTF16ToUTF8(form.password_value));

  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      element_data = password_group_data->add_element_data();
  element_data->set_origin(form.url.spec());
  element_data->set_signon_realm(form.signon_realm);
  element_data->set_username_element(base::UTF16ToUTF8(form.username_element));
  element_data->set_password_element(base::UTF16ToUTF8(form.password_element));
  return invitation;
}

}  // namespace

// Test param decides whether the test should use the legacy invitation proto
// format that represent one credential, or the modern format that represents a
// group of credentials.
class PasswordReceiverServiceImplTest : public testing::TestWithParam<bool> {
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

  sync_pb::IncomingPasswordSharingInvitationSpecifics
  CreateIncomingSharingInvitation() {
    return GetParam() ? CreateModernIncomingSharingInvitation()
                      : CreateLegacyIncomingSharingInvitation();
  }

  sync_pb::IncomingPasswordSharingInvitationSpecifics
  PasswordFormToIncomingSharingInvitation(const PasswordForm& form) {
    return GetParam() ? PasswordFormToModernIncomingSharingInvitation(form)
                      : PasswordFormToLegacyIncomingSharingInvitation(form);
  }

  // Returns the origin of the credentials shared in the invitation. If the
  // invitation represents a group of credentials, it returns the origin of the
  // first one.
  std::string GetInvitationOrigin(
      const sync_pb::IncomingPasswordSharingInvitationSpecifics& invitation) {
    if (GetParam()) {
      CHECK(
          invitation.client_only_unencrypted_data().has_password_group_data());
      return invitation.client_only_unencrypted_data()
          .password_group_data()
          .element_data(0)
          .origin();
    }
    CHECK(invitation.client_only_unencrypted_data().has_password_data());
    return invitation.client_only_unencrypted_data().password_data().origin();
  }

  // Sets ths `password_value` in the `invitation`, using either the credential
  // or the group invitation format.
  void SetPasswordValueInInvitation(
      const std::u16string& password_value,
      sync_pb::IncomingPasswordSharingInvitationSpecifics& invitation) {
    if (GetParam()) {
      invitation.mutable_client_only_unencrypted_data()
          ->mutable_password_group_data()
          ->set_password_value(base::UTF16ToUTF8(password_value));
      return;
    }
    invitation.mutable_client_only_unencrypted_data()
        ->mutable_password_data()
        ->set_password_value(base::UTF16ToUTF8(password_value));
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

TEST_P(PasswordReceiverServiceImplTest,
       ShouldAcceptIncomingInvitationWhenStoreIsEmpty) {
  base::HistogramTester histogram_tester;
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      CreateIncomingSharingInvitation();

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  EXPECT_THAT(
      profile_password_store().stored_passwords().at(
          GetInvitationOrigin(invitation)),
      ElementsAre(AllOf(
          Field(&PasswordForm::signon_realm, kUrl),
          Field(&PasswordForm::username_value, kUsername),
          Field(&PasswordForm::password_value, kPassword),
          Field(&PasswordForm::type, PasswordForm::Type::kReceivedViaSharing),
          Field(&PasswordForm::sender_email, kSenderEmail),
          Field(&PasswordForm::sender_name, kSenderName),
          Field(&PasswordForm::sender_profile_image_url,
                GURL(kSenderProfileImagerUrl)),
          Field(&PasswordForm::sharing_notification_displayed, false))));

  EXPECT_TRUE(account_password_store().stored_passwords().empty());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kInvitationAutoApproved,
      1);
}

TEST_P(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenPasswordAlreadyExists) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  // Mark the password as generated to guarantee that this remains as is and
  // isn't overwritten by a password of type ReceivedViaSharing.
  existing_password.type = PasswordForm::Type::kGenerated;
  existing_password.in_store = PasswordForm::Store::kProfileStore;
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored passwords.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The store should contain the `existing_password` and the
  // incoming invitation is ignored.
  EXPECT_THAT(profile_password_store().stored_passwords().at(
                  GetInvitationOrigin(invitation)),
              ElementsAre(existing_password));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kCredentialsExistWithSamePassword,
      1);
}

TEST_P(PasswordReceiverServiceImplTest,
       ShouldIgnoreIncomingInvitationWhenConflictingPasswordExists) {
  base::HistogramTester histogram_tester;
  PasswordForm password = CreatePasswordForm();
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(password);

  PasswordForm conflicting_password = password;
  conflicting_password.type = PasswordForm::Type::kReceivedViaSharing;
  conflicting_password.password_value = u"AnotherPassword";
  conflicting_password.in_store = PasswordForm::Store::kProfileStore;

  AddLoginAndWait(conflicting_password, profile_password_store());

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  EXPECT_THAT(profile_password_store().stored_passwords().at(
                  GetInvitationOrigin(invitation)),
              ElementsAre(conflicting_password));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndDifferentPassword,
      1);
}

TEST_P(
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

TEST_P(PasswordReceiverServiceImplTest,
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

TEST_P(PasswordReceiverServiceImplTest,
       ShouldRecordWhenSharedPasswordAlreadyExistsWithDifferentPassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);

  SetPasswordValueInInvitation(u"another_password", invitation);

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kCredentialsExistWithDifferentPassword,
      1);
}

TEST_P(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromSameSenderWithSamePassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.mutable_sender_info()->mutable_user_display_info()->set_email(
      base::UTF16ToUTF8(existing_password.sender_email));

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndSamePassword,
      1);
}

TEST_P(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromDifferentSenderWithSamePassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.mutable_sender_info()->mutable_user_display_info()->set_email(
      "another_user@example.com");

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithDifferentSenderAndSamePassword,
      1);
}

TEST_P(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromSameSenderWithDifferentPassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.mutable_sender_info()->mutable_user_display_info()->set_email(
      base::UTF16ToUTF8(existing_password.sender_email));
  SetPasswordValueInInvitation(u"another_password", invitation);

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndDifferentPassword,
      1);
}

TEST_P(
    PasswordReceiverServiceImplTest,
    ShouldRecordWhenSharedPasswordAlreadyExistsAsSharedFromDifferentSenderWithDifferentPassword) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.type = PasswordForm::Type::kReceivedViaSharing;
  existing_password.sender_email = u"user@example.com";
  AddLoginAndWait(existing_password, profile_password_store());

  // Simulate an incoming invitation for the same stored credentials with a
  // different password.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.mutable_sender_info()->mutable_user_display_info()->set_email(
      "another_user@example.com");
  SetPasswordValueInInvitation(u"another_password", invitation);

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithDifferentSenderAndDifferentPassword,
      1);
}

TEST_P(PasswordReceiverServiceImplTest,
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
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.mutable_sender_info()->mutable_user_display_info()->set_email(
      base::UTF16ToUTF8(existing_password.sender_email));
  SetPasswordValueInInvitation(kNewPassword, invitation);

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The password value should remain kPassword.
  EXPECT_THAT(
      profile_password_store().stored_passwords().at(
          GetInvitationOrigin(invitation)),
      ElementsAre(AllOf(Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kPassword),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing))));
}

TEST_P(PasswordReceiverServiceImplTest,
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
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  invitation.mutable_sender_info()->mutable_user_display_info()->set_email(
      base::UTF16ToUTF8(existing_password.sender_email));
  SetPasswordValueInInvitation(kNewPassword, invitation);

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The password value should have been updated to kNewPassword.
  EXPECT_THAT(
      profile_password_store().stored_passwords().at(
          GetInvitationOrigin(invitation)),
      ElementsAre(AllOf(Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kNewPassword),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing))));
}

TEST_P(PasswordReceiverServiceImplTest, ShouldAddAllCredentialsInInvitation) {
  // This test is relevant only for the modern invitation proto format.
  if (!GetParam()) {
    return;
  }
  base::HistogramTester histogram_tester;
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      CreateIncomingSharingInvitation();
  // Add another origin to the same invitation.
  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      element_data = invitation.mutable_client_only_unencrypted_data()
                         ->mutable_password_group_data()
                         ->add_element_data();
  element_data->set_origin(kPslMatchUrl);
  element_data->set_signon_realm(kPslMatchUrl);

  // Add credentials using the legacy proto format that doesn't support password
  // group. This should be ignored.
  sync_pb::PasswordSharingInvitationData::PasswordData* password_data =
      invitation.mutable_client_only_unencrypted_data()
          ->mutable_password_data();
  password_data->set_origin("https:/www.to-be-ignored.com");
  password_data->set_signon_realm("https:/www.to-be-ignored.com");
  password_data->set_username_value(base::UTF16ToUTF8(kUsername));
  password_data->set_password_value(base::UTF16ToUTF8(kPassword));

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // Both origins in the invitation using the modern format should have been
  // added to the store. The one in the legacy format should be ignored.
  EXPECT_EQ(profile_password_store().stored_passwords().size(), 2U);

  EXPECT_THAT(
      profile_password_store().stored_passwords().at(kUrl),
      ElementsAre(AllOf(Field(&PasswordForm::signon_realm, kUrl),
                        Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kPassword))));

  EXPECT_THAT(
      profile_password_store().stored_passwords().at(kPslMatchUrl),
      ElementsAre(AllOf(Field(&PasswordForm::signon_realm, kPslMatchUrl),
                        Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kPassword))));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kInvitationAutoApproved,
      2);
}

INSTANTIATE_TEST_SUITE_P(, PasswordReceiverServiceImplTest, Bool());

}  // namespace password_manager
