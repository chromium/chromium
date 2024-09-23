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
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::AllOf;
using testing::Bool;
using testing::Combine;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;

constexpr std::string_view kUrl = "https://www.test.com";
constexpr std::string_view kPslMatchUrl = "https://m.test.com";
constexpr std::string_view kGroupedMatchUrl = "https://grouped.match.com/";
constexpr std::u16string_view kUsername = u"username";
constexpr std::u16string_view kPassword = u"password";
constexpr std::u16string_view kSenderEmail = u"sender@example.com";
constexpr std::u16string_view kSenderName = u"Sender Name";
constexpr std::string_view kSenderProfileImagerUrl =
    "https://sender.com/avatar";

// Creates an invitation that represents a group of passwords.
sync_pb::IncomingPasswordSharingInvitationSpecifics
CreateIncomingSharingInvitation() {
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation;
  sync_pb::PasswordSharingInvitationData::PasswordGroupData*
      password_group_data = invitation.mutable_client_only_unencrypted_data()
                                ->mutable_password_group_data();
  password_group_data->set_username_value(base::UTF16ToUTF8(kUsername));
  password_group_data->set_password_value(base::UTF16ToUTF8(kPassword));

  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      element_data = password_group_data->add_element_data();
  element_data->set_origin(std::string(kUrl));
  element_data->set_signon_realm(std::string(kUrl));

  sync_pb::UserDisplayInfo* sender_info =
      invitation.mutable_sender_info()->mutable_user_display_info();
  sender_info->set_email(base::UTF16ToUTF8(kSenderEmail));
  sender_info->set_display_name(base::UTF16ToUTF8(kSenderName));
  sender_info->set_profile_image_url(std::string(kSenderProfileImagerUrl));
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
PasswordFormToIncomingSharingInvitation(const PasswordForm& form) {
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

// See GetEnableAccountStoreTestParam() for the meaning of the parameters.
class PasswordReceiverServiceImplTest : public testing::TestWithParam<bool> {
 public:
  PasswordReceiverServiceImplTest() {
    // Initialize `AffiliatedMatchHelper` for the password store that will be
    // used for syncing.
    auto profile_store_match_helper =
        std::make_unique<MockAffiliatedMatchHelper>(&affiliation_service_);
    mock_affiliated_match_helper_ = profile_store_match_helper.get();
    std::unique_ptr<MockAffiliatedMatchHelper> account_store_match_helper;
#if BUILDFLAG(IS_ANDROID)
    if (GetEnableAccountStoreTestParam()) {
      account_store_match_helper.swap(profile_store_match_helper);
    }
#endif  // BUILDFLAG(IS_ANDROID)

    profile_password_store_ = base::MakeRefCounted<TestPasswordStore>();
    profile_password_store_->Init(
        /*prefs=*/nullptr,
        /*affiliated_match_helper=*/std::move(profile_store_match_helper));

    if (GetEnableAccountStoreTestParam()) {
      account_password_store_ = base::MakeRefCounted<TestPasswordStore>();
      account_password_store_->Init(
          /*prefs=*/nullptr,
          /*affiliated_match_helper=*/std::move(account_store_match_helper));
    }
#if BUILDFLAG(IS_ANDROID)
    const auto upm_pref_value =
        GetEnableAccountStoreTestParam()
            ? password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn
            : password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff;
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(upm_pref_value));
#endif  // BUILDFLAG(IS_ANDROID)

    password_receiver_service_ = std::make_unique<PasswordReceiverServiceImpl>(
        &pref_service_,
        /*sync_bridge=*/nullptr, profile_password_store_.get(),
        account_password_store_.get());
    password_receiver_service_->OnSyncServiceInitialized(&sync_service_);
  }

  void SetUp() override {
    testing::Test::SetUp();
    // Set the user to be syncing passwords.
    sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  }

  void TearDown() override {
    mock_affiliated_match_helper_ = nullptr;
    if (account_password_store_) {
      account_password_store_->ShutdownOnUIThread();
    }
    profile_password_store_->ShutdownOnUIThread();
    testing::Test::TearDown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void AddLoginAndWait(const PasswordForm& form,
                       TestPasswordStore& password_store) {
    password_store.AddLogin(form);
    RunUntilIdle();
  }

  // Returns the origin of the first element in the credential group shared in
  // the invitation.
  std::string GetInvitationOrigin(
      const sync_pb::IncomingPasswordSharingInvitationSpecifics& invitation) {
    return invitation.client_only_unencrypted_data()
        .password_group_data()
        .element_data(0)
        .origin();
  }

  // Sets ths `password_value` in the `invitation`.
  void SetPasswordValueInInvitation(
      const std::u16string& password_value,
      sync_pb::IncomingPasswordSharingInvitationSpecifics& invitation) {
    invitation.mutable_client_only_unencrypted_data()
        ->mutable_password_group_data()
        ->set_password_value(base::UTF16ToUTF8(password_value));
  }

  PasswordReceiverService* password_receiver_service() {
    return password_receiver_service_.get();
  }

  // The PasswordStore where syncing users should store shared passwords.
  TestPasswordStore& expected_password_store_for_syncing() {
#if BUILDFLAG(IS_ANDROID)
    return GetEnableAccountStoreTestParam() ? account_password_store()
                                            : profile_password_store();
#else
    return profile_password_store();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // The PasswordStore where syncing users should NOT store shared passwords.
  TestPasswordStore& unexpected_password_store_for_syncing() {
    EXPECT_TRUE(GetEnableAccountStoreTestParam())
        << "unexpected_password_store_for_syncing() must only be called if "
           "there are 2 PasswordStores";
#if BUILDFLAG(IS_ANDROID)
    return profile_password_store();
#else
    return account_password_store();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  TestPasswordStore& profile_password_store() {
    return *profile_password_store_;
  }

  TestPasswordStore& account_password_store() {
    return *account_password_store_;
  }

  MockAffiliatedMatchHelper& affiliated_match_helper() {
    return *mock_affiliated_match_helper_;
  }

  TestingPrefServiceSimple& pref_service() { return pref_service_; }
  syncer::TestSyncService& sync_service() { return sync_service_; }

  // Whether the test should enable the account-scoped PasswordStore.
  bool GetEnableAccountStoreTestParam() { return GetParam(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<TestPasswordStore> profile_password_store_;
  scoped_refptr<TestPasswordStore> account_password_store_;
  std::unique_ptr<PasswordReceiverServiceImpl> password_receiver_service_;
  affiliations::FakeAffiliationService affiliation_service_;
  raw_ptr<MockAffiliatedMatchHelper> mock_affiliated_match_helper_;
};

TEST_P(PasswordReceiverServiceImplTest,
       ShouldAcceptIncomingInvitationWhenStoreIsEmpty) {
  if (!GetEnableAccountStoreTestParam()) {
    return;
  }

  base::HistogramTester histogram_tester;
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      CreateIncomingSharingInvitation();

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      GetInvitationOrigin(invitation)));
  EXPECT_THAT(
      expected_password_store_for_syncing().stored_passwords().at(
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

  EXPECT_TRUE(
      unexpected_password_store_for_syncing().stored_passwords().empty());

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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

  // Simulate an incoming invitation for the same stored passwords.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(existing_password);
  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The store should contain the `existing_password` and the
  // incoming invitation is ignored.
  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      GetInvitationOrigin(invitation)));
  EXPECT_THAT(expected_password_store_for_syncing().stored_passwords().at(
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

  AddLoginAndWait(conflicting_password, expected_password_store_for_syncing());

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      GetInvitationOrigin(invitation)));
  EXPECT_THAT(expected_password_store_for_syncing().stored_passwords().at(
                  GetInvitationOrigin(invitation)),
              ElementsAre(conflicting_password));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kSharedCredentialsExistWithSameSenderAndDifferentPassword,
      1);
}

TEST_P(PasswordReceiverServiceImplTest,
       ShouldAcceptInvitationForNonSyncingUserOptedInToAccountStore) {
  if (!GetEnableAccountStoreTestParam()) {
    return;
  }

  ASSERT_TRUE(profile_password_store().stored_passwords().empty());
  ASSERT_TRUE(account_password_store().stored_passwords().empty());

  // Set up an account store user (a non-syncing one, but that doesn't really
  // matter).
  base::test::ScopedFeatureList feature_list(
      syncer::kEnablePasswordsAccountStorageForNonSyncingUsers);
  sync_service().SetSignedIn(signin::ConsentLevel::kSignin);
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
       ShouldNotAcceptInvitationForNonSyncingUserOptedOutOfAccountStore) {
  base::HistogramTester histogram_tester;
  if (!GetEnableAccountStoreTestParam()) {
    return;
  }

  ASSERT_TRUE(profile_password_store().stored_passwords().empty());
  ASSERT_TRUE(account_password_store().stored_passwords().empty());

  // Setup a signed-in user that opted-out from using the account store:
  sync_service().SetSignedIn(signin::ConsentLevel::kSignin);
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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      GetInvitationOrigin(invitation)));
  EXPECT_THAT(
      expected_password_store_for_syncing().stored_passwords().at(
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
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

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
  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      GetInvitationOrigin(invitation)));
  EXPECT_THAT(
      expected_password_store_for_syncing().stored_passwords().at(
          GetInvitationOrigin(invitation)),
      ElementsAre(AllOf(Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kNewPassword),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing))));
}

