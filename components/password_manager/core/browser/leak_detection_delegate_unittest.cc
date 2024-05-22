// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.url = GURL("http://www.example.com/a/LoginAuth");
  form.username_value = u"Adam";
  form.password_value = u"p4ssword";
  form.signon_realm = "http://www.example.com/";
  return form;
}

GURL GetTestUrl() {
  return GURL("https://example.com");
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(bool, IsOffTheRecord, (), (const override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const override));
  MOCK_METHOD(void,
              NotifyUserCredentialsWereLeaked,
              (password_manager::CredentialLeakType,
               const GURL&,
               const std::u16string&,
               bool in_account_store),
              (override));
  MOCK_METHOD(const syncer::SyncService*, GetSyncService, (), (const override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const override));
  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
};

class MockLeakDetectionCheck : public LeakDetectionCheck {
 public:
  MOCK_METHOD(
      void,
      Start,
      (LeakDetectionInitiator, const GURL&, std::u16string, std::u16string),
      (override));
};

}  // namespace

class LeakDetectionDelegateTest : public testing::Test {
 public:
  explicit LeakDetectionDelegateTest(
      const std::vector<base::test::FeatureRef>& enabled_features) {
    features_.InitWithFeatures(enabled_features, {});

    auto mock_factory =
        std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
    mock_factory_ = mock_factory.get();
    delegate_.set_leak_factory(std::move(mock_factory));
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPasswordLeakDetectionEnabled, true);
    pref_service_->registry()->RegisterBooleanPref(
        ::prefs::kSafeBrowsingEnabled, true);
    pref_service_->registry()->RegisterBooleanPref(
        ::prefs::kSafeBrowsingEnhanced, false);
#if BUILDFLAG(IS_ANDROID)
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOff));
#endif  // BUILDFLAG(IS_ANDROID)
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(pref_service()));
  }

  LeakDetectionDelegateTest()
      : LeakDetectionDelegateTest(std::vector<base::test::FeatureRef>()) {}

  ~LeakDetectionDelegateTest() override = default;

  MockPasswordManagerClient& client() { return client_; }
  MockLeakDetectionCheckFactory& factory() { return *mock_factory_; }
  LeakDetectionDelegate& delegate() { return delegate_; }
  syncer::TestSyncService* sync_service() { return &test_sync_service_; }
  MockPasswordStoreInterface* profile_store() {
    return mock_profile_store_.get();
  }
  MockPasswordStoreInterface* account_store() {
    return mock_account_store_.get();
  }
  PrefService* pref_service() { return pref_service_.get(); }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  void SetSBState(safe_browsing::SafeBrowsingState state) {
    switch (state) {
      case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnhanced, true);
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnabled, true);
        break;
      case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnhanced, false);
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnabled, true);
        break;
      case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      default:
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnhanced, false);
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnabled, false);
        break;
    }
  }

  // Sets kSafeBrowsingAllowlistDomains policy which controls for what domains
  // leak check is blocked
  void SetBlockedDomain(std::string_view domain) {
    pref_service_->registry()->RegisterListPref(
        ::prefs::kSafeBrowsingAllowlistDomains);
    base::Value::List domains;
    domains.Append(domain);
    pref_service()->SetList(::prefs::kSafeBrowsingAllowlistDomains,
                            std::move(domains));
  }

  void SetLeakDetectionEnabled(bool is_on) {
    pref_service_->SetBoolean(
        password_manager::prefs::kPasswordLeakDetectionEnabled, is_on);
  }

  void ExpectPasswords(std::vector<PasswordForm> password_forms,
                       MockPasswordStoreInterface* store = nullptr) {
    if (!store) {
      store = profile_store();
    }

    PasswordForm::Store in_store =
        (store == account_store() ? PasswordForm::Store::kAccountStore
                                  : PasswordForm::Store::kProfileStore);

    EXPECT_CALL(*store, GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>([password_forms, store, in_store](
                                          base::WeakPtr<PasswordStoreConsumer>
                                              consumer) {
          std::vector<PasswordForm> results;
          for (auto& form : password_forms) {
            results.push_back(std::move(form));
            results.back().in_store = in_store;
          }
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom,
                  consumer, base::Unretained(store), std::move(results)));
        }));
  }

 private:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<syncer::TestSyncService> test_sync_service_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  scoped_refptr<MockPasswordStoreInterface> mock_profile_store_ =
      base::MakeRefCounted<testing::StrictMock<MockPasswordStoreInterface>>();
  scoped_refptr<MockPasswordStoreInterface> mock_account_store_ =
      base::MakeRefCounted<testing::StrictMock<MockPasswordStoreInterface>>();
  LeakDetectionDelegate delegate_{&client_};
  raw_ptr<MockLeakDetectionCheckFactory> mock_factory_ = nullptr;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_ =
      std::make_unique<TestingPrefServiceSimple>();
};