TEST_P(PasswordReceiverServiceImplTest, ShouldAddAllCredentialsInInvitation) {
  base::HistogramTester histogram_tester;
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      CreateIncomingSharingInvitation();
  // Add another origin to the same invitation.
  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      element_data = invitation.mutable_client_only_unencrypted_data()
                         ->mutable_password_group_data()
                         ->add_element_data();
  element_data->set_origin(std::string(kPslMatchUrl));
  element_data->set_signon_realm(std::string(kPslMatchUrl));

  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // Both origins in the invitation using the modern format should have been
  // added to the store. The one in the legacy format should be ignored.
  EXPECT_EQ(expected_password_store_for_syncing().stored_passwords().size(),
            2U);

  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      std::string(kUrl)));
  EXPECT_THAT(
      expected_password_store_for_syncing().stored_passwords().at(
          std::string(kUrl)),
      ElementsAre(AllOf(Field(&PasswordForm::signon_realm, std::string(kUrl)),
                        Field(&PasswordForm::username_value, kUsername),
                        Field(&PasswordForm::password_value, kPassword))));

  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      kPslMatchUrl));
  EXPECT_THAT(expected_password_store_for_syncing().stored_passwords().at(
                  std::string(kPslMatchUrl)),
              ElementsAre(AllOf(
                  Field(&PasswordForm::signon_realm, std::string(kPslMatchUrl)),
                  Field(&PasswordForm::username_value, kUsername),
                  Field(&PasswordForm::password_value, kPassword))));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kInvitationAutoApproved,
      2);
}

TEST_P(PasswordReceiverServiceImplTest, ShouldIgnoreInvalidPasswordForm) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.password_value.clear();

  password_receiver_service()->ProcessIncomingSharingInvitation(
      PasswordFormToIncomingSharingInvitation(existing_password));
  RunUntilIdle();

  EXPECT_THAT(expected_password_store_for_syncing().stored_passwords(),
              IsEmpty());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kInvalidInvitation,
      1);
}

TEST_P(PasswordReceiverServiceImplTest, ShouldIgnoreGroupedCredentials) {
  base::HistogramTester histogram_tester;
  PasswordForm existing_password = CreatePasswordForm();
  existing_password.scheme = PasswordForm::Scheme::kHtml;
  existing_password.url = GURL(kGroupedMatchUrl);
  existing_password.signon_realm = existing_password.url.spec();
  existing_password.in_store = PasswordForm::Store::kProfileStore;
  AddLoginAndWait(existing_password, expected_password_store_for_syncing());

  PasswordForm shared_form = CreatePasswordForm();
  PasswordFormDigest digest = PasswordFormDigest(shared_form);
  affiliated_match_helper().ExpectCallToGetAffiliatedAndGrouped(
      digest, {std::string(kUrl)}, {std::string(kGroupedMatchUrl)});
  // Simulate an incoming invitation for the same stored passwords.
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      PasswordFormToIncomingSharingInvitation(shared_form);
  password_receiver_service()->ProcessIncomingSharingInvitation(invitation);

  RunUntilIdle();

  // The store should contain the `existing_password` and the
  // incoming invitation is ignored.
  ASSERT_TRUE(expected_password_store_for_syncing().stored_passwords().contains(
      GetInvitationOrigin(invitation)));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      metrics_util::ProcessIncomingPasswordSharingInvitationResult::
          kInvitationAutoApproved,
      1);
}

INSTANTIATE_TEST_SUITE_P(, PasswordReceiverServiceImplTest, Bool());

}  // namespace password_manager