TEST_F(LeakDetectionDelegateTest, InIncognito) {
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(true));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, SafeBrowsingOff) {
  pref_service()->SetBoolean(::prefs::kSafeBrowsingEnabled, false);

  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  const PasswordForm form = CreateTestForm();
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, UsernameIsEmpty) {
  PasswordForm form = CreateTestForm();
  form.username_value.clear();

  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, StartCheck) {
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form.url,
                    form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_TRUE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, DoNotStartCheck) {
  SetLeakDetectionEnabled(false);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, StartCheckWithStandardProtection) {
  SetSBState(safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form.url,
                    form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_TRUE(delegate().leak_check());
  EXPECT_TRUE(LeakDetectionCheck::CanStartLeakCheck(*pref_service(),
                                                    GetTestUrl(), nullptr));
}

TEST_F(LeakDetectionDelegateTest, DontStartLeakCheckWithBlockedFormURL) {
  SetSBState(safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetLeakDetectionEnabled(true);
  SetBlockedDomain("blocked_domain.com");

  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GURL("http://blocked_domain.com"));

  EXPECT_FALSE(delegate().leak_check());
  EXPECT_FALSE(LeakDetectionCheck::CanStartLeakCheck(
      *pref_service(), GURL("http://blocked_domain.com"), nullptr));
}

TEST_F(LeakDetectionDelegateTest, StartCheckWithNonBlockedFormURL) {
  SetSBState(safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetLeakDetectionEnabled(true);
  SetBlockedDomain("blocked_domain.com");

  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form.url,
                    form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GURL("http://not_blocked_domain.com"));

  EXPECT_TRUE(delegate().leak_check());
  EXPECT_TRUE(LeakDetectionCheck::CanStartLeakCheck(
      *pref_service(), GURL("http://not_blocked_domain.com"), nullptr));
}

TEST_F(LeakDetectionDelegateTest, StartCheckWithEnhancedProtection) {
  SetSBState(safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  SetLeakDetectionEnabled(false);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form.url,
                    form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_TRUE(delegate().leak_check());
  EXPECT_TRUE(LeakDetectionCheck::CanStartLeakCheck(*pref_service(),
                                                    GetTestUrl(), nullptr));
}

TEST_F(LeakDetectionDelegateTest, DoNotStartCheckWithoutSafeBrowsing) {
  SetSBState(safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_FALSE(delegate().leak_check());
  EXPECT_FALSE(LeakDetectionCheck::CanStartLeakCheck(*pref_service(),
                                                     GetTestUrl(), nullptr));
}

TEST_F(LeakDetectionDelegateTest, DoNotStartLeakCheckIfLeakCheckIsOff) {
  SetSBState(safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetLeakDetectionEnabled(false);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_FALSE(delegate().leak_check());
  EXPECT_FALSE(LeakDetectionCheck::CanStartLeakCheck(*pref_service(),
                                                     GetTestUrl(), nullptr));
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDoneWithFalseResult) {
  base::HistogramTester histogram_tester;
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(client(), NotifyUserCredentialsWereLeaked).Times(0);
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/false, form.url, form.username_value, form.password_value);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.LeakDetection.NotifyIsLeakedTime", 0);
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDoneWithTrueResult) {
  base::HistogramTester histogram_tester;
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(Return(profile_store()));
  ExpectPasswords({});
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(false), IsReused(false),
                                           IsSyncing(false)),
          form.url, form.username_value, /* in_account_store = */ false));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);
  WaitForPasswordStore();
  histogram_tester.ExpectTotalCount(
      "PasswordManager.LeakDetection.NotifyIsLeakedTime", 1);
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDoneForSyncingUser) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  ON_CALL(client(), GetSyncService()).WillByDefault(Return(sync_service()));
  ON_CALL(client(), GetProfilePasswordStore())
      .WillByDefault(Return(profile_store()));

  ASSERT_EQ(sync_util::GetPasswordSyncState(sync_service()),
            sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_TRUE(
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service()));

  ExpectPasswords({form});
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(true), IsReused(false),
                                           IsSyncing(true)),
          form.url, form.username_value, /* in_account_store = */ false));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  EXPECT_CALL(*profile_store(), UpdateLogin);
  WaitForPasswordStore();
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDoneForAccountStoreUser) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  ON_CALL(client(), GetSyncService()).WillByDefault(Return(sync_service()));
  ON_CALL(client(), GetAccountPasswordStore())
      .WillByDefault(Return(account_store()));
  ON_CALL(client(), GetProfilePasswordStore())
      .WillByDefault(Return(profile_store()));

  sync_service()->GetUserSettings()->ClearInitialSyncFeatureSetupComplete();

  ASSERT_EQ(sync_util::GetPasswordSyncState(sync_service()),
            sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_FALSE(
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service()));

  ExpectPasswords({form}, /*store=*/account_store());
  ExpectPasswords({}, /*store=*/profile_store());
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(true), IsReused(false),
                                           IsSyncing(true)),
          form.url, form.username_value, /* in_account_store = */ true));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  EXPECT_CALL(*account_store(), UpdateLogin);
  WaitForPasswordStore();
}

TEST_F(LeakDetectionDelegateTest,
       LeakDetectionDoneForAccountStoreUserWithCredentialInProfileStore) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  ON_CALL(client(), GetSyncService()).WillByDefault(Return(sync_service()));
  ON_CALL(client(), GetAccountPasswordStore())
      .WillByDefault(Return(account_store()));
  ON_CALL(client(), GetProfilePasswordStore())
      .WillByDefault(Return(profile_store()));

  sync_service()->GetUserSettings()->ClearInitialSyncFeatureSetupComplete();

  ASSERT_EQ(sync_util::GetPasswordSyncState(sync_service()),
            sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_FALSE(
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service()));

  ExpectPasswords({}, /*store=*/account_store());
  ExpectPasswords({form}, /*store=*/profile_store());
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  // The credential is not saved in a store that is synced remotely - therefore
  // `IsSyncing` is false.
  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(true), IsReused(false),
                                           IsSyncing(false)),
          form.url, form.username_value, /* in_account_store = */ false));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  EXPECT_CALL(*profile_store(), UpdateLogin);
  WaitForPasswordStore();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(LeakDetectionDelegateTest,
       LeakDetectionDoneForLocalStoreWithSplitStoresAndUPMForLocal) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  ON_CALL(client(), GetSyncService()).WillByDefault(Return(sync_service()));
  ON_CALL(client(), GetAccountPasswordStore())
      .WillByDefault(Return(account_store()));
  ON_CALL(client(), GetProfilePasswordStore())
      .WillByDefault(Return(profile_store()));

  // Mark that the local and account stores are split.
  pref_service()->SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  ASSERT_EQ(sync_util::GetPasswordSyncState(sync_service()),
            sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_TRUE(
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service()));

  ExpectPasswords({}, /*store=*/account_store());
  ExpectPasswords({form}, /*store=*/profile_store());
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(true), IsReused(false),
                                           IsSyncing(false)),
          form.url, form.username_value, /* in_account_store = */ false));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  EXPECT_CALL(*profile_store(), UpdateLogin);
  WaitForPasswordStore();
}

TEST_F(LeakDetectionDelegateTest,
       LeakDetectionDoneForAccountStoreWithSplitStoresAndUPMForLocal) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  ON_CALL(client(), GetSyncService()).WillByDefault(Return(sync_service()));
  ON_CALL(client(), GetAccountPasswordStore())
      .WillByDefault(Return(account_store()));
  ON_CALL(client(), GetProfilePasswordStore())
      .WillByDefault(Return(profile_store()));

  // Mark that the local and account stores are split.
  pref_service()->SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));

  ASSERT_EQ(sync_util::GetPasswordSyncState(sync_service()),
            sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_TRUE(
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service()));

  ExpectPasswords({form}, /*store=*/account_store());
  ExpectPasswords({}, /*store=*/profile_store());
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(true), IsReused(false),
                                           IsSyncing(true)),
          form.url, form.username_value, /* in_account_store = */ true));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  EXPECT_CALL(*account_store(), UpdateLogin);
  WaitForPasswordStore();
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(LeakDetectionDelegateTest, LeakHistoryAddCredentials) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  PasswordForm form = CreateTestForm();

  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(Return(profile_store()));
  ExpectPasswords({form});
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_CALL(
      client(),
      NotifyUserCredentialsWereLeaked(
          password_manager::CreateLeakType(IsSaved(true), IsReused(false),
                                           IsSyncing(false)),
          form.url, form.username_value, /* in_account_store = */ false));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  // The expected form should have a leaked entry.
  form.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  form.in_store = PasswordForm::Store::kProfileStore;
  EXPECT_CALL(*profile_store(), UpdateLogin(form, _));
  WaitForPasswordStore();
}

// crbug.com/1083937 regression
TEST_F(LeakDetectionDelegateTest, CallStartTwice) {
  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(Return(profile_store()));
  ExpectPasswords({});
  auto check_instance = std::make_unique<NiceMock<MockLeakDetectionCheck>>();
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  PasswordForm form = CreateTestForm();
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());
  ASSERT_TRUE(delegate().leak_check());

  // The delegate analyses the password store after this call.
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  // Start the check again on another form in the mean time.
  check_instance = std::make_unique<NiceMock<MockLeakDetectionCheck>>();
  ExpectPasswords({});
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  form.username_value = u"username";
  form.password_value = u"password";
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());
  ASSERT_TRUE(delegate().leak_check());

  // Simulate the previous check is complete now.
  WaitForPasswordStore();

  // The second check is finishing and talking to the password store. It should
  // not crash.
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);
  WaitForPasswordStore();
}

TEST_F(LeakDetectionDelegateTest, PassesChromeChannel) {
  SetLeakDetectionEnabled(true);
  EXPECT_CALL(client(), IsOffTheRecord).WillOnce(Return(false));
  const PasswordForm form = CreateTestForm();
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form.url,
                    form.username_value, form.password_value));
  const version_info::Channel channel = version_info::Channel::STABLE;
  EXPECT_CALL(client(), GetChannel).WillOnce(Return(channel));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, channel))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(LeakDetectionInitiator::kSignInCheck, form,
                            GetTestUrl());

  EXPECT_TRUE(delegate().leak_check());
}

}  // namespace password_manager
