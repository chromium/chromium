// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/i18n/time_formatting.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_profile_validator.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/suggestion_selection.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

const char kPrimaryAccountEmail[] = "syncuser@example.com";
const char kSyncTransportAccountEmail[] = "transport@example.com";

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);
const base::Time kSomeLaterTime = base::Time::FromDoubleT(1000);
const base::Time kMuchLaterTime = base::Time::FromDoubleT(5000);

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

bool StructuredNames() {
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSupportForMoreStructureInNames);
}

bool StructuredAddress() {
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSupportForMoreStructureInAddresses);
}

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  ~PersonalDataLoadedObserverMock() override {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
  MOCK_METHOD0(OnPersonalDataFinishedProfileTasks, void());
};

class PersonalDataManagerMock : public PersonalDataManager {
 public:
  explicit PersonalDataManagerMock(const std::string& app_locale,
                                   const std::string& variations_country_code)
      : PersonalDataManager(app_locale, variations_country_code) {}
  ~PersonalDataManagerMock() override = default;

  MOCK_METHOD1(OnValidated, void(const AutofillProfile* profile));
  void OnValidatedPDM(const AutofillProfile* profile) {
    PersonalDataManager::OnValidated(profile);
  }
};

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(std::mismatch(results_copy.begin(), results_copy.end(),
                          expectations_copy.begin(), ElementsEqual<T>)
                .first,
            results_copy.end());
}

class ScopedFeatureListWrapper {
 public:
  explicit ScopedFeatureListWrapper(
      const std::vector<base::Feature>& default_enabled_features,
      const std::vector<base::Feature>& additional_enabled_features) {
    std::vector<base::Feature> all_enabled_features(default_enabled_features);
    std::copy(additional_enabled_features.begin(),
              additional_enabled_features.end(),
              std::back_inserter(all_enabled_features));
    scoped_features_.InitWithFeatures(all_enabled_features,
                                      /*disabled_features=*/{});
  }
  ~ScopedFeatureListWrapper() = default;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

}  // anonymous namespace

class PersonalDataManagerTestBase {
 protected:
  static std::vector<base::Feature> GetDefaultEnabledFeatures() {
    // Enable account storage by default, some tests will override this to be
    // false.
    return {features::kAutofillEnableAccountWalletStorage,
            features::kAutofillProfileClientValidation};
  }

  PersonalDataManagerTestBase()
      : scoped_features_(
            PersonalDataManagerTestBase::GetDefaultEnabledFeatures(),
            /*additioanal_enabled_features=*/{}),
        identity_test_env_(&test_url_loader_factory_) {}

  explicit PersonalDataManagerTestBase(
      const std::vector<base::Feature>& additioanal_enabled_features)
      : scoped_features_(
            PersonalDataManagerTestBase::GetDefaultEnabledFeatures(),
            additioanal_enabled_features),
        identity_test_env_(&test_url_loader_factory_) {}

  void SetUpTest() {
    OSCryptMocker::SetUp();
    prefs_ = test::PrefServiceForTesting();
    base::FilePath path(WebDatabase::kInMemoryPath);
    profile_web_database_ =
        new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get());

    // Hacky: hold onto a pointer but pass ownership.
    profile_autofill_table_ = new AutofillTable;
    profile_web_database_->AddTable(
        std::unique_ptr<WebDatabaseTable>(profile_autofill_table_));
    profile_web_database_->LoadDatabase();
    profile_database_service_ = new AutofillWebDataService(
        profile_web_database_, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    profile_database_service_->Init(base::NullCallback());

    account_web_database_ =
        new WebDatabaseService(base::FilePath(WebDatabase::kInMemoryPath),
                               base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get());
    account_autofill_table_ = new AutofillTable;
    account_web_database_->AddTable(
        std::unique_ptr<WebDatabaseTable>(account_autofill_table_));
    account_web_database_->LoadDatabase();
    account_database_service_ = new AutofillWebDataService(
        account_web_database_, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    account_database_service_->Init(base::NullCallback());

    test::DisableSystemServices(prefs_.get());
  }

  void TearDownTest() {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    test::ReenableSystemServices();
    OSCryptMocker::TearDown();
  }

  void ResetPersonalDataManager(UserMode user_mode,
                                bool use_sync_transport_mode,
                                PersonalDataManager* personal_data) {
    bool is_incognito = (user_mode == USER_MODE_INCOGNITO);

    personal_data->Init(
        scoped_refptr<AutofillWebDataService>(profile_database_service_),
        base::FeatureList::IsEnabled(
            features::kAutofillEnableAccountWalletStorage)
            ? scoped_refptr<AutofillWebDataService>(account_database_service_)
            : nullptr,
        prefs_.get(), identity_test_env_.identity_manager(),
        TestAutofillProfileValidator::GetInstance(),
        /*history_service=*/nullptr, is_incognito);

    personal_data->AddObserver(&personal_data_observer_);
    AccountInfo account_info;
    account_info.email = use_sync_transport_mode ? kSyncTransportAccountEmail
                                                 : kPrimaryAccountEmail;
    sync_service_.SetAuthenticatedAccountInfo(account_info);
    sync_service_.SetIsAuthenticatedAccountPrimary(!use_sync_transport_mode);
    personal_data->OnSyncServiceInitialized(&sync_service_);
    personal_data->OnStateChanged(&sync_service_);

    WaitForOnPersonalDataChangedRepeatedly();
  }

  bool TurnOnSyncFeature(PersonalDataManager* personal_data)
      WARN_UNUSED_RESULT {
    sync_service_.SetIsAuthenticatedAccountPrimary(true);
    if (!sync_service_.IsSyncFeatureEnabled())
      return false;
    personal_data->OnStateChanged(&sync_service_);

    return personal_data->IsSyncFeatureEnabled();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid,
                                           PersonalDataManager* personal_data) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data->RemoveByGUID(guid);
    run_loop.Run();
  }

  void SetServerCards(std::vector<CreditCard> server_cards) {
    test::SetServerCreditCards(account_autofill_table_, server_cards);
  }

  // Verify that the web database has been updated and the notification sent.
  void WaitOnceForOnPersonalDataChanged() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
    run_loop.Run();
  }

  // Verifies that the web database has been updated and the notification sent.
  void WaitForOnPersonalDataChanged() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    run_loop.Run();
  }

  // Verifies that the web database has been updated and the notification sent.
  void WaitForOnPersonalDataChangedRepeatedly() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    run_loop.Run();
  }

  AccountInfo SetActiveSecondaryAccount() {
    AccountInfo account_info;
    account_info.email = kSyncTransportAccountEmail;
    account_info.account_id = CoreAccountId("account_id");
    sync_service_.SetAuthenticatedAccountInfo(account_info);
    sync_service_.SetIsAuthenticatedAccountPrimary(false);
    return account_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<PrefService> prefs_;
  ScopedFeatureListWrapper scoped_features_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<AutofillWebDataService> profile_database_service_;
  scoped_refptr<AutofillWebDataService> account_database_service_;
  scoped_refptr<WebDatabaseService> profile_web_database_;
  scoped_refptr<WebDatabaseService> account_web_database_;
  AutofillTable* profile_autofill_table_;  // weak ref
  AutofillTable* account_autofill_table_;  // weak ref
  PersonalDataLoadedObserverMock personal_data_observer_;
};

class PersonalDataManagerHelper : public PersonalDataManagerTestBase {
 protected:
  PersonalDataManagerHelper() = default;

  explicit PersonalDataManagerHelper(
      const std::vector<base::Feature>& additional_enabled_features)
      : PersonalDataManagerTestBase(additional_enabled_features) {}

  virtual ~PersonalDataManagerHelper() {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
  }

  void ResetPersonalDataManager(UserMode user_mode,
                                bool use_account_server_storage = false) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset(new PersonalDataManager("EN", "US"));
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        user_mode, use_account_server_storage, personal_data_.get());
  }

  void ResetProfiles() {
    std::vector<AutofillProfile> empty_profiles;
    personal_data_->SetProfiles(&empty_profiles);
    WaitForOnPersonalDataChanged();
  }

  bool TurnOnSyncFeature() {
    return PersonalDataManagerTestBase::TurnOnSyncFeature(personal_data_.get());
  }

  void EnableAutofillProfileCleanup() {
    personal_data_->is_autofill_profile_cleanup_pending_ = true;
  }

  void SetUpReferenceProfile(const AutofillProfile& profile) {
    ASSERT_EQ(0U, personal_data_->GetProfiles().size());

    AddProfileToPersonalDataManager(profile);

    ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  }

  AutofillProfile GetDefaultProfile() {
    AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
    test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                         "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910",
                         false);

    return profile;
  }

  // Adds three local cards to the |personal_data_|. The three cards are
  // different: two are from different companies and the third doesn't have a
  // number. All three have different owners and credit card number. This allows
  // to test the suggestions based on name as well as on credit card number.
  void SetUpReferenceLocalCreditCards() {
    ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

    CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                            test::kEmptyOrigin);
    test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    credit_card0.set_use_count(3);
    credit_card0.set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(1));
    personal_data_->AddCreditCard(credit_card0);

    CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                            test::kEmptyOrigin);
    credit_card1.set_use_count(300);
    credit_card1.set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(10));
    test::SetCreditCardInfo(&credit_card1, "John Dillinger",
                            "4234567890123456" /* Visa */, "01", "2999", "1");
    personal_data_->AddCreditCard(credit_card1);

    CreditCard credit_card2("002149C1-EE28-4213-A3B9-DA243FFF021B",
                            test::kEmptyOrigin);
    credit_card2.set_use_count(1);
    credit_card2.set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(1));
    test::SetCreditCardInfo(&credit_card2, "Bonnie Parker",
                            "5105105105105100" /* Mastercard */, "12", "2999",
                            "1");
    personal_data_->AddCreditCard(credit_card2);

    WaitOnceForOnPersonalDataChanged();
    ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Add 3 credit cards. One local, one masked, one full. Creates two masked
  // cards on Linux, since full server cards are not supported.
  void SetUpThreeCardTypes() {
    EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    masked_server_card.set_server_id("masked_id");
    masked_server_card.set_use_count(15);
    personal_data_->AddFullServerCreditCard(masked_server_card);
    WaitOnceForOnPersonalDataChanged();
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

// Cards are automatically remasked on Linux since full server cards are not
// supported.
#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
    personal_data_->ResetFullServerCard(
        personal_data_->GetCreditCards()[0]->guid());
#endif

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    full_server_card.set_server_id("full_id");
    full_server_card.set_use_count(10);
    personal_data_->AddFullServerCreditCard(full_server_card);

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                            "4234567890123463",  // Visa
                            "08", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    local_card.set_use_count(5);
    personal_data_->AddCreditCard(local_card);

    WaitOnceForOnPersonalDataChanged();
    EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Helper method to create a local card that was expired 400 days ago,
  // and has not been used in last 400 days. This card is supposed to be
  // deleted during a major version upgrade.
  void CreateDeletableExpiredAndDisusedCreditCard() {
    CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
    test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "1999", "1");
    credit_card1.set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(400));

    personal_data_->AddCreditCard(credit_card1);

    WaitForOnPersonalDataChanged();
    EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  }

  // Helper method to create a profile that was last used 400 days ago.
  // This profile is supposed to be deleted during a major version upgrade.
  void CreateDeletableDisusedProfile() {
    AutofillProfile profile0(test::GetFullProfile());
    profile0.set_use_date(AutofillClock::Now() -
                          base::TimeDelta::FromDays(400));
    AddProfileToPersonalDataManager(profile0);

    EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  }

  AutofillTable* GetServerDataTable() {
    return personal_data_->IsSyncFeatureEnabled() ? profile_autofill_table_
                                                  : account_autofill_table_;
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    personal_data_->AddProfile(profile);
    run_loop.Run();
  }

  void UpdateProfileOnPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data_->UpdateProfile(profile);
    run_loop.Run();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataManagerTestBase::RemoveByGUIDFromPersonalDataManager(
        guid, personal_data_.get());
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(GetServerDataTable(), server_cards);
  }

  void SetServerProfiles(const std::vector<AutofillProfile>& server_profiles) {
    GetServerDataTable()->SetServerProfiles(server_profiles);
  }

  void SaveImportedProfileToPersonalDataManager(
      const AutofillProfile& profile) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    personal_data_->SaveImportedProfile(profile);
    run_loop.Run();
  }

  void ConvertWalletAddressesAndUpdateWalletCards() {
    // Simulate new data is coming from sync which triggers a conversion of
    // wallet addresses which in turn triggers a refresh.
    personal_data_->AutofillMultipleChangedBySync();
    WaitForOnPersonalDataChanged();
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

class PersonalDataManagerTest : public PersonalDataManagerHelper,
                                public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL);
  }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerMigrationTest : public PersonalDataManagerHelper,
                                         public testing::Test {
 public:
  PersonalDataManagerMigrationTest()
      : PersonalDataManagerHelper(
#if defined(OS_CHROMEOS)
            { ::switches::kAccountIdMigration }
#endif
        ) {
  }

 protected:
  void SetUp() override { SetUpTest(); }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerMockTest : public PersonalDataManagerTestBase,
                                    public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL);
    // Reset the deduping and profile validation prefs to their default value.
    personal_data_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped, 0);
    personal_data_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionValidated,
        atoi(version_info::GetVersionNumber().c_str()));
    personal_data_->is_autofill_profile_cleanup_pending_ = true;
  }

  void TearDown() override {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
    TearDownTest();
  }

  void ResetPersonalDataManager(UserMode user_mode) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset(new PersonalDataManagerMock("en", std::string()));
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        user_mode, /*use_account_server_storage=*/true, personal_data_.get());
  }

  bool TurnOnSyncFeature() {
    return PersonalDataManagerTestBase::TurnOnSyncFeature(personal_data_.get());
  }

  void StopTheDedupeProcess() {
    personal_data_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped,
        atoi(version_info::GetVersionNumber().c_str()));
  }

  void ResetAutofillLastVersionValidated() {
    ASSERT_TRUE(personal_data_);
    personal_data_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionValidated, 0);
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;

    EXPECT_CALL(*personal_data_, OnValidated(testing::_)).Times(1);
    ON_CALL(*personal_data_, OnValidated(testing::_))
        .WillByDefault(testing::Invoke(
            personal_data_.get(), &PersonalDataManagerMock::OnValidatedPDM));

    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data_->AddProfile(profile);

    run_loop.Run();
  }

  void UpdateProfileOnPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;

    EXPECT_CALL(*personal_data_, OnValidated(testing::_)).Times(1);
    ON_CALL(*personal_data_, OnValidated(testing::_))
        .WillByDefault(testing::Invoke(
            personal_data_.get(), &PersonalDataManagerMock::OnValidatedPDM));

    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data_->UpdateProfile(profile);

    run_loop.Run();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataManagerTestBase::RemoveByGUIDFromPersonalDataManager(
        guid, personal_data_.get());
  }

  void UpdateClientValidityStatesOnPersonalDataManager(
      const std::vector<AutofillProfile*>& profiles) {
    int num_updates = 0;
    if (GetLastVersionValidatedUpdate() < CHROME_VERSION_MAJOR) {
      num_updates = profiles.size();
    } else {
      for (auto* profile : profiles) {
        if (!profile->is_client_validity_states_updated())
          num_updates++;
      }
    }

    base::RunLoop run_loop;

    EXPECT_CALL(*personal_data_, OnValidated(testing::_)).Times(num_updates);
    ON_CALL(*personal_data_, OnValidated(testing::_))
        .WillByDefault(testing::Invoke(
            personal_data_.get(), &PersonalDataManagerMock::OnValidatedPDM));

    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    // Validate the profiles through the client validation API.
    personal_data_->UpdateClientValidityStates(profiles);
    run_loop.Run();
  }

  int GetLastVersionValidatedUpdate() {
    return personal_data_->pref_service_->GetInteger(
        prefs::kAutofillLastVersionValidated);
  }

  std::unique_ptr<PersonalDataManagerMock> personal_data_;
};

TEST_F(PersonalDataManagerTest, AddProfile) {
  // Add profile0 to the database.
  AutofillProfile profile0(test::GetFullProfile());
  profile0.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("j@s.com"));
  AddProfileToPersonalDataManager(profile0);
  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // Verify the addition.
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile profile0a = profile0;
  profile0a.set_guid(base::GenerateGUID());

  AddProfileToPersonalDataManager(profile0a);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the non-addition.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // New profile with different email.
  AutofillProfile profile1 = profile0;
  profile1.set_guid(base::GenerateGUID());
  profile1.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("john@smith.com"));

  // Add the different profile.  This should save as a separate profile.
  // Note that if this same profile was "merged" it would collapse to one
  // profile with a multi-valued entry for email.
  AddProfileToPersonalDataManager(profile1);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the addition.
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

// Adding, updating, removing operations without waiting in between.
TEST_F(PersonalDataManagerTest, AddRemoveUpdateProfileSequence) {
  AutofillProfile profile(test::GetFullProfile());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->RemoveByGUID(profile.guid());
  WaitForOnPersonalDataChanged();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("new@email.com"));
  personal_data_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS),
            base::ASCIIToUTF16("new@email.com"));

  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("newer@email.com"));
  personal_data_->UpdateProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("newest@email.com"));
  personal_data_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS),
            base::ASCIIToUTF16("newest@email.com"));
}

// The changes should happen in the same order as requested. If the later change
// is validated before an earlier one, still we should process the earlier one
// first.
TEST_F(PersonalDataManagerTest, InconsistentValidationSequence) {
  auto profile = test::GetFullProfile();
  // Slow validation.
  personal_data_->set_client_profile_validator_for_test(
      TestAutofillProfileValidator::GetDelayedInstance());
  personal_data_->AddProfile(profile);

  // No validator, zero delay for validation.
  personal_data_->set_client_profile_validator_for_test(nullptr);
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("new@email.com"));
  personal_data_->UpdateProfile(profile);

  WaitForOnPersonalDataChanged();

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS),
            base::ASCIIToUTF16("new@email.com"));
  EXPECT_FALSE(profiles[0]->is_client_validity_states_updated());
}

// Test that a new profile has its basic information set.
TEST_F(PersonalDataManagerTest, AddProfile_BasicInformation) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Add a profile to the database.
  AutofillProfile profile(test::GetFullProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("j@s.com"));
  AddProfileToPersonalDataManager(profile);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the addition.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));

  // Make sure the use count and use date were set.
  EXPECT_EQ(1U, results[0]->use_count());
  EXPECT_EQ(kArbitraryTime, results[0]->use_date());
  EXPECT_EQ(kArbitraryTime, results[0]->modification_date());
}

// Test filling profiles with unicode strings and crazy characters.
TEST_F(PersonalDataManagerTest, AddProfile_CrazyCharacters) {
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile1;
  profile1.SetRawInfo(
      NAME_FIRST,
      base::WideToUTF16(L"\u0623\u0648\u0628\u0627\u0645\u0627 "
                        L"\u064a\u0639\u062a\u0630\u0631 "
                        L"\u0647\u0627\u062a\u0641\u064a\u0627 "
                        L"\u0644\u0645\u0648\u0638\u0641\u0629 "
                        L"\u0633\u0648\u062f\u0627\u0621 "
                        L"\u0627\u0633\u062a\u0642\u0627\u0644\u062a "
                        L"\u0628\u0633\u0628\u0628 "
                        L"\u062a\u0635\u0631\u064a\u062d\u0627\u062a "
                        L"\u0645\u062c\u062a\u0632\u0623\u0629"));
  profile1.SetRawInfo(NAME_MIDDLE, base::WideToUTF16(L"BANK\xcBERF\xc4LLE"));
  profile1.SetRawInfo(EMAIL_ADDRESS,
                      base::WideToUTF16(L"\uacbd\uc81c \ub274\uc2a4 "
                                        L"\ub354\ubcf4\uae30@google.com"));
  profile1.SetRawInfo(
      ADDRESS_HOME_LINE1,
      base::WideToUTF16(L"\uad6d\uc815\uc6d0\xb7\uac80\ucc30, "
                        L"\ub178\ubb34\ud604\uc815\ubd80 "
                        L"\ub300\ubd81\uc811\ucd09 \ub2f4\ub2f9 "
                        L"\uc778\uc0ac\ub4e4 \uc870\uc0ac"));
  profile1.SetRawInfo(
      ADDRESS_HOME_CITY,
      base::WideToUTF16(L"\u653f\u5e9c\u4e0d\u6392\u9664\u7acb\u6cd5"
                        L"\u898f\u7ba1\u5c0e\u904a"));
  profile1.SetRawInfo(ADDRESS_HOME_ZIP, base::WideToUTF16(L"YOHO_54676"));
  profile1.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      base::WideToUTF16(L"861088828000"));
  profile1.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                   base::WideToUTF16(L"India"), "en-US");
  profiles.push_back(profile1);

  AutofillProfile profile2;
  profile2.SetRawInfo(NAME_FIRST,
                      base::WideToUTF16(L"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a "
                                        L"\u677e\u9690\u9547\u4ead\u67ab\u516c"
                                        L"\u8def1915\u53f7"));
  profile2.SetRawInfo(NAME_LAST, base::WideToUTF16(L"aguantó"));
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, base::WideToUTF16(L"HOME 94043"));
  profiles.push_back(profile2);

  AutofillProfile profile3;
  profile3.SetRawInfo(EMAIL_ADDRESS, base::WideToUTF16(L"sue@example.com"));
  profile3.SetRawInfo(COMPANY_NAME, base::WideToUTF16(L"Company X"));
  profiles.push_back(profile3);

  AutofillProfile profile4;
  profile4.SetRawInfo(NAME_FIRST, base::WideToUTF16(L"Joe 3254"));
  profile4.SetRawInfo(NAME_LAST,
                      base::WideToUTF16(L"\u8bb0\u8d262\u5e74\u591a"));
  profile4.SetRawInfo(
      ADDRESS_HOME_ZIP,
      base::WideToUTF16(L"\uff08\u90ae\u7f16\uff1a201504\uff09"));
  profile4.SetRawInfo(EMAIL_ADDRESS,
                      base::WideToUTF16(L"télévision@example.com"));
  profile4.SetRawInfo(
      COMPANY_NAME,
      base::WideToUTF16(L"\u0907\u0932\u0947\u0915\u093f\u091f\u094d"
                        L"\u0930\u0928\u093f\u0915\u094d\u0938, "
                        L"\u0905\u092a\u094b\u0932\u094b "
                        L"\u091f\u093e\u092f\u0930\u094d\u0938 "
                        L"\u0906\u0926\u093f"));
  profiles.push_back(profile4);

  AutofillProfile profile5;
  profile5.SetRawInfo(NAME_FIRST, base::WideToUTF16(L"Larry"));
  profile5.SetRawInfo(
      NAME_LAST, base::WideToUTF16(L"\u0938\u094d\u091f\u093e\u0902\u092a "
                                   L"\u0921\u094d\u092f\u0942\u091f\u0940"));
  profile5.SetRawInfo(ADDRESS_HOME_ZIP,
                      base::WideToUTF16(L"111111111111110000GOOGLE"));
  profile5.SetRawInfo(EMAIL_ADDRESS, base::WideToUTF16(L"page@000000.com"));
  profile5.SetRawInfo(COMPANY_NAME, base::WideToUTF16(L"Google"));
  profiles.push_back(profile5);

  AutofillProfile profile6;
  profile6.SetRawInfo(NAME_FIRST,
                      base::WideToUTF16(L"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a "
                                        L"\u677e\u9690\u9547\u4ead\u67ab\u516c"
                                        L"\u8def1915\u53f7"));
  profile6.SetRawInfo(
      NAME_LAST,
      base::WideToUTF16(L"\u0646\u062c\u0627\u0645\u064a\u0646\u0627 "
                        L"\u062f\u0639\u0645\u0647\u0627 "
                        L"\u0644\u0644\u0631\u0626\u064a\u0633 "
                        L"\u0627\u0644\u0633\u0648\u062f\u0627\u0646"
                        L"\u064a \u0639\u0645\u0631 "
                        L"\u0627\u0644\u0628\u0634\u064a\u0631"));
  profile6.SetRawInfo(ADDRESS_HOME_ZIP, base::WideToUTF16(L"HOME 94043"));
  profiles.push_back(profile6);

  AutofillProfile profile7;
  profile7.SetRawInfo(NAME_FIRST,
                      base::WideToUTF16(L"&$%$$$ TESTO *&*&^&^& MOKO"));
  profile7.SetRawInfo(NAME_MIDDLE, base::WideToUTF16(L"WOHOOOO$$$$$$$$****"));
  profile7.SetRawInfo(EMAIL_ADDRESS, base::WideToUTF16(L"yuvu@example.com"));
  profile7.SetRawInfo(ADDRESS_HOME_LINE1,
                      base::WideToUTF16(L"34544, anderson ST.(120230)"));
  profile7.SetRawInfo(ADDRESS_HOME_CITY, base::WideToUTF16(L"Sunnyvale"));
  profile7.SetRawInfo(ADDRESS_HOME_STATE, base::WideToUTF16(L"CA"));
  profile7.SetRawInfo(ADDRESS_HOME_ZIP, base::WideToUTF16(L"94086"));
  profile7.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      base::WideToUTF16(L"15466784565"));
  profile7.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                   base::WideToUTF16(L"United States"), "en-US");
  profiles.push_back(profile7);

  personal_data_->SetProfiles(&profiles);

  WaitForOnPersonalDataChanged();

  ASSERT_EQ(profiles.size(), personal_data_->GetProfiles().size());
  for (size_t i = 0; i < profiles.size(); ++i) {
    EXPECT_TRUE(base::Contains(profiles, *personal_data_->GetProfiles()[i]));
  }
}

// Test filling in invalid values for profiles are saved as-is. Phone
// information entered into the settings UI is not validated or rejected except
// for duplicates.
TEST_F(PersonalDataManagerTest, AddProfile_Invalid) {
  // First try profiles with invalid ZIP input.
  AutofillProfile without_invalid;
  without_invalid.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Will"));
  without_invalid.SetRawInfo(ADDRESS_HOME_CITY,
                             base::ASCIIToUTF16("Sunnyvale"));
  without_invalid.SetRawInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16("CA"));
  without_invalid.SetRawInfo(ADDRESS_HOME_ZIP, base::ASCIIToUTF16("my_zip"));
  without_invalid.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                          base::ASCIIToUTF16("United States"), "en-US");

  AutofillProfile with_invalid = without_invalid;
  with_invalid.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                          base::ASCIIToUTF16("Invalid_Phone_Number"));

  std::vector<AutofillProfile> profiles;
  profiles.push_back(with_invalid);
  personal_data_->SetProfiles(&profiles);
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1u, personal_data_->GetProfiles().size());
  AutofillProfile profile = *personal_data_->GetProfiles()[0];
  ASSERT_NE(without_invalid.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

// Tests that SaveImportedProfile sets the modification date on new profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileSetModificationDate) {
  AutofillProfile profile(test::GetFullProfile());
  EXPECT_NE(base::Time(), profile.modification_date());

  SaveImportedProfileToPersonalDataManager(profile);
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_GT(base::TimeDelta::FromMilliseconds(2000),
            AutofillClock::Now() - profiles[0]->modification_date());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveProfiles) {
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  // Add two test profiles to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Update, remove, and add.
  profile0.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("John"));
  UpdateProfileOnPersonalDataManager(profile0);
  RemoveByGUIDFromPersonalDataManager(profile1.guid());
  AddProfileToPersonalDataManager(profile2);

  profiles.clear();
  profiles.push_back(&profile0);
  profiles.push_back(&profile2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the profiles from the web database.
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveCreditCards) {
  CreditCard credit_card0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card0.SetNickname(base::ASCIIToUTF16("card zero"));

  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");

  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card2.SetNickname(base::ASCIIToUTF16("card two"));

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  WaitForOnPersonalDataChanged();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Update, remove, and add.
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Joe"));
  credit_card0.SetNickname(base::ASCIIToUTF16("new card zero"));
  personal_data_->UpdateCreditCard(credit_card0);
  RemoveByGUIDFromPersonalDataManager(credit_card1.guid());
  personal_data_->AddCreditCard(credit_card2);

  WaitForOnPersonalDataChanged();

  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the credit cards from the web database.
  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Add a full server card.
  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Jane Doe",
                          "4111111111111111" /* Visa */, "04", "2999", "1");
  credit_card3.set_record_type(CreditCard::FULL_SERVER_CARD);
  credit_card3.set_server_id("server_id");

  personal_data_->AddFullServerCreditCard(credit_card3);
  WaitForOnPersonalDataChanged();

  cards.push_back(&credit_card3);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Must not add a duplicate server card with same GUID.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  personal_data_->AddFullServerCreditCard(credit_card3);

  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Must not add a duplicate card with same contents as another server card.
  CreditCard duplicate_server_card(credit_card3);
  duplicate_server_card.set_guid(base::GenerateGUID());

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  personal_data_->AddFullServerCreditCard(duplicate_server_card);

  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, DoNotAddGoogleIssuedCreditCardExpOff) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kAutofillEnableGoogleIssuedCard);
  // Set up the credit cards.
  CreditCard credit_card0 = test::GetMaskedServerCard();
  credit_card0.set_card_issuer(CreditCard::Issuer::ISSUER_UNKNOWN);
  CreditCard credit_card1 = test::GetMaskedServerCardAmex();
  credit_card1.set_card_issuer(CreditCard::Issuer::GOOGLE);
  // Add the above cards to server_cards.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(credit_card0);
  server_cards.push_back(credit_card1);
  SetServerCards(server_cards);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  std::vector<CreditCard*> cards;
  // Since the flag is off, only the card with ISSUER_UNKNOWN should be
  // returned.
  cards.push_back(&credit_card0);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, AddGoogleIssuedCreditCard) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillEnableGoogleIssuedCard);
  // Set up the credit cards.
  CreditCard credit_card0 = test::GetMaskedServerCard();
  credit_card0.set_card_issuer(CreditCard::Issuer::ISSUER_UNKNOWN);
  CreditCard credit_card1 = test::GetMaskedServerCardAmex();
  credit_card1.set_card_issuer(CreditCard::Issuer::GOOGLE);
  // Add the above cards to server_cards.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(credit_card0);
  server_cards.push_back(credit_card1);
  SetServerCards(server_cards);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

// Test that a new credit card has its basic information set.
TEST_F(PersonalDataManagerTest, AddCreditCard_BasicInformation) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Add a credit card to the database.
  CreditCard credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  personal_data_->AddCreditCard(credit_card);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the addition.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));

  // Make sure the use count and use date were set.
  EXPECT_EQ(1U, results[0]->use_count());
  EXPECT_EQ(kArbitraryTime, results[0]->use_date());
  EXPECT_EQ(kArbitraryTime, results[0]->modification_date());
}

// Test filling credit cards with unicode strings and crazy characters.
TEST_F(PersonalDataManagerTest, AddCreditCard_CrazyCharacters) {
  std::vector<CreditCard> cards;
  CreditCard card1;
  card1.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   base::WideToUTF16(L"\u751f\u6d3b\u5f88\u6709\u89c4\u5f8b "
                                     L"\u4ee5\u73a9\u4e3a\u4e3b"));
  card1.SetRawInfo(CREDIT_CARD_NUMBER, base::WideToUTF16(L"6011111111111117"));
  card1.SetRawInfo(CREDIT_CARD_EXP_MONTH, base::WideToUTF16(L"12"));
  card1.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, base::WideToUTF16(L"2011"));
  cards.push_back(card1);

  CreditCard card2;
  card2.SetRawInfo(CREDIT_CARD_NAME_FULL, base::WideToUTF16(L"John Williams"));
  card2.SetRawInfo(CREDIT_CARD_NUMBER, base::WideToUTF16(L"WokoAwesome12345"));
  card2.SetRawInfo(CREDIT_CARD_EXP_MONTH, base::WideToUTF16(L"10"));
  card2.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, base::WideToUTF16(L"2015"));
  cards.push_back(card2);

  CreditCard card3;
  card3.SetRawInfo(
      CREDIT_CARD_NAME_FULL,
      base::WideToUTF16(L"\u0623\u062d\u0645\u062f\u064a "
                        L"\u0646\u062c\u0627\u062f "
                        L"\u0644\u0645\u062d\u0627\u0648\u0644\u0647 "
                        L"\u0627\u063a\u062a\u064a\u0627\u0644 "
                        L"\u0641\u064a \u0645\u062f\u064a\u0646\u0629 "
                        L"\u0647\u0645\u062f\u0627\u0646 "));
  card3.SetRawInfo(
      CREDIT_CARD_NUMBER,
      base::WideToUTF16(L"\u092a\u0941\u0928\u0930\u094d\u091c\u0940"
                        L"\u0935\u093f\u0924 \u0939\u094b\u0917\u093e "
                        L"\u0928\u093e\u0932\u0902\u0926\u093e"));
  card3.SetRawInfo(CREDIT_CARD_EXP_MONTH, base::WideToUTF16(L"10"));
  card3.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, base::WideToUTF16(L"2015"));
  cards.push_back(card3);

  CreditCard card4;
  card4.SetRawInfo(
      CREDIT_CARD_NAME_FULL,
      base::WideToUTF16(L"\u039d\u03ad\u03b5\u03c2 "
                        L"\u03c3\u03c5\u03b3\u03c7\u03c9\u03bd\u03b5"
                        L"\u03cd\u03c3\u03b5\u03b9\u03c2 "
                        L"\u03ba\u03b1\u03b9 "
                        L"\u03ba\u03b1\u03c4\u03b1\u03c1\u03b3\u03ae"
                        L"\u03c3\u03b5\u03b9\u03c2"));
  card4.SetRawInfo(CREDIT_CARD_NUMBER,
                   base::WideToUTF16(L"00000000000000000000000"));
  card4.SetRawInfo(CREDIT_CARD_EXP_MONTH, base::WideToUTF16(L"01"));
  card4.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, base::WideToUTF16(L"2016"));
  cards.push_back(card4);

  personal_data_->SetCreditCards(&cards);

  WaitForOnPersonalDataChanged();

  ASSERT_EQ(cards.size(), personal_data_->GetCreditCards().size());
  for (size_t i = 0; i < cards.size(); ++i) {
    EXPECT_TRUE(base::Contains(cards, *personal_data_->GetCreditCards()[i]));
  }
}

// Test invalid credit card numbers typed in settings UI should be saved as-is.
TEST_F(PersonalDataManagerTest, AddCreditCard_Invalid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, base::ASCIIToUTF16("Not_0123-5Checked"));

  std::vector<CreditCard> cards;
  cards.push_back(card);
  personal_data_->SetCreditCards(&cards);

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  ASSERT_EQ(card, *personal_data_->GetCreditCards()[0]);
}

TEST_F(PersonalDataManagerTest, UpdateUnverifiedProfilesAndCreditCards) {
  // Start with unverified data.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  AddProfileToPersonalDataManager(profile);
  personal_data_->AddCreditCard(credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<AutofillProfile*>& profiles1 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles1.size());
  ASSERT_EQ(1U, cards1.size());
  EXPECT_EQ(0, profile.Compare(*profiles1[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards1[0]));

  // Try to update with just the origin changed.
  AutofillProfile original_profile(profile);
  ASSERT_FALSE(original_profile.IsVerified());
  CreditCard original_credit_card(credit_card);
  profile.set_origin(kSettingsOrigin);
  credit_card.set_origin(kSettingsOrigin);

  EXPECT_TRUE(profile.IsVerified());
  EXPECT_TRUE(credit_card.IsVerified());
  UpdateProfileOnPersonalDataManager(profile);
  personal_data_->UpdateCreditCard(credit_card);

  // Credit Card origin should not be overwritten.
  const std::vector<AutofillProfile*>& profiles2 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles2.size());
  ASSERT_EQ(1U, cards2.size());
  EXPECT_EQ(profile.origin(), profiles2[0]->origin());
  EXPECT_NE(credit_card.origin(), cards2[0]->origin());
  EXPECT_NE(original_profile.origin(), profiles2[0]->origin());
  EXPECT_EQ(original_credit_card.origin(), cards2[0]->origin());

  // Try to update with data changed as well.
  profile.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("John"));
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Joe"));

  UpdateProfileOnPersonalDataManager(profile);
  personal_data_->UpdateCreditCard(credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<AutofillProfile*>& profiles3 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards3 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles3.size());
  ASSERT_EQ(1U, cards3.size());
  EXPECT_EQ(0, profile.Compare(*profiles3[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards3[0]));
  EXPECT_EQ(profile.origin(), profiles3[0]->origin());
  EXPECT_EQ(credit_card.origin(), cards3[0]->origin());
}

// Test that updating a verified profile with another profile whose only
// difference is the origin, would not change the old profile, and thus it would
// remain verified.
TEST_F(PersonalDataManagerTest, UpdateVerifiedProfilesOrigin) {
  // Start with verified data.
  AutofillProfile profile(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  ASSERT_TRUE(profile.IsVerified());
  AddProfileToPersonalDataManager(profile);

  const std::vector<AutofillProfile*>& profiles1 =
      personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles1.size());
  EXPECT_EQ(0, profile.Compare(*profiles1[0]));

  // Try to update with just the origin changed to a non-setting origin.
  AutofillProfile new_profile(profile);
  new_profile.set_origin("");
  ASSERT_FALSE(new_profile.IsVerified());

  UpdateProfileOnPersonalDataManager(profile);

  // Verified profile origin should not be overwritten.
  const std::vector<AutofillProfile*>& profiles2 =
      personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles2.size());
  EXPECT_EQ(profile.origin(), profiles2[0]->origin());
  EXPECT_NE(new_profile.origin(), profiles2[0]->origin());
  EXPECT_TRUE(profiles2[0]->IsVerified());
}

// Test that ensure local data is not lost on sign-in.
TEST_F(PersonalDataManagerTest, KeepExistingLocalDataOnSignIn) {
  // Set up the experiment flags.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAccountWalletStorage},
      /*disabled_features=*/{});

// ClearPrimaryAccount is not supported on CrOS.
#if !defined(OS_CHROMEOS)
  // Sign out.
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(AutofillSyncSigninState::kSignedOut,
            personal_data_->GetSyncSigninState());
#endif
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  local_card.set_use_count(5);
  personal_data_->AddCreditCard(local_card);
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Sign in.
  identity_test_env_.SetPrimaryAccount("test@gmail.com");
  sync_service_.SetIsAuthenticatedAccountPrimary(true);
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
            personal_data_->GetSyncSigninState());
  ASSERT_TRUE(TurnOnSyncFeature());

  // Check saved local card should be not lost.
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(0, local_card.Compare(*personal_data_->GetCreditCards()[0]));
}

TEST_F(PersonalDataManagerTest, AddProfilesAndCreditCards) {
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  CreditCard credit_card0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");

  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");

  // Add two test profiles to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  WaitForOnPersonalDataChanged();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Determine uniqueness by inserting all of the GUIDs into a set and verifying
  // the size of the set matches the number of GUIDs.
  std::set<std::string> guids;
  guids.insert(profile0.guid());
  guids.insert(profile1.guid());
  guids.insert(credit_card0.guid());
  guids.insert(credit_card1.guid());
  EXPECT_EQ(4U, guids.size());
}

// Test for http://crbug.com/50047. Makes sure that guids are populated
// correctly on load.
TEST_F(PersonalDataManagerTest, PopulateUniqueIDsOnLoad) {
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "y", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the profile0 to the db.
  AddProfileToPersonalDataManager(profile0);

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "z", "", "", "", "", "", "", "", "", "", "",
                       "");
  AddProfileToPersonalDataManager(profile1);

  // Make sure the two profiles have different GUIDs, both valid.
  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_NE(results3[0]->guid(), results3[1]->guid());
  EXPECT_TRUE(base::IsValidGUID(results3[0]->guid()));
  EXPECT_TRUE(base::IsValidGUID(results3[1]->guid()));
}

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("John"));
  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card1.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Paul"));
  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card2.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Ringo"));
  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card3.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Other"));
  CreditCard credit_card4(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card4.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Ozzy"));
  CreditCard credit_card5(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card5.SetRawInfo(CREDIT_CARD_NAME_FULL, base::ASCIIToUTF16("Dio"));

  // Add the test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);
  personal_data_->AddCreditCard(credit_card5);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  cards.push_back(&credit_card2);
  cards.push_back(&credit_card3);
  cards.push_back(&credit_card4);
  cards.push_back(&credit_card5);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, SetEmptyProfile) {
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the empty profile to the database.
  AddProfileToPersonalDataManager(profile0);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the profiles from the web database.
  ASSERT_EQ(0U, personal_data_->GetProfiles().size());
}

TEST_F(PersonalDataManagerTest, SetEmptyCreditCard) {
  CreditCard credit_card0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "", "", "", "", "");

  // Add the empty credit card to the database.
  personal_data_->AddCreditCard(credit_card0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the credit cards from the web database.
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  // Add the test profiles to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  profile_database_service_->AddAutofillProfile(profile2);

  personal_data_->Refresh();

  WaitForOnPersonalDataChanged();

  profiles.clear();
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  profiles.push_back(&profile2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  profile_database_service_->RemoveAutofillProfile(profile1.guid());
  profile_database_service_->RemoveAutofillProfile(profile2.guid());

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  auto results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);

  profile0.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Mar"));
  profile_database_service_->UpdateAutofillProfile(profile0);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);
}

// Ensure that verified profiles can be saved via SaveImportedProfile,
// overwriting existing unverified profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileWithVerifiedData) {
  // Start with an unverified profile.
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  AddProfileToPersonalDataManager(profile);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  AutofillProfile new_verified_profile = profile;
  new_verified_profile.set_guid(base::GenerateGUID());
  new_verified_profile.set_origin(kSettingsOrigin);
  new_verified_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                                  base::ASCIIToUTF16("1 234 567-8910"));
  EXPECT_TRUE(new_verified_profile.IsVerified());

  SaveImportedProfileToPersonalDataManager(new_verified_profile);

  // The new profile should be merged into the existing one.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  AutofillProfile expected(new_verified_profile);
  // The full name was missing in |profile| and was formatted from its
  // components.
  expected.SetRawInfoWithVerificationStatus(
      NAME_FULL, base::ASCIIToUTF16("Marion Mitchell Morrison"),
      structured_address::VerificationStatus::kFormatted);
  expected.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      base::ASCIIToUTF16("+1 234-567-8910"));
  EXPECT_EQ(0, expected.Compare(*results[0]))
      << "result = {" << *results[0] << "} | expected = {" << expected << "}";
}

// Ensure that verified credit cards can be saved via
// OnAcceptedLocalCreditCardSave.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalCreditCardSaveWithVerifiedData) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(base::GenerateGUID());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME_FULL,
                               base::ASCIIToUTF16("B. Small"));
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->OnAcceptedLocalCreditCardSave(new_verified_card);

  WaitForOnPersonalDataChanged();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(base::ASCIIToUTF16("B. Small"),
            results[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

TEST_F(PersonalDataManagerTest, GetNonEmptyTypes) {
  // Check that there are no available types with no profiles stored.
  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(0U, non_empty_types.size());

  // Test with one profile stored.
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", nullptr, "Morrison",
                       "johnwayne@me.xyz", nullptr, "123 Zoo St.", nullptr,
                       "Hollywood", "CA", "91601", "US", "14155678910");

  AddProfileToPersonalDataManager(profile0);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  // For structured names and addresses, there are more non-empty types.
  // TODO(crbug.com/1103421): Clean once launched.
  unsigned int non_empty_types_expectation = 15;
  if (StructuredNames())
    non_empty_types_expectation += 1;
  // TODO(crbug.com/1130194): Clean once launched.
  if (StructuredAddress())
    non_empty_types_expectation += 2;

  EXPECT_EQ(non_empty_types_expectation, non_empty_types.size());

  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  // TODO(crbug.com/1103421): Clean once launched.
  if (StructuredNames())
    EXPECT_TRUE(non_empty_types.count(NAME_LAST_SECOND));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_ADDRESS));
  // TODO(crbug.com/1130194): Clean once launched.
  if (StructuredAddress()) {
    EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_NAME));
    EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_HOUSE_NUMBER));
  }
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));

  // Test with multiple profiles stored.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "16502937549");

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "16502937549");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  non_empty_types_expectation = 19;
  // For structured names, there is one more non-empty type.
  // TODO(crbug.com/1103421): Clean once launched.
  if (StructuredNames())
    non_empty_types_expectation += 1;
  // TODO(crbug.com/1130194): Clean once launched.
  if (StructuredAddress())
    non_empty_types_expectation += 2;
  EXPECT_EQ(non_empty_types_expectation, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE_INITIAL));
  // TODO(crbug.com/1103421): Clean once launched.
  if (StructuredNames())
    EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(COMPANY_NAME));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE2));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_ADDRESS));
  // TODO(crbug.com/1130194): Clean once launched.
  if (StructuredAddress()) {
    EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_NAME));
    EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_HOUSE_NUMBER));
  }
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));

  // Test with credit card information also stored.
  CreditCard credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "");
  personal_data_->AddCreditCard(credit_card);

  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  // For structured names, there is one more non-empty type.
  // TODO(crbug.com/1103421): Clean once launched.
  non_empty_types_expectation = 29;
  if (StructuredNames())
    non_empty_types_expectation += 1;
  // TODO(crbug.com/1130194): Clean once launched.
  if (StructuredAddress())
    non_empty_types_expectation += 2;
  EXPECT_EQ(non_empty_types_expectation, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE_INITIAL));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  if (StructuredNames())
    EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(COMPANY_NAME));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE2));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_ADDRESS));
  // TODO(crbug.com/1130194): Clean once launched.
  if (StructuredAddress()) {
    EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_NAME));
    EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_HOUSE_NUMBER));
  }
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NUMBER));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_TYPE));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_2_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));
}

TEST_F(PersonalDataManagerTest, IncognitoReadOnly) {
  ASSERT_TRUE(personal_data_->GetProfiles().empty());
  ASSERT_TRUE(personal_data_->GetCreditCards().empty());

  AutofillProfile steve_jobs(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&steve_jobs, "Steven", "Paul", "Jobs", "sjobs@apple.com",
                       "Apple Computer, Inc.", "1 Infinite Loop", "",
                       "Cupertino", "CA", "95014", "US", "(800) 275-2273");
  AddProfileToPersonalDataManager(steve_jobs);

  CreditCard bill_gates(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&bill_gates, "William H. Gates", "5555555555554444",
                          "1", "2020", "1");
  personal_data_->AddCreditCard(bill_gates);

  // The personal data manager should be able to read existing profiles in an
  // off-the-record context.
  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // No adds, saves, or updates should take effect.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  // Add profiles or credit card shouldn't work.
  personal_data_->AddProfile(test::GetFullProfile());

  CreditCard larry_page(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&larry_page, "Lawrence Page", "4111111111111111",
                          "10", "2025", "1");
  personal_data_->AddCreditCard(larry_page);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Saving or creating profiles from imported profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Steve"));
  personal_data_->SaveImportedProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME_FULL,
                        base::ASCIIToUTF16("Bill Gates"));
  personal_data_->OnAcceptedLocalCreditCardSave(bill_gates);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(base::ASCIIToUTF16("Steven"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(
      base::ASCIIToUTF16("William H. Gates"),
      personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));

  // Updating existing profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Steve"));
  personal_data_->UpdateProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME_FULL,
                        base::ASCIIToUTF16("Bill Gates"));
  personal_data_->UpdateCreditCard(bill_gates);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(base::ASCIIToUTF16("Steven"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(
      base::ASCIIToUTF16("William H. Gates"),
      personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));

  // Removing shouldn't work.
  personal_data_->RemoveByGUID(steve_jobs.guid());
  personal_data_->RemoveByGUID(bill_gates.guid());

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeIsCached) {
  // The return value should always be some country code, no matter what.
  std::string default_country =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(2U, default_country.size());

  AutofillProfile moose(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&moose, "Moose", "P", "McMahon", "mpm@example.com", "",
                       "1 Taiga TKTR", "", "Calgary", "AB", "T2B 2K2", "CA",
                       "(800) 555-9000");
  AddProfileToPersonalDataManager(moose);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // The value is cached and doesn't change even after adding an address.
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Disabling Autofill blows away this cache and shouldn't account for Autofill
  // profiles.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  prefs::SetAutofillCreditCardEnabled(prefs_.get(), false);
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Enabling Autofill blows away the cached value and should reflect the new
  // value (accounting for profiles).
  prefs::SetAutofillProfileEnabled(prefs_.get(), true);
  EXPECT_EQ(base::UTF16ToUTF8(moose.GetRawInfo(ADDRESS_HOME_COUNTRY)),
            personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromProfiles) {
  AutofillProfile moose(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&moose, "Moose", "P", "McMahon", "mpm@example.com", "",
                       "1 Taiga TKTR", "", "Calgary", "AB", "T2B 2K2", "CA",
                       "(800) 555-9000");
  AddProfileToPersonalDataManager(moose);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Multiple profiles cast votes.
  AutofillProfile armadillo(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&armadillo, "Armin", "Dill", "Oh", "ado@example.com", "",
                       "1 Speed Bump", "", "Lubbock", "TX", "77500", "MX",
                       "(800) 555-9000");
  AutofillProfile armadillo2(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&armadillo2, "Armin", "Dill", "Oh", "ado@example.com",
                       "", "2 Speed Bump", "", "Lubbock", "TX", "77500", "MX",
                       "(800) 555-9000");
  AddProfileToPersonalDataManager(armadillo);
  AddProfileToPersonalDataManager(armadillo2);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());

  RemoveByGUIDFromPersonalDataManager(armadillo.guid());
  RemoveByGUIDFromPersonalDataManager(armadillo2.guid());
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // Verified profiles count more.
  armadillo.set_origin("http://randomwebsite.com");
  armadillo2.set_origin("http://randomwebsite.com");
  AddProfileToPersonalDataManager(armadillo);
  AddProfileToPersonalDataManager(armadillo2);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  RemoveByGUIDFromPersonalDataManager(armadillo.guid());
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // But unverified profiles can be a tie breaker.
  armadillo.set_origin(kSettingsOrigin);
  AddProfileToPersonalDataManager(armadillo);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromVariations) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseVariationCountryCode);

  const std::string expected_country_code = "DE";
  const std::string unepected_country_code = "FR";

  // Normally, the variation country code is passed to the constructor.
  personal_data_->set_variations_country_code_for_testing(
      expected_country_code);

  // Verify that there are no profiles set.
  EXPECT_EQ(0u, personal_data_->GetProfiles().size());

  // Since there are no profiles set, the country code supplied buy variations
  // should be used get get a default country code.
  std::string actual_country_code =
      personal_data_->GetDefaultCountryCodeForNewAddress();

  // Verify the the correct country code was retrieved.
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Set a new country code.
  personal_data_->set_variations_country_code_for_testing(
      unepected_country_code);

  // The default country code retrieved before should have been cached.
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();

  // Verify the expectations newly set country code is actually different.
  EXPECT_NE(expected_country_code, unepected_country_code);

  // Verify that it was actually set.
  EXPECT_EQ(unepected_country_code,
            personal_data_->variations_country_code_for_testing());

  // Verify that the retrieved country code is the initial one.
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Now a profile is set and the correctcaching of the country code is verified
  // once more.
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  // Once more, retrieve the defaultcountry code.
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();

  // Verify that the profile was actually set.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // Verify that the country code is still the initial one.
  EXPECT_EQ(actual_country_code, expected_country_code);
}

TEST_F(PersonalDataManagerTest, UpdateLanguageCodeInProfile) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  profile.set_language_code("en");
  UpdateProfileOnPersonalDataManager(profile);

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
  EXPECT_EQ("en", results[0]->language_code());
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
      false, std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(base::ASCIIToUTF16("123 Zoo St., Second Line, Third line, unit 5"),
            suggestions[0].value);
}

TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_PhoneSubstring_NoImprovedDisambiguation) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(PHONE_HOME_WHOLE_NUMBER), base::ASCIIToUTF16("234"), false,
      std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(base::ASCIIToUTF16("12345678910"), suggestions[0].value);
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_PhoneSubstring_ImprovedDisambiguation) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(PHONE_HOME_WHOLE_NUMBER), base::ASCIIToUTF16("234"), false,
      std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(base::ASCIIToUTF16("(234) 567-8910"), suggestions[0].value);
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_HideSubsets) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Dupe profile, except different in email address (irrelevant for this form).
  AutofillProfile profile1 = profile;
  profile1.set_guid(base::GenerateGUID());
  profile1.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("spam_me@example.com"));

  // Dupe profile, except different in address state.
  AutofillProfile profile2 = profile;
  profile2.set_guid(base::GenerateGUID());
  profile2.SetRawInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16("TX"));

  // Subset profile.
  AutofillProfile profile3 = profile;
  profile3.set_guid(base::GenerateGUID());
  profile3.SetRawInfo(ADDRESS_HOME_STATE, base::string16());

  // For easier results verification, make sure |profile| is suggested first.
  profile.set_use_count(5);
  AddProfileToPersonalDataManager(profile);
  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Simulate a form with street address, city and state.
  std::vector<ServerFieldType> types;
  types.push_back(ADDRESS_HOME_CITY);
  types.push_back(ADDRESS_HOME_STATE);
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
      false, types);
  ASSERT_EQ(2U, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16("Hollywood, CA"), suggestions[0].label);
  EXPECT_EQ(base::ASCIIToUTF16("Hollywood, TX"), suggestions[1].label);
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_SuggestionsLimit) {
  // Drawing takes noticeable time when there are more than 10 profiles.
  // Therefore, we keep only the 10 first suggested profiles.
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < 2 * suggestion_selection::kMaxUniqueSuggestionsCount;
       i++) {
    AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
    test::SetProfileInfo(&profile, base::StringPrintf("Marion%zu", i).c_str(),
                         "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    AddProfileToPersonalDataManager(profile);
    profiles.push_back(profile);
  }
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::ASCIIToUTF16("Ma"), false,
      std::vector<ServerFieldType>());

  ASSERT_EQ(2 * suggestion_selection::kMaxUniqueSuggestionsCount,
            personal_data_->GetProfiles().size());
  ASSERT_EQ(suggestion_selection::kMaxUniqueSuggestionsCount,
            suggestions.size());
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_ProfilesLimit) {
  // Deduping takes noticeable time when there are more than 50 profiles.
  // Therefore, keep only the 50 first pre-dedupe matching profiles.
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < suggestion_selection::kMaxSuggestedProfilesCount;
       i++) {
    AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

    test::SetProfileInfo(
        &profile, "Marion", "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
        base::StringPrintf("%zu123 Zoo St.\nSecond Line\nThird line", i)
            .c_str(),
        "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");

    // Set frecency such that they appear before the "last" profile (added
    // next).
    profile.set_use_count(12);
    profile.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));

    AddProfileToPersonalDataManager(profile);
    profiles.push_back(profile);
  }

  // Add another profile that matches, but that will get stripped out.
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marie", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "000 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile.set_use_count(1);
  profile.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(7));
  AddProfileToPersonalDataManager(profile);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::ASCIIToUTF16("Ma"), false,
      std::vector<ServerFieldType>());

  ASSERT_EQ(suggestion_selection::kMaxSuggestedProfilesCount + 1,
            personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16("Marion"), suggestions[0].value);
}

// Tests that GetProfileSuggestions orders its suggestions based on the frecency
// formula.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_Ranking) {
  // Set up the profiles. They are named with number suffixes X so the X is the
  // order in which they should be ordered by frecency.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));
  profile3.set_use_count(5);
  AddProfileToPersonalDataManager(profile3);

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));
  profile1.set_use_count(10);
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(15));
  profile2.set_use_count(300);
  AddProfileToPersonalDataManager(profile2);

  ResetPersonalDataManager(USER_MODE_NORMAL);
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::ASCIIToUTF16("Ma"), false,
      std::vector<ServerFieldType>());
  ASSERT_EQ(3U, suggestions.size());
  EXPECT_EQ(suggestions[0].value, base::ASCIIToUTF16("Marion1"));
  EXPECT_EQ(suggestions[1].value, base::ASCIIToUTF16("Marion2"));
  EXPECT_EQ(suggestions[2].value, base::ASCIIToUTF16("Marion3"));
}

// Tests that GetProfileSuggestions returns all profiles suggestions.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_NumberOfSuggestions) {
  // Set up 3 different profiles.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile2);

  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile3);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that all the profiles are suggested.
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::string16(), false,
      std::vector<ServerFieldType>());
  EXPECT_EQ(3U, suggestions.size());
}

// Tests that disused profiles are suppressed when supression is enabled and
// the input field is empty.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_SuppressDisusedProfilesOnEmptyField) {
  // Set up 2 different profiles.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(200));
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "456 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(20));
  AddProfileToPersonalDataManager(profile2);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Query with empty string only returns profile2.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::string16(), false,
        std::vector<ServerFieldType>());
    EXPECT_EQ(1U, suggestions.size());
  }

  // Query with non-alpha-numeric string only returns profile2.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("--"),
        false, std::vector<ServerFieldType>());
    EXPECT_EQ(1U, suggestions.size());
  }

  // Query with prefix for profile1 returns profile1.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
        false, std::vector<ServerFieldType>());
    ASSERT_EQ(1U, suggestions.size());
    EXPECT_EQ(
        base::ASCIIToUTF16("123 Zoo St., Second Line, Third line, unit 5"),
        suggestions[0].value);
  }

  // Query with prefix for profile2 returns profile2.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("456"),
        false, std::vector<ServerFieldType>());
    EXPECT_EQ(1U, suggestions.size());
    EXPECT_EQ(
        base::ASCIIToUTF16("456 Zoo St., Second Line, Third line, unit 5"),
        suggestions[0].value);
  }
}

// Tests that suggestions based on invalid data are handled correctly.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_Validity) {
  // Set up 2 different profiles: one valid and one invalid.
  AutofillProfile valid_profile(test::GetFullValidProfileForCanada());
  valid_profile.set_use_date(AutofillClock::Now() -
                             base::TimeDelta::FromDays(1));
  valid_profile.set_use_count(1);
  AddProfileToPersonalDataManager(valid_profile);

  AutofillProfile invalid_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&invalid_profile, "Marion1", "Mitchell", "Morrison",
                       "invalid email", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "Invalid Phone");
  invalid_profile.set_use_date(AutofillClock::Now() -
                               base::TimeDelta::FromDays(1));
  invalid_profile.set_use_count(1);
  AddProfileToPersonalDataManager(invalid_profile);

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(2U, profiles.size());

  // Invalid based on client, and not invalid by server. Relying on both
  // validity sources.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillProfileServerValidation,
                              features::kAutofillProfileClientValidation},
        /*disabled_features=*/{});
    std::vector<Suggestion> email_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(EMAIL_ADDRESS),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());

    for (auto* profile : profiles) {
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByClient());
      ASSERT_TRUE(profile->IsValidByServer());
    }
    ASSERT_EQ(1U, email_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("alice@wonderland.ca"),
              email_suggestions[0].value);

    std::vector<Suggestion> name_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(NAME_FIRST),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());
    ASSERT_EQ(2U, name_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Alice"), name_suggestions[0].value);
    EXPECT_EQ(base::ASCIIToUTF16("Marion1"), name_suggestions[1].value);
  }

  // Set the validity state of ADDRESS_HOME_STATE to INVALID on the prefs.
  {
    ProfileValidityMap profile_validity_map;
    UserProfileValidityMap user_profile_validity_map;
    std::string autofill_profile_validity;
    personal_data_->pref_service_->SetString(prefs::kAutofillProfileValidity,
                                             autofill_profile_validity);
    (*profile_validity_map
          .mutable_field_validity_states())[static_cast<int>(EMAIL_ADDRESS)] =
        static_cast<int>(AutofillProfile::INVALID);
    (*user_profile_validity_map
          .mutable_profile_validity())[invalid_profile.guid()] =
        profile_validity_map;
    ASSERT_TRUE(user_profile_validity_map.SerializeToString(
        &autofill_profile_validity));
    base::Base64Encode(autofill_profile_validity, &autofill_profile_validity);
    personal_data_->pref_service_->SetString(prefs::kAutofillProfileValidity,
                                             autofill_profile_validity);
  }
  // Invalid based on client, and server. Relying on both validity sources.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillProfileClientValidation,
                              features::kAutofillProfileServerValidation},
        /*disabled_features=*/{});

    std::vector<Suggestion> email_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(EMAIL_ADDRESS),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());

    for (auto* profile : profiles) {
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByClient());
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByServer());
    }
    ASSERT_EQ(1U, email_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("alice@wonderland.ca"),
              email_suggestions[0].value);

    std::vector<Suggestion> name_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(NAME_FIRST),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());
    ASSERT_EQ(2U, name_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Alice"), name_suggestions[0].value);
    EXPECT_EQ(base::ASCIIToUTF16("Marion1"), name_suggestions[1].value);
  }
  // Invalid based on client, and server. Relying only on the client source.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillProfileClientValidation},
        /*disabled_features=*/{features::kAutofillProfileServerValidation});
    std::vector<Suggestion> email_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(EMAIL_ADDRESS),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());

    for (auto* profile : profiles) {
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByClient());
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByServer());
    }
    ASSERT_EQ(1U, email_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("alice@wonderland.ca"),
              email_suggestions[0].value);

    std::vector<Suggestion> name_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(NAME_FIRST),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());
    ASSERT_EQ(2U, name_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Alice"), name_suggestions[0].value);
    EXPECT_EQ(base::ASCIIToUTF16("Marion1"), name_suggestions[1].value);
  }
  // Invalid based on client, and server. Relying on server as a validity
  // source.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillProfileServerValidation},
        /*disabled_features=*/{features::kAutofillProfileClientValidation});
    std::vector<Suggestion> email_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(EMAIL_ADDRESS),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());

    for (auto* profile : profiles) {
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByClient());
      ASSERT_EQ(profile->guid() == valid_profile.guid(),
                profile->IsValidByServer());
    }
    ASSERT_EQ(1U, email_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("alice@wonderland.ca"),
              email_suggestions[0].value);

    std::vector<Suggestion> name_suggestions =
        personal_data_->GetProfileSuggestions(AutofillType(NAME_FIRST),
                                              base::string16(), false,
                                              std::vector<ServerFieldType>());
    ASSERT_EQ(2U, name_suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Alice"), name_suggestions[0].value);
    EXPECT_EQ(base::ASCIIToUTF16("Marion1"), name_suggestions[1].value);
  }
}

// Test that local and server profiles are not shown if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_ProfileAutofillDisabled) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  const std::string kServerAddressId("server_address1");

  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, base::ASCIIToUTF16("John Doe"));
  SetServerProfiles(server_profiles);

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);
  WaitForOnPersonalDataChanged();
  ConvertWalletAddressesAndUpdateWalletCards();

  // Check that profiles were saved.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
      false, std::vector<ServerFieldType>());
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local and server profiles are not loaded into memory on start-up if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_NoProfilesLoadedIfDisabled) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  const std::string kServerAddressId("server_address1");

  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, base::ASCIIToUTF16("John Doe"));
  SetServerProfiles(server_profiles);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ConvertWalletAddressesAndUpdateWalletCards();

  // Expect 2 autofilled values or suggestions.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetProfilesToSuggest().size());

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);
  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect no profile values or suggestions were loaded.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
      false, std::vector<ServerFieldType>());
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local profiles are not added if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_NoProfilesAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);

  // Add a local profile.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Expect no profile values or suggestions were added.
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_LogProfileSuggestionsMadeWithFormatter) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FIRST), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          testing::Field(&Suggestion::value, base::ASCIIToUTF16("Hoa"))));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileSuggestionsMadeWithFormatter", true, 1);
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_ForContactForm) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FIRST), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(AllOf(
          testing::Field(
              &Suggestion::label,
              ConstructLabelLine({base::ASCIIToUTF16("(978) 674-4120"),
                                  base::ASCIIToUTF16("hoa.pham@comcast.net")})),
          testing::Field(&Suggestion::icon, ""))));
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_AddressForm) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(personal_data_->GetProfileSuggestions(
                  AutofillType(NAME_FULL), base::string16(), false,
                  std::vector<ServerFieldType>{
                      NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
                      ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP}),
              ElementsAre(AllOf(
                  testing::Field(
                      &Suggestion::label,
                      base::ASCIIToUTF16("401 Merrimack St, Lowell, MA 01852")),
                  testing::Field(&Suggestion::icon, ""))));
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_AddressPhoneForm) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(AllOf(
          testing::Field(
              &Suggestion::label,
              ConstructLabelLine({base::ASCIIToUTF16("(978) 674-4120"),
                                  base::ASCIIToUTF16("401 Merrimack St")})),
          testing::Field(&Suggestion::icon, ""))));
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_AddressEmailForm) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       EMAIL_ADDRESS}),
      ElementsAre(AllOf(
          testing::Field(
              &Suggestion::label,
              ConstructLabelLine({base::ASCIIToUTF16("401 Merrimack St"),
                                  base::ASCIIToUTF16("hoa.pham@comcast.net")})),
          testing::Field(&Suggestion::icon, ""))));
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_FormWithOneProfile) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(AllOf(testing::Field(&Suggestion::label,
                                       ConstructLabelLine({base::ASCIIToUTF16(
                                           "401 Merrimack St")})),
                        testing::Field(&Suggestion::icon, ""))));
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_AddressContactFormWithProfiles) {
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Hoa", "", "Pham", "hp@aol.com", "",
                       "216 Broadway St", "", "Lowell", "MA", "01854", "US",
                       "19784523366");

  // The profiles' use dates and counts are set make this test deterministic.
  // The suggestion created with data from profile1 should be ranked higher
  // than profile2's associated suggestion. This ensures that profile1's
  // suggestion is the first element in the collection returned by
  // GetProfileSuggestions.
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_use_count(10);
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(10));
  profile2.set_use_count(1);

  EXPECT_TRUE(profile1.HasGreaterFrecencyThan(&profile2, AutofillClock::Now()));

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(
              testing::Field(&Suggestion::label,
                             ConstructLabelLine(
                                 {base::ASCIIToUTF16("401 Merrimack St"),
                                  base::ASCIIToUTF16("(978) 674-4120"),
                                  base::ASCIIToUTF16("hoa.pham@comcast.net")})),
              testing::Field(&Suggestion::icon, "")),
          AllOf(testing::Field(
                    &Suggestion::label,
                    ConstructLabelLine({base::ASCIIToUTF16("216 Broadway St"),
                                        base::ASCIIToUTF16("(978) 452-3366"),
                                        base::ASCIIToUTF16("hp@aol.com")})),
                testing::Field(&Suggestion::icon, ""))));
}
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(OS_ANDROID) || defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_MobileShowOne) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne;
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "María", "", "Lòpez", "maria@aol.com", "",
                       "11 Elkins St", "", "Boston", "MA", "02127", "US",
                       "6172686862");

  // The profiles' use dates and counts are set make this test deterministic.
  // The suggestion created with data from profile1 should be ranked higher
  // than profile2's associated suggestion. This ensures that profile1's
  // suggestion is the first element in the collection returned by
  // GetProfileSuggestions.
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_use_count(10);
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(10));
  profile2.set_use_count(1);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  // Tests a form with name, email address, and phone number fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(AllOf(testing::Field(&Suggestion::label,
                                       base::ASCIIToUTF16("(978) 674-4120")),
                        testing::Field(&Suggestion::icon, "")),
                  AllOf(testing::Field(&Suggestion::label,
                                       base::ASCIIToUTF16("(617) 268-6862")),
                        testing::Field(&Suggestion::icon, ""))));

  // Tests a form with name, address, phone number, and email address fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       ADDRESS_HOME_CITY, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(AllOf(testing::Field(&Suggestion::label,
                                       base::ASCIIToUTF16("401 Merrimack St")),
                        testing::Field(&Suggestion::icon, "")),
                  AllOf(testing::Field(&Suggestion::label,
                                       base::ASCIIToUTF16("11 Elkins St")),
                        testing::Field(&Suggestion::icon, ""))));
}
#endif  // if defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_ANDROID) || defined(OS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_MobileShowAll) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowAll;
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "María", "", "Lòpez", "maria@aol.com", "",
                       "11 Elkins St", "", "Boston", "MA", "02127", "US",
                       "6172686862");

  // The profiles' use dates and counts are set make this test deterministic.
  // The suggestion created with data from profile1 should be ranked higher
  // than profile2's associated suggestion. This ensures that profile1's
  // suggestion is the first element in the collection returned by
  // GetProfileSuggestions.
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_use_count(10);
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(10));
  profile2.set_use_count(1);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  // Tests a form with name, email address, and phone number fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::label,
                               ConstructMobileLabelLine(
                                   {base::ASCIIToUTF16("Hoa"),
                                    base::ASCIIToUTF16("(978) 674-4120")})),
                testing::Field(&Suggestion::icon, "")),
          AllOf(testing::Field(&Suggestion::label,
                               ConstructMobileLabelLine(
                                   {base::UTF8ToUTF16("María"),
                                    base::ASCIIToUTF16("(617) 268-6862")})),
                testing::Field(&Suggestion::icon, ""))));

  // Tests a form with name, address, phone number, and email address fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), base::string16(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       ADDRESS_HOME_CITY, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::label,
                               ConstructMobileLabelLine(
                                   {base::ASCIIToUTF16("Hoa"),
                                    base::ASCIIToUTF16("401 Merrimack St"),
                                    base::ASCIIToUTF16("(978) 674-4120")})),
                testing::Field(&Suggestion::icon, "")),
          AllOf(testing::Field(&Suggestion::label,
                               ConstructMobileLabelLine(
                                   {base::UTF8ToUTF16("María"),
                                    base::ASCIIToUTF16("11 Elkins St"),
                                    base::ASCIIToUTF16("(617) 268-6862")})),
                testing::Field(&Suggestion::icon, ""))));
}
#endif  // if defined(OS_ANDROID) || defined(OS_IOS)

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(base::ASCIIToUTF16("4234 5678 9012 2110") /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(base::ASCIIToUTF16("4234 5678 9012 2110") /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesLocalCard) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(base::ASCIIToUTF16("4234567890122110") /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_TypeDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(
      base::ASCIIToUTF16("5105 1051 0510 2110") /* American Express */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_LastFourDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(base::ASCIIToUTF16("4234 5678 9012 0000") /* Visa */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Add a dupe local card of a full server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(base::ASCIIToUTF16("4234 5678 9012 2110") /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Add a dupe local card of a full server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(base::ASCIIToUTF16("4234 5678 9012 2110") /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_AlreadyServerCard) {
  std::vector<CreditCard> server_cards;
  // Create a full server card.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  server_cards.push_back(full_server_card);
  // Create a masked server card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "2110" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  server_cards.push_back(masked_card);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ASSERT_TRUE(personal_data_->IsServerCard(&full_server_card));
  ASSERT_TRUE(personal_data_->IsServerCard(&masked_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_UniqueLocalCard) {
  // Add a unique local card.
  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  ASSERT_FALSE(personal_data_->IsServerCard(&local_card));
}

// Test that a masked server card is not suggested if more that six numbers have
// been typed in the field.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_MaskedCardWithMoreThan6Numbers) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(AutofillType(CREDIT_CARD_NUMBER),
                                               base::ASCIIToUTF16("12345678"),
                                               /*include_server_cards=*/true);

  // There should be no suggestions.
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local credit cards are ordered as expected.
TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions_LocalCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Sublabel is card number when filling name (exact format depends on
  // the platform, but the last 4 digits should appear).
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(3U, suggestions.size());

  // Ordered as expected.
  EXPECT_EQ(base::ASCIIToUTF16("John Dillinger"), suggestions[0].value);
  EXPECT_TRUE(suggestions[0].label.find(base::ASCIIToUTF16("3456")) !=
              base::string16::npos);
  EXPECT_EQ(base::ASCIIToUTF16("Clyde Barrow"), suggestions[1].value);
  EXPECT_TRUE(suggestions[1].label.find(base::ASCIIToUTF16("0005")) !=
              base::string16::npos);
  EXPECT_EQ(base::ASCIIToUTF16("Bonnie Parker"), suggestions[2].value);
  EXPECT_TRUE(suggestions[2].label.find(base::ASCIIToUTF16("5100")) !=
              base::string16::npos);
}

// Test that local and server cards are ordered as expected.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_LocalAndServerCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "b460"));
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(1));

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(5U, suggestions.size());

  // All cards should be ordered as expected.
  EXPECT_EQ(base::ASCIIToUTF16("Jesse James"), suggestions[0].value);
  EXPECT_EQ(base::ASCIIToUTF16("John Dillinger"), suggestions[1].value);
  EXPECT_EQ(base::ASCIIToUTF16("Clyde Barrow"), suggestions[2].value);
  EXPECT_EQ(base::ASCIIToUTF16("Emmet Dalton"), suggestions[3].value);
  EXPECT_EQ(base::ASCIIToUTF16("Bonnie Parker"), suggestions[4].value);
}

// Test that local and server cards are not shown if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_CreditCardAutofillDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "b460"));
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(1));

  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Disable Credit card autofill.
  prefs::SetAutofillCreditCardEnabled(personal_data_->pref_service_, false);
  WaitForOnPersonalDataChanged();

  // Check that profiles were saved.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(
      0U, personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true)
              .size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local and server cards are not loaded into memory on start-up if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_NoCardsLoadedIfDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "b460"));
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(1));

  SetServerCards(server_cards);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Expect 5 autofilled values or suggestions.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  // Disable Credit card autofill.
  prefs::SetAutofillCreditCardEnabled(personal_data_->pref_service_, false);
  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect no credit card values or suggestions were loaded.
  EXPECT_EQ(
      0U, personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true)
              .size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local profiles are not added if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_NoCreditCardsAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillCreditCardEnabled(personal_data_->pref_service_, false);

  // Add a local credit card.
  CreditCard credit_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data_->AddCreditCard(credit_card);

  // Expect no profile values or suggestions were added.
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
}

// Test that expired cards are ordered by frecency and are always suggested
// after non expired cards even if they have a higher frecency score.
TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions_ExpiredCards) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add a never used non expired credit card.
  CreditCard credit_card0("002149C1-EE28-4213-A3B9-DA243FFF021B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data_->AddCreditCard(credit_card0);

  // Add an expired card with a higher frecency score.
  CreditCard credit_card1("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "1999", "1");
  credit_card1.set_use_count(300);
  credit_card1.set_use_date(AutofillClock::Now() -
                            base::TimeDelta::FromDays(10));
  personal_data_->AddCreditCard(credit_card1);

  // Add an expired card with a lower frecency score.
  CreditCard credit_card2("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  credit_card2.set_use_count(3);
  credit_card2.set_use_date(AutofillClock::Now() -
                            base::TimeDelta::FromDays(1));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "1998", "1");
  personal_data_->AddCreditCard(credit_card2);

  // Make sure everything is set up correctly.
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /* include_server_cards= */ true);
  ASSERT_EQ(3U, suggestions.size());

  // The never used non expired card should be suggested first.
  EXPECT_EQ(base::ASCIIToUTF16("Bonnie Parker"), suggestions[0].value);

  // The expired cards should be sorted by frecency
  EXPECT_EQ(base::ASCIIToUTF16("Clyde Barrow"), suggestions[1].value);
  EXPECT_EQ(base::ASCIIToUTF16("John Dillinger"), suggestions[2].value);
}

// Test cards that are expired AND disused are suppressed when supression is
// enabled and the input field is empty.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_SuppressDisusedCreditCardsOnEmptyField) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add a never used non expired local credit card.
  CreditCard credit_card0("002149C1-EE28-4213-A3B9-DA243FFF021B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data_->AddCreditCard(credit_card0);

  auto now = AutofillClock::Now();

  // Add an expired unmasked card last used 10 days ago
  CreditCard credit_card1(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "4234567890123456" /* Visa */, "04", "1999", "1");
  credit_card1.set_use_date(now - base::TimeDelta::FromDays(10));

  // Add an expired masked card last used 180 days ago.
  CreditCard credit_card2(CreditCard::MASKED_SERVER_CARD, "c987");
  test::SetCreditCardInfo(&credit_card2, "Jane Doe", "6543", "01", "1998", "1");
  credit_card2.set_use_date(now - base::TimeDelta::FromDays(181));
  credit_card2.SetNetworkForMaskedCard(kVisaCard);

  // Save the server cards and set used_date to desired dates.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(credit_card1);
  server_cards.push_back(credit_card2);
  SetServerCards(server_cards);
  personal_data_->UpdateServerCardMetadata(credit_card1);
  personal_data_->UpdateServerCardMetadata(credit_card2);

  // Add an expired local card last used 180 days ago.
  CreditCard credit_card3("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  credit_card3.set_use_date(now - base::TimeDelta::FromDays(182));
  test::SetCreditCardInfo(&credit_card3, "John Dillinger",
                          "378282246310005" /* American Express */, "01",
                          "1998", "1");
  personal_data_->AddCreditCard(credit_card3);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // Query with empty string only returns card0 and card1. Note expired
  // masked card2 is not suggested on empty fields.
  {
    std::vector<Suggestion> suggestions =
        personal_data_->GetCreditCardSuggestions(
            AutofillType(CREDIT_CARD_NAME_FULL), base::string16(),
            /*include_server_cards=*/true);
    EXPECT_EQ(2U, suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Bonnie Parker"), suggestions[0].value);
    EXPECT_EQ(base::ASCIIToUTF16("Clyde Barrow"), suggestions[1].value);
  }

  // Query with name prefix for card0 returns card0.
  {
    std::vector<Suggestion> suggestions =
        personal_data_->GetCreditCardSuggestions(
            AutofillType(CREDIT_CARD_NAME_FULL), base::ASCIIToUTF16("B"),
            /*include_server_cards=*/true);

    ASSERT_EQ(1U, suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Bonnie Parker"), suggestions[0].value);
  }

  // Query with name prefix for card1 returns card1.
  {
    std::vector<Suggestion> suggestions =
        personal_data_->GetCreditCardSuggestions(
            AutofillType(CREDIT_CARD_NAME_FULL), base::ASCIIToUTF16("Cl"),
            /*include_server_cards=*/true);

    ASSERT_EQ(1U, suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("Clyde Barrow"), suggestions[0].value);
  }

  // Query with name prefix for card2 returns card2.
  {
    std::vector<Suggestion> suggestions =
        personal_data_->GetCreditCardSuggestions(
            AutofillType(CREDIT_CARD_NAME_FULL), base::ASCIIToUTF16("Jo"),
            /*include_server_cards=*/true);

    ASSERT_EQ(1U, suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16("John Dillinger"), suggestions[0].value);
  }

  // Query with card number prefix for card1 returns card1 and card2.
  // Expired masked card2 is shown when user starts to type credit card
  // number because we are not sure if it is the masked card that they want.
  {
    std::vector<Suggestion> suggestions =
        personal_data_->GetCreditCardSuggestions(
            AutofillType(CREDIT_CARD_NUMBER), base::ASCIIToUTF16("4234"),
            /*include_server_cards=*/true);

    ASSERT_EQ(2U, suggestions.size());
    EXPECT_EQ(base::UTF8ToUTF16(std::string("Visa  ") +
                                test::ObfuscatedCardDigitsAsUTF8("3456")),
              suggestions[0].value);
    EXPECT_EQ(base::UTF8ToUTF16(std::string("Visa  ") +
                                test::ObfuscatedCardDigitsAsUTF8("6543")),
              suggestions[1].value);
  }
}

// Test that a card that doesn't have a number is not shown in the suggestions
// when querying credit cards by their number.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_NumberMissing_QueryNumberField) {
  // Create one normal credit card and one credit card with the number missing.
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card0.set_use_count(3);
  credit_card0.set_use_date(AutofillClock::Now() -
                            base::TimeDelta::FromDays(1));
  personal_data_->AddCreditCard(credit_card0);

  CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  credit_card1.set_use_count(300);
  credit_card1.set_use_date(AutofillClock::Now() -
                            base::TimeDelta::FromDays(10));
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2999",
                          "1");
  personal_data_->AddCreditCard(credit_card1);

  // Make sure everything is set up correctly.
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(2U, personal_data_->GetCreditCards().size());

  // Sublabel is expiration date when filling card number. The second card
  // doesn't have a number so it should not be included in the suggestions.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NUMBER),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(base::UTF8ToUTF16(std::string("Amex  ") +
                              test::ObfuscatedCardDigitsAsUTF8("0005")),
            suggestions[0].value);

#if defined(OS_ANDROID) || defined(OS_IOS)
  EXPECT_EQ(base::ASCIIToUTF16("04/99"), suggestions[0].label);
#else
  EXPECT_EQ(base::ASCIIToUTF16("Expires on 04/99"), suggestions[0].label);
#endif  // defined (OS_ANDROID) || defined(OS_IOS)
}

// Test that a card that doesn't have a number is shown in the suggestion list
// with nickname if a non-number field is queried.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_NumberMissing_QueryNonNumberField) {
  base::test::ScopedFeatureList scoped_feature_list;
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  CreditCard credit_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger", "", "01", "2999",
                          "1");
  credit_card.SetNickname(base::UTF8ToUTF16("nickname"));
  personal_data_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // Ensures the suggestion label is the card's nickname.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(base::UTF8ToUTF16("nickname"), suggestions[0].label);
}

// Tests the suggestions of duplicate local and server credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions_ServerDuplicates) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards. If there are local dupes, the locals should be
  // hidden.
  std::vector<CreditCard> server_cards;
  // This server card matches a local card, except the local card is missing the
  // number. This should count as a dupe and thus not be shown in the
  // suggestions since the locally saved card takes precedence.
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "3456" /* Visa */, "01", "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(15));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  // This unmasked server card is an exact dupe of a local card. Therefore only
  // this card should appear in the suggestions as full server cards have
  // precedence over local cards.
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  server_cards.back().set_use_count(1);
  server_cards.back().set_use_date(AutofillClock::Now() -
                                   base::TimeDelta::FromDays(15));

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(3U, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16("John Dillinger"), suggestions[0].value);
  EXPECT_EQ(base::ASCIIToUTF16("Clyde Barrow"), suggestions[1].value);
  EXPECT_EQ(base::ASCIIToUTF16("Bonnie Parker"), suggestions[2].value);

  suggestions = personal_data_->GetCreditCardSuggestions(
      AutofillType(CREDIT_CARD_NUMBER), /*field_contents=*/base::string16(),
      /*include_server_cards=*/true);
  ASSERT_EQ(3U, suggestions.size());
  EXPECT_EQ(base::UTF8ToUTF16(std::string("Visa  ") +
                              test::ObfuscatedCardDigitsAsUTF8("3456")),
            suggestions[0].value);
  EXPECT_EQ(base::UTF8ToUTF16(std::string("Amex  ") +
                              test::ObfuscatedCardDigitsAsUTF8("0005")),
            suggestions[1].value);
  EXPECT_EQ(base::UTF8ToUTF16(std::string("Mastercard  ") +
                              test::ObfuscatedCardDigitsAsUTF8("5100")),
            suggestions[2].value);
}

// Tests that a full server card can be a dupe of more than one local card.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_ServerCardDuplicateOfMultipleLocalCards) {
  SetUpReferenceLocalCreditCards();

  // Add a duplicate server card.
  std::vector<CreditCard> server_cards;
  // This unmasked server card is an exact dupe of a local card. Therefore only
  // the local card should appear in the suggestions.
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME_FULL),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(3U, suggestions.size());

  // Add a second dupe local card to make sure a full server card can be a dupe
  // of more than one local card.
  CreditCard credit_card3("4141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde Barrow", "", "04", "", "");
  personal_data_->AddCreditCard(credit_card3);

  WaitForOnPersonalDataChanged();

  suggestions = personal_data_->GetCreditCardSuggestions(
      AutofillType(CREDIT_CARD_NAME_FULL),
      /*field_contents=*/base::string16(), /*include_server_cards=*/true);
  ASSERT_EQ(3U, suggestions.size());
}

// Tests that only the full server card is kept when deduping with a local
// duplicate of it.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardToSuggest_FullServerShadowsLocal) {
  std::list<CreditCard*> credit_cards;

  // Create 3 different local credit cards.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  local_card.set_use_count(3);
  local_card.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));
  credit_cards.push_back(&local_card);

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() -
                                base::TimeDelta::FromDays(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card(credit_cards.front());
  EXPECT_TRUE(*deduped_card == full_server_card);
}

// Tests that only the local card is kept when deduping with a masked server
// duplicate of it.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_LocalShadowsMasked) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  local_card.set_use_count(300);
  local_card.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(10));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.set_use_count(2);
  masked_card.set_use_date(AutofillClock::Now() -
                           base::TimeDelta::FromDays(15));
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card(credit_cards.front());
  EXPECT_TRUE(*deduped_card == local_card);
}

// Tests that identical full server and masked credit cards are not deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_FullServerAndMasked) {
  std::list<CreditCard*> credit_cards;

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() -
                                base::TimeDelta::FromDays(15));
  credit_cards.push_back(&full_server_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.set_use_count(2);
  masked_card.set_use_date(AutofillClock::Now() -
                           base::TimeDelta::FromDays(15));
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(2U, credit_cards.size());
}

// Tests that different local, masked, and full server credit cards are not
// deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_DifferentCards) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                        test::kEmptyOrigin);
  local_card.set_use_count(1);
  local_card.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "5105105105105100" /* Mastercard */, "", "", "");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is different from the local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "0005", "12", "2999",
                          "1");
  masked_card.set_use_count(3);
  masked_card.set_use_date(AutofillClock::Now() -
                           base::TimeDelta::FromDays(15));
  // credit_card4.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  // Create a full server card that is slightly different of the two other
  // cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() -
                                base::TimeDelta::FromDays(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(3U, credit_cards.size());
}

TEST_F(PersonalDataManagerTest, RecordUseOf) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  AutofillProfile profile(test::GetFullProfile());
  EXPECT_EQ(1U, profile.use_count());
  EXPECT_EQ(kArbitraryTime, profile.use_date());
  EXPECT_EQ(kArbitraryTime, profile.modification_date());
  AddProfileToPersonalDataManager(profile);

  CreditCard credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  EXPECT_EQ(1U, credit_card.use_count());
  EXPECT_EQ(kArbitraryTime, credit_card.use_date());
  EXPECT_EQ(kArbitraryTime, credit_card.modification_date());
  personal_data_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Set the current time to another value.
  test_clock.SetNow(kSomeLaterTime);

  // Notify the PDM that the profile and credit card were used.
  AutofillProfile* added_profile =
      personal_data_->GetProfileByGUID(profile.guid());
  ASSERT_TRUE(added_profile);
  EXPECT_EQ(*added_profile, profile);
  EXPECT_EQ(1U, added_profile->use_count());
  EXPECT_EQ(kArbitraryTime, added_profile->use_date());
  EXPECT_EQ(kArbitraryTime, added_profile->modification_date());

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);

  personal_data_->RecordUseOf(profile);

  run_loop.Run();

  CreditCard* added_card =
      personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_card);
  EXPECT_EQ(*added_card, credit_card);
  EXPECT_EQ(1U, added_card->use_count());
  EXPECT_EQ(kArbitraryTime, added_card->use_date());
  EXPECT_EQ(kArbitraryTime, added_card->modification_date());
  personal_data_->RecordUseOf(credit_card);

  // Verify usage stats are updated.
  added_profile = personal_data_->GetProfileByGUID(profile.guid());
  ASSERT_TRUE(added_profile);
  EXPECT_EQ(2U, added_profile->use_count());
  EXPECT_EQ(kSomeLaterTime, added_profile->use_date());
  EXPECT_EQ(kArbitraryTime, added_profile->modification_date());

  added_card = personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_card);
  EXPECT_EQ(2U, added_card->use_count());
  EXPECT_EQ(kSomeLaterTime, added_card->use_date());
  EXPECT_EQ(kArbitraryTime, added_card->modification_date());
}

TEST_F(PersonalDataManagerTest, ClearAllServerData) {
  // Add a server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "3456" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // The card and profile should be there.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());

  personal_data_->ClearAllServerData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
}

TEST_F(PersonalDataManagerTest, ClearAllLocalData) {
  // Add some local data.
  AddProfileToPersonalDataManager(test::GetFullProfile());
  personal_data_->AddCreditCard(test::GetCreditCard());
  personal_data_->Refresh();

  // The card and profile should be there.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());
  EXPECT_FALSE(personal_data_->GetProfiles().empty());

  personal_data_->ClearAllLocalData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
}

// Tests the SaveImportedProfile method with different profiles to make sure the
// merge logic works correctly.
typedef struct {
  autofill::ServerFieldType field_type;
  std::string field_value;
} ProfileField;

typedef std::vector<ProfileField> ProfileFields;

typedef struct {
  // Each test starts with a default pre-existing profile and applies these
  // changes to it.
  ProfileFields changes_to_original;
  // Each test saves a second profile. Applies these changes to the default
  // values before saving.
  ProfileFields changes_to_new;
  // For tests with profile merging, makes sure that these fields' values are
  // the ones we expect (depending on the test).
  ProfileFields changed_field_values;
} SaveImportedProfileTestCase;

class SaveImportedProfileTest
    : public PersonalDataManagerHelper,
      public testing::TestWithParam<
          std::tuple<bool, SaveImportedProfileTestCase>> {
 public:
  SaveImportedProfileTest() {}
  ~SaveImportedProfileTest() override {}

  void SetUp() override {
    InitializeFeatures();
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL);
  }

  void TearDown() override { TearDownTest(); }

  void InitializeFeatures() {
    structured_names_enabled_ = std::get<0>(GetParam());
    if (structured_names_enabled_) {
      scoped_features_.InitAndEnableFeature(
          features::kAutofillEnableSupportForMoreStructureInNames);
    } else {
      scoped_features_.InitAndDisableFeature(
          features::kAutofillEnableSupportForMoreStructureInNames);
    }
  }

  bool StructuredNames() const { return structured_names_enabled_; }

 private:
  bool structured_names_enabled_;
  base::test::ScopedFeatureList scoped_features_;
};

TEST_P(SaveImportedProfileTest, SaveImportedProfile) {
  // Create the test clock.
  TestAutofillClock test_clock;
  auto test_case = std::get<1>(GetParam());
  // Set the time to a specific value.
  test_clock.SetNow(kArbitraryTime);

  AutofillProfile original_profile = GetDefaultProfile();

  // Apply changes to the original profile (if applicable).
  for (ProfileField change : test_case.changes_to_original) {
    original_profile.SetRawInfoWithVerificationStatus(
        change.field_type, base::UTF8ToUTF16(change.field_value),
        structured_address::VerificationStatus::kObserved);
  }

  // Initialize PersonalDataManager with the original profile.
  original_profile.FinalizeAfterImport();
  SetUpReferenceProfile(original_profile);

  // Set the time to a bigger value.
  test_clock.SetNow(kSomeLaterTime);

  AutofillProfile profile2(GetDefaultProfile());

  // Apply changes to the second profile (if applicable).
  for (ProfileField change : test_case.changes_to_new) {
    profile2.SetRawInfoWithVerificationStatus(
        change.field_type, base::UTF8ToUTF16(change.field_value),
        structured_address::VerificationStatus::kObserved);
  }

  profile2.FinalizeAfterImport();
  SaveImportedProfileToPersonalDataManager(profile2);

  const std::vector<AutofillProfile*>& saved_profiles =
      personal_data_->GetProfiles();

  // Get the set of profiles persisted in the db.
  std::vector<std::unique_ptr<AutofillProfile>> db_profiles;
  profile_autofill_table_->GetAutofillProfiles(&db_profiles);

  // Expect the profiles held in-memory by PersonalDataManager and the db
  // profiles to be the same.
  EXPECT_EQ(db_profiles.size(), saved_profiles.size());
  for (const auto& it : db_profiles) {
    AutofillProfile* inmemory_profile =
        personal_data_->GetProfileByGUID(it->guid());
    ASSERT_TRUE(inmemory_profile != nullptr);
    EXPECT_TRUE(it->EqualsIncludingUsageStatsForTesting(*inmemory_profile));
  }

  // If there are no merge changes to verify, make sure that two profiles were
  // saved.
  if (test_case.changed_field_values.empty()) {
    EXPECT_EQ(2U, saved_profiles.size());
  } else {
    EXPECT_EQ(1U, saved_profiles.size());

    // Make sure the new information was merged correctly.
    for (ProfileField changed_field : test_case.changed_field_values) {
      EXPECT_EQ(base::UTF8ToUTF16(changed_field.field_value),
                saved_profiles.front()->GetRawInfo(changed_field.field_type));
    }
    // Verify that the merged profile's use count, use date and modification
    // date were properly updated.
    EXPECT_EQ(1U, saved_profiles.front()->use_count());
    EXPECT_EQ(kSomeLaterTime, saved_profiles.front()->use_date());
    // For structured names, the modification date is only updated when the
    // profile actually changes.
    if (StructuredNames()) {
      EXPECT_EQ(*saved_profiles.front() == original_profile ? kArbitraryTime
                                                            : kSomeLaterTime,
                saved_profiles.front()->modification_date());
    } else {
      // The reason why this profiles changes is that the initial is set to M,
      // but once it is retrieved from the db it is reparsed reset due to its
      // internal name parsing logic.
      EXPECT_EQ(kSomeLaterTime, saved_profiles.front()->modification_date());
    }
  }

  // Erase the profiles for the next test.
  ResetProfiles();
}

INSTANTIATE_TEST_SUITE_P(
    PersonalDataManagerTest,
    SaveImportedProfileTest,
    testing::Combine(
        testing::Bool(),  // Test with and without the feature
                          // |kAutofillSupportForMoreStructuredNames|.
        testing::Values(
            // Test that saving an identical profile except for the name results
            // in two profiles being saved.
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{NAME_FIRST, "Marionette"}}},

            // Test that saving an identical profile except with the middle name
            // initial instead of the full middle name results in the profiles
            // getting merged and the full middle name being kept.
            SaveImportedProfileTestCase{
                ProfileFields(),
                {{NAME_MIDDLE, "M"}},
                {{NAME_MIDDLE, "Mitchell"},
                 {NAME_FULL, "Marion Mitchell Morrison"}}},

            // Test that saving an identical profile except with the full middle
            // name instead of the middle name initial results in the profiles
            // getting merged and the full middle name replacing the initial.
            SaveImportedProfileTestCase{{{NAME_MIDDLE, "M"}},
                                        {{NAME_MIDDLE, "Mitchell"}},
                                        {{NAME_MIDDLE, "Mitchell"}}},

            // Test that saving an identical profile except with no middle name
            // results in the profiles getting merged and the full middle name
            // being kept.
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{NAME_MIDDLE, ""}},
                                        {{NAME_MIDDLE, "Mitchell"}}},

            // Test that saving an identical profile except with a middle name
            // initial results in the profiles getting merged and the middle
            // name initial being saved.
            SaveImportedProfileTestCase{{{NAME_MIDDLE, ""}},
                                        {{NAME_MIDDLE, "M"}},
                                        {{NAME_MIDDLE, "M"}}},

            // Test that saving an identical profile except with a middle name
            // results in the profiles getting merged and the full middle name
            // being saved.
            SaveImportedProfileTestCase{{{NAME_MIDDLE, ""}},
                                        {{NAME_MIDDLE, "Mitchell"}},
                                        {{NAME_MIDDLE, "Mitchell"}}},

            // Test that saving a identical profile except with the full name
            // set instead of the name parts results in the two profiles being
            // merged and all the name parts kept and the full name being added.
            SaveImportedProfileTestCase{
                {
                    {NAME_FIRST, "Marion"},
                    {NAME_MIDDLE, "Mitchell"},
                    {NAME_LAST, "Morrison"},
                    {NAME_FULL, ""},
                },
                {
                    {NAME_FIRST, ""},
                    {NAME_MIDDLE, ""},
                    {NAME_LAST, ""},
                    {NAME_FULL, "Marion Mitchell Morrison"},
                },
                {
                    {NAME_FIRST, "Marion"},
                    {NAME_MIDDLE, "Mitchell"},
                    {NAME_LAST, "Morrison"},
                    {NAME_FULL, "Marion Mitchell Morrison"},
                },
            },

            // Test that saving a identical profile except with the name parts
            // set instead of the full name results in the two profiles being
            // merged and the full name being kept and all the name parts being
            // added.
            SaveImportedProfileTestCase{
                {
                    {NAME_FIRST, ""},
                    {NAME_MIDDLE, ""},
                    {NAME_LAST, ""},
                    {NAME_FULL, "Marion Mitchell Morrison"},
                },
                {
                    {NAME_FIRST, "Marion"},
                    {NAME_MIDDLE, "Mitchell"},
                    {NAME_LAST, "Morrison"},
                    {NAME_FULL, ""},
                },
                {
                    {NAME_FIRST, "Marion"},
                    {NAME_MIDDLE, "Mitchell"},
                    {NAME_LAST, "Morrison"},
                    {NAME_FULL, "Marion Mitchell Morrison"},
                },
            },

            // Test that saving a profile that has only a full name set does not
            // get merged with a profile with only the name parts set if the
            // names are different.
            SaveImportedProfileTestCase{
                {
                    {NAME_FIRST, "Marion"},
                    {NAME_MIDDLE, "Mitchell"},
                    {NAME_LAST, "Morrison"},
                    {NAME_FULL, ""},
                },
                {
                    {NAME_FIRST, ""},
                    {NAME_MIDDLE, ""},
                    {NAME_LAST, ""},
                    {NAME_FULL, "John Thompson Smith"},
                },
            },

            // Test that saving a profile that has only the name parts set does
            // not get merged with a profile with only the full name set if the
            // names are different.
            SaveImportedProfileTestCase{
                {
                    {NAME_FIRST, ""},
                    {NAME_MIDDLE, ""},
                    {NAME_LAST, ""},
                    {NAME_FULL, "John Thompson Smith"},
                },
                {
                    {NAME_FIRST, "Marion"},
                    {NAME_MIDDLE, "Mitchell"},
                    {NAME_LAST, "Morrison"},
                    {NAME_FULL, ""},
                },
            },

            // Test that saving an identical profile except for the first
            // address line results in two profiles being saved.
            SaveImportedProfileTestCase{
                ProfileFields(),
                {{ADDRESS_HOME_LINE1, "123 Aquarium St."}}},

            // Test that saving an identical profile except for the second
            // address line results in two profiles being saved.
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{ADDRESS_HOME_LINE2, "unit 7"}}},

            // Tests that saving an identical profile that has a new piece of
            // information (company name) results in a merge and that the
            // original empty value gets overwritten by the new information.
            SaveImportedProfileTestCase{{{COMPANY_NAME, ""}},
                                        ProfileFields(),
                                        {{COMPANY_NAME, "Fox"}}},

            // Tests that saving an identical profile except a loss of
            // information results in a merge but the original value is not
            // overwritten (no information loss).
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{COMPANY_NAME, ""}},
                                        {{COMPANY_NAME, "Fox"}}},

            // Tests that saving an identical profile except a slightly
            // different postal code results in a merge with the new value kept.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_ZIP, "R2C 0A1"}},
                                        {{ADDRESS_HOME_ZIP, "R2C0A1"}},
                                        {{ADDRESS_HOME_ZIP, "R2C0A1"}}},
            SaveImportedProfileTestCase{{{ADDRESS_HOME_ZIP, "R2C0A1"}},
                                        {{ADDRESS_HOME_ZIP, "R2C 0A1"}},
                                        {{ADDRESS_HOME_ZIP, "R2C 0A1"}}},
            SaveImportedProfileTestCase{{{ADDRESS_HOME_ZIP, "r2c 0a1"}},
                                        {{ADDRESS_HOME_ZIP, "R2C0A1"}},
                                        {{ADDRESS_HOME_ZIP, "R2C0A1"}}},

            // Tests that saving an identical profile plus a new piece of
            // information on the address line 2 results in a merge and that the
            // original empty value gets overwritten by the new information.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE2, ""}},
                                        ProfileFields(),
                                        {{ADDRESS_HOME_LINE2, "unit 5"}}},

            // Tests that saving an identical profile except a loss of
            // information on the address line 2 results in a merge but that the
            // original value gets not overwritten (no information loss).
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{ADDRESS_HOME_LINE2, ""}},
                                        {{ADDRESS_HOME_LINE2, "unit 5"}}},

            // Tests that saving an identical except with more punctuation in
            // the fist address line, while the second is empty, results in a
            // merge and that the original address gets overwritten.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE2, ""}},
                                        {{ADDRESS_HOME_LINE2, ""},
                                         {ADDRESS_HOME_LINE1, "123, Zoo St."}},
                                        {{ADDRESS_HOME_LINE1, "123, Zoo St."}}},

            // Tests that saving an identical profile except with less
            // punctuation in the fist address line, while the second is empty,
            // results in a merge and that the longer address is retained.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE2, ""},
                                         {ADDRESS_HOME_LINE1, "123, Zoo St."}},
                                        {{ADDRESS_HOME_LINE2, ""}},
                                        {{ADDRESS_HOME_LINE1, "123 Zoo St"}}},

            // Tests that saving an identical profile except additional
            // punctuation in the two address lines results in a merge and that
            // the newer address is retained.
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{ADDRESS_HOME_LINE1, "123, Zoo St."},
                                         {ADDRESS_HOME_LINE2, "unit. 5"}},
                                        {{ADDRESS_HOME_LINE1, "123, Zoo St."},
                                         {ADDRESS_HOME_LINE2, "unit. 5"}}},

            // Tests that saving an identical profile except less punctuation in
            // the two address lines results in a merge and that the newer
            // address is retained.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE1, "123, Zoo St."},
                                         {ADDRESS_HOME_LINE2, "unit. 5"}},
                                        ProfileFields(),
                                        {{ADDRESS_HOME_LINE1, "123 Zoo St"},
                                         {ADDRESS_HOME_LINE2, "unit 5"}}},

            // Tests that saving an identical profile with accented characters
            // in the two address lines results in a merge and that the newer
            // address is retained.
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{ADDRESS_HOME_LINE1, "123 Zôö St"},
                                         {ADDRESS_HOME_LINE2, "üñìt 5"}},
                                        {{ADDRESS_HOME_LINE1, "123 Zôö St"},
                                         {ADDRESS_HOME_LINE2, "üñìt 5"}}},

            // Tests that saving an identical profile without accented
            // characters in the two address lines results in a merge and that
            // the newer address is retained.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE1, "123 Zôö St"},
                                         {ADDRESS_HOME_LINE2, "üñìt 5"}},
                                        ProfileFields(),
                                        {{ADDRESS_HOME_LINE1, "123 Zoo St"},
                                         {ADDRESS_HOME_LINE2, "unit 5"}}},

            // Tests that saving an identical profile except that the address
            // line 1 is in the address line 2 results in a merge and that the
            // multi-lne address is retained.
            SaveImportedProfileTestCase{
                ProfileFields(),
                {{ADDRESS_HOME_LINE1, "123 Zoo St, unit 5"},
                 {ADDRESS_HOME_LINE2, ""}},
                {{ADDRESS_HOME_LINE1, "123 Zoo St"},
                 {ADDRESS_HOME_LINE2, "unit 5"}}},

            // Tests that saving an identical profile except that the address
            // line 2 contains part of the old address line 1 results in a merge
            // and that the original address lines of the reference profile get
            // overwritten.
            SaveImportedProfileTestCase{
                {{ADDRESS_HOME_LINE1, "123 Zoo St, unit 5"},
                 {ADDRESS_HOME_LINE2, ""}},
                ProfileFields(),
                {{ADDRESS_HOME_LINE1, "123 Zoo St"},
                 {ADDRESS_HOME_LINE2, "unit 5"}}},

            // Tests that saving an identical profile except that the state is
            // the abbreviation instead of the full form results in a merge and
            // that the original state gets overwritten.
            SaveImportedProfileTestCase{{{ADDRESS_HOME_STATE, "California"}},
                                        ProfileFields(),
                                        {{ADDRESS_HOME_STATE, "CA"}}},

            // Tests that saving an identical profile except that the state is
            // the full form instead of the abbreviation results in a merge and
            // that the abbreviated state is retained.
            SaveImportedProfileTestCase{ProfileFields(),
                                        {{ADDRESS_HOME_STATE, "California"}},
                                        {{ADDRESS_HOME_STATE, "CA"}}},

            // Tests that saving and identical profile except that the company
            // name has different punctuation and case results in a merge and
            // that the syntax of the new profile replaces the old one.
            SaveImportedProfileTestCase{{{COMPANY_NAME, "Stark inc"}},
                                        {{COMPANY_NAME, "Stark Inc."}},
                                        {{COMPANY_NAME, "Stark Inc."}}})));

// Tests that MergeProfile tries to merge the imported profile into the
// existing profile in decreasing order of frecency.
TEST_F(PersonalDataManagerTest, MergeProfile_Frecency) {
  // Create two very similar profiles except with different company names.
  std::unique_ptr<AutofillProfile> profile1 = std::make_unique<AutofillProfile>(
      base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile1.get(), "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "SNP", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  AutofillProfile* profile2 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");

  // Give the "Fox" profile a bigger frecency score.
  profile2->set_use_count(15);

  // Create the |existing_profiles| vector.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::move(profile1));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile2));

  // Create a new imported profile with no company name.
  AutofillProfile imported_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&imported_profile, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");

  // Merge the imported profile into the existing profiles.
  std::vector<AutofillProfile> profiles;
  std::string guid = AutofillProfileComparator::MergeProfile(
      imported_profile, existing_profiles, "US-EN", &profiles);

  // The new profile should be merged into the "fox" profile.
  EXPECT_EQ(profile2->guid(), guid);
}

// Tests that MergeProfile produces a merged profile with the expected usage
// statistics.
// Flaky on TSan, see crbug.com/686226.
#if defined(THREAD_SANITIZER)
#define MAYBE_MergeProfile_UsageStats DISABLED_MergeProfile_UsageStats
#else
#define MAYBE_MergeProfile_UsageStats MergeProfile_UsageStats
#endif
TEST_F(PersonalDataManagerTest, MAYBE_MergeProfile_UsageStats) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create an initial profile with a use count of 10, an old use date and an
  // old modification date of 4 days ago.
  AutofillProfile* profile =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "SNP", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile->set_use_count(4U);
  EXPECT_EQ(kArbitraryTime, profile->use_date());
  EXPECT_EQ(kArbitraryTime, profile->modification_date());

  // Create the |existing_profiles| vector.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile));

  // Change the current date.
  test_clock.SetNow(kSomeLaterTime);

  // Create a new imported profile that will get merged with the existing one.
  AutofillProfile imported_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&imported_profile, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");

  // Change the current date.
  test_clock.SetNow(kMuchLaterTime);

  // Merge the imported profile into the existing profiles.
  std::vector<AutofillProfile> profiles;
  std::string guid = AutofillProfileComparator::MergeProfile(
      imported_profile, existing_profiles, "US-EN", &profiles);

  // The new profile should be merged into the existing profile.
  EXPECT_EQ(profile->guid(), guid);
  EXPECT_EQ(1U, profiles.size());
  // The use count should have be max(4, 1) => 4.
  EXPECT_EQ(4U, profiles[0].use_count());
  // The use date should be the one of the most recent profile, which is
  // kSecondArbitraryTime.
  EXPECT_EQ(kSomeLaterTime, profiles[0].use_date());
  // Since the merge is considered a modification, the modification_date should
  // be set to kMuchLaterTime.
  EXPECT_EQ(kMuchLaterTime, profiles[0].modification_date());
}

// Tests that DedupeProfiles sets the correct profile guids to
// delete after merging similar profiles.
TEST_F(PersonalDataManagerTest, DedupeProfiles_ProfilesToDelete) {
  // Create the profile for which to find duplicates. It has the highest
  // frecency.
  AutofillProfile* profile1 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile1, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile1->set_use_count(9);

  // Create a different profile that should not be deduped (different address).
  AutofillProfile* profile2 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "1234 Other Street", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile2->set_use_count(7);

  // Create a profile similar to profile1 which should be deduped.
  AutofillProfile* profile3 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile3->set_use_count(5);

  // Create another different profile that should not be deduped (different
  // name).
  AutofillProfile* profile4 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile4, "Marjorie", "Jacqueline", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile4->set_use_count(3);

  // Create another profile similar to profile1. Since that one has the lowest
  // frecency, the result of the merge should be in this profile at the end of
  // the test.
  AutofillProfile* profile5 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile5, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile5->set_use_count(1);

  // Add the profiles.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile1));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile2));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile3));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile4));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile5));

  // Enable the profile cleanup.
  EnableAutofillProfileCleanup();

  base::HistogramTester histogram_tester;
  std::unordered_map<std::string, std::string> guids_merge_map;
  std::unordered_set<std::string> profiles_to_delete;
  personal_data_->DedupeProfiles(&existing_profiles, &profiles_to_delete,
                                 &guids_merge_map);
  // 5 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 5, 1);
  // 2 profiles were removed (profiles 1 and 3).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Profile1 should be deleted because it was sent as the profile to merge and
  // thus was merged into profile3 and then into profile5.
  EXPECT_TRUE(profiles_to_delete.count(profile1->guid()));

  // Profile3 should be deleted because profile1 was merged into it and the
  // resulting profile was then merged into profile5.
  EXPECT_TRUE(profiles_to_delete.count(profile3->guid()));

  // Only these two profiles should be deleted.
  EXPECT_EQ(2U, profiles_to_delete.size());

  // All profiles should still be present in |existing_profiles|.
  EXPECT_EQ(5U, existing_profiles.size());
}

// Tests that DedupeProfiles sets the correct merge mapping for billing address
// id references.
TEST_F(PersonalDataManagerTest, DedupeProfiles_GuidsMergeMap) {
  // Create the profile for which to find duplicates. It has the highest
  // frecency.
  AutofillProfile* profile1 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile1, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile1->set_use_count(9);

  // Create a different profile that should not be deduped (different address).
  AutofillProfile* profile2 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "1234 Other Street", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile2->set_use_count(7);

  // Create a profile similar to profile1 which should be deduped.
  AutofillProfile* profile3 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile3->set_use_count(5);

  // Create another different profile that should not be deduped (different
  // name).
  AutofillProfile* profile4 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile4, "Marjorie", "Jacqueline", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile4->set_use_count(3);

  // Create another profile similar to profile1. Since that one has the lowest
  // frecency, the result of the merge should be in this profile at the end of
  // the test.
  AutofillProfile* profile5 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile5, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile5->set_use_count(1);

  // Add the profiles.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile1));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile2));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile3));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile4));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile5));

  // Enable the profile cleanup.
  EnableAutofillProfileCleanup();

  std::unordered_map<std::string, std::string> guids_merge_map;
  std::unordered_set<std::string> profiles_to_delete;

  personal_data_->DedupeProfiles(&existing_profiles, &profiles_to_delete,
                                 &guids_merge_map);

  // The two profile merges should be recorded in the map.
  EXPECT_EQ(2U, guids_merge_map.size());

  // Profile 1 was merged into profile 3.
  ASSERT_TRUE(guids_merge_map.count(profile1->guid()));
  EXPECT_TRUE(guids_merge_map.at(profile1->guid()) == profile3->guid());

  // Profile 3 was merged into profile 5.
  ASSERT_TRUE(guids_merge_map.count(profile3->guid()));
  EXPECT_TRUE(guids_merge_map.at(profile3->guid()) == profile5->guid());
}

// Tests that UpdateCardsBillingAddressReference sets the correct billing
// address id as specified in the map.
TEST_F(PersonalDataManagerTest, UpdateCardsBillingAddressReference) {
  /*  The merges will be as follow:

      A -> B            F (not merged)
             \
               -> E
             /
      C -> D
  */

  std::unordered_map<std::string, std::string> guids_merge_map;
  guids_merge_map.insert(std::pair<std::string, std::string>("A", "B"));
  guids_merge_map.insert(std::pair<std::string, std::string>("C", "D"));
  guids_merge_map.insert(std::pair<std::string, std::string>("B", "E"));
  guids_merge_map.insert(std::pair<std::string, std::string>("D", "E"));

  // Create cards that use A, D, E and F as their billing address id.
  CreditCard* credit_card1 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card1->set_billing_address_id("A");
  CreditCard* credit_card2 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card2->set_billing_address_id("D");
  CreditCard* credit_card3 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card3->set_billing_address_id("E");
  CreditCard* credit_card4 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card4->set_billing_address_id("F");

  // Add the credit cards to the database.
  personal_data_->local_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card1));
  personal_data_->server_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card2));
  personal_data_->local_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card3));
  personal_data_->server_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card4));

  personal_data_->UpdateCardsBillingAddressReference(guids_merge_map);

  // The first card's billing address should now be E.
  EXPECT_EQ("E", credit_card1->billing_address_id());
  // The second card's billing address should now be E.
  EXPECT_EQ("E", credit_card2->billing_address_id());
  // The third card's billing address should still be E.
  EXPECT_EQ("E", credit_card3->billing_address_id());
  // The fourth card's billing address should still be F.
  EXPECT_EQ("F", credit_card4->billing_address_id());
}

// Tests that ApplyDedupingRoutine updates the credit cards' billing address id
// based on the deduped profiles.
TEST_F(PersonalDataManagerTest,
       ApplyDedupingRoutine_CardsBillingAddressIdUpdated) {
  // A set of 6 profiles will be created. They should merge in this way:
  //  1 -> 2 -> 3
  //  4 -> 5
  //  6
  // Set their frencency score so that profile 3 has a higher score than 5, and
  // 5 has a higher score than 6. This will ensure a deterministic order when
  // verifying results.

  // Create a set of 3 profiles to be merged together.
  // Create a profile with a higher frecency score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(12);
  profile1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));

  // Create a profile with a medium frecency score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));

  // Create a profile with a lower frecency score.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(5));

  // Create a set of two profiles to be merged together.
  // Create a profile with a higher frecency score.
  AutofillProfile profile4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Marge", "B", "Simpson",
                       "marge.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile4.set_use_count(11);
  profile4.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));

  // Create a profile with a lower frecency score.
  AutofillProfile profile5(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "Marge", "B", "Simpson",
                       "marge.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile5.set_use_count(5);
  profile5.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));

  // Create a unique profile.
  AutofillProfile profile6(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "Bart", "J", "Simpson",
                       "bart.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile6.set_use_count(10);
  profile6.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));

  // Add three credit cards. Give them a frecency score so that they are
  // suggested in order (1, 2, 3). This will ensure a deterministic order for
  // verifying results.
  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_count(10);

  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card2.set_use_count(5);

  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");
  credit_card3.set_use_count(1);

  // Associate the first card with profile1.
  credit_card1.set_billing_address_id(profile1.guid());
  // Associate the second card with profile4.
  credit_card2.set_billing_address_id(profile4.guid());
  // Associate the third card with profile6.
  credit_card3.set_billing_address_id(profile6.guid());

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);
  AddProfileToPersonalDataManager(profile4);
  AddProfileToPersonalDataManager(profile5);
  AddProfileToPersonalDataManager(profile6);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);

  WaitForOnPersonalDataChanged();

  // Make sure the 6 profiles and 3 credit cards were saved.
  EXPECT_EQ(6U, personal_data_->GetProfiles().size());
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  // Get the profiles and cards sorted by frecency to have a deterministic
  // order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();
  std::vector<CreditCard*> credit_cards =
      personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true);

  // |profile1| should have been merged into |profile2| which should then have
  // been merged into |profile3|. |profile4| should have been merged into
  // |profile5| and |profile6| should not have merged. Therefore there should be
  // 3 profile left.
  ASSERT_EQ(3U, profiles.size());

  // Make sure the remaining profiles are the expected ones.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  EXPECT_EQ(profile5.guid(), profiles[1]->guid());
  EXPECT_EQ(profile6.guid(), profiles[2]->guid());

  // |credit_card1|'s billing address should now be profile 3.
  EXPECT_EQ(profile3.guid(), credit_cards[0]->billing_address_id());

  // |credit_card2|'s billing address should now be profile 5.
  EXPECT_EQ(profile5.guid(), credit_cards[1]->billing_address_id());

  // |credit_card3|'s billing address should still be profile 6.
  EXPECT_EQ(profile6.guid(), credit_cards[2]->billing_address_id());
}

// Tests that ApplyDedupingRoutine merges the profile values correctly, i.e.
// never lose information and keep the syntax of the profile with the higher
// frecency score.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_MergedProfileValues) {
  // Create a profile with a higher frecency score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(10);
  profile1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));

  // Create a profile with a medium frecency score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));

  // Create a profile with a lower frecency score.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(5));

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved;
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // |profile1| should have been merged into |profile2| which should then have
  // been merged into |profile3|. Therefore there should only be 1 saved
  // profile.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profiles were removed (profiles 1 and 2).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Since profiles with higher frecency scores are merged into profiles with
  // lower frecency scores, the result of the merge should be contained in
  // profile3 since it had a lower frecency score compared to profile1.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  // The address syntax that results from the merge should be the one from the
  // imported profile (highest frecency).
  EXPECT_EQ(base::UTF8ToUTF16("742. Evergreen Terrace"),
            profiles[0]->GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be full, even if the profile with the higher
  // frecency only had an initial (no loss of information).
  EXPECT_EQ(base::UTF8ToUTF16("Jay"), profiles[0]->GetRawInfo(NAME_MIDDLE));
  // The specified phone number from profile1 should be kept (no loss of
  // information).
  EXPECT_EQ(base::UTF8ToUTF16("12345678910"),
            profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The specified company name from profile2 should be kept (no loss of
  // information).
  EXPECT_EQ(base::UTF8ToUTF16("Fox"), profiles[0]->GetRawInfo(COMPANY_NAME));
  // The specified country from the imported profile shoudl be kept (no loss of
  // information).
  EXPECT_EQ(base::UTF8ToUTF16("US"),
            profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of all the
  // profiles use counts.
  EXPECT_EQ(10U, profiles[0]->use_count());
  // The use date that results from the merge should be the one from the
  // profile1 since it was the most recently used profile.
  EXPECT_LT(profile1.use_date() - base::TimeDelta::FromSeconds(10),
            profiles[0]->use_date());
}

// Tests that ApplyDedupingRoutine only keeps the verified profile with its
// original data when deduping with similar profiles, even if it has a higher
// frecency score.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_VerifiedProfileFirst) {
  // Create a verified profile with a higher frecency score.
  AutofillProfile profile1(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile1, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile1.set_use_count(7);
  profile1.set_use_date(kMuchLaterTime);

  // Create a similar non verified profile with a medium frecency score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile2.set_use_count(5);
  profile2.set_use_date(kSomeLaterTime);

  // Create a similar non verified profile with a lower frecency score.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(kArbitraryTime);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved.
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // |profile2| should have merged with |profile3|. |profile3|
  // should then have been discarded because it is similar to the verified
  // |profile1|.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profile were removed (profiles 2 and 3).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Only the verified |profile1| with its original data should have been kept.
  EXPECT_EQ(profile1.guid(), profiles[0]->guid());
  EXPECT_TRUE(profile1 == *profiles[0]);
  EXPECT_EQ(profile1.use_count(), profiles[0]->use_count());
  EXPECT_EQ(profile1.use_date(), profiles[0]->use_date());
}

// Tests that ApplyDedupingRoutine only keeps the verified profile with its
// original data when deduping with similar profiles, even if it has a lower
// frecency score.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_VerifiedProfileLast) {
  // Create a profile to dedupe with a higher frecency score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(5);
  profile1.set_use_date(kMuchLaterTime);

  // Create a similar non verified profile with a medium frecency score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile2.set_use_count(5);
  profile2.set_use_date(kSomeLaterTime);

  // Create a similar verified profile with a lower frecency score.
  AutofillProfile profile3(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile3.set_use_count(3);
  profile3.set_use_date(kArbitraryTime);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved.
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // |profile1| should have merged with |profile2|. |profile2|
  // should then have been discarded because it is similar to the verified
  // |profile3|.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profile were removed (profiles 1 and 2).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Only the verified |profile3| with it's original data should have been kept.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  EXPECT_TRUE(profile3 == *profiles[0]);
  EXPECT_EQ(profile3.use_count(), profiles[0]->use_count());
  EXPECT_EQ(profile3.use_date(), profiles[0]->use_date());
}

// Tests that ApplyDedupingRoutine does not merge unverified data into
// a verified profile. Also tests that two verified profiles don't get merged.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_MultipleVerifiedProfiles) {
  // Create a profile to dedupe with a higher frecency score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(5);
  profile1.set_use_date(kMuchLaterTime);

  // Create a similar verified profile with a medium frecency score.
  AutofillProfile profile2(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile2.set_use_count(5);
  profile2.set_use_date(kSomeLaterTime);

  // Create a similar verified profile with a lower frecency score.
  AutofillProfile profile3(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile3.set_use_count(3);
  profile3.set_use_date(kArbitraryTime);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved.
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  // Get the profiles, sorted by frecency to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // |profile1| should have been discarded because the saved profile with the
  // highest frecency score is verified (|profile2|). Therefore, |profile1|'s
  // data should not have been merged with |profile2|'s data. Then |profile2|
  // should have been compared to |profile3| but they should not have merged
  // because both profiles are verified.
  ASSERT_EQ(2U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 1 profile was removed (|profile1|).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 1, 1);

  EXPECT_EQ(profile2.guid(), profiles[0]->guid());
  EXPECT_EQ(profile3.guid(), profiles[1]->guid());
  // The profiles should have kept their original data.
  EXPECT_TRUE(profile2 == *profiles[0]);
  EXPECT_TRUE(profile3 == *profiles[1]);
  EXPECT_EQ(profile2.use_count(), profiles[0]->use_count());
  EXPECT_EQ(profile3.use_count(), profiles[1]->use_count());
  EXPECT_EQ(profile2.use_date(), profiles[0]->use_date());
  EXPECT_EQ(profile3.use_date(), profiles[1]->use_date());
}

// Tests that ApplyDedupingRoutine works as expected in a realistic scenario.
// Tests that it merges the diffent set of similar profiles independently and
// that the resulting profiles have the right values, has no effect on the other
// profiles and that the data of verified profiles is not modified.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_MultipleDedupes) {
  // Create a Homer home profile with a higher frecency score than other Homer
  // profiles.
  AutofillProfile Homer1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  Homer1.set_use_count(10);
  Homer1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(1));

  // Create a Homer home profile with a medium frecency score compared to other
  // Homer profiles.
  AutofillProfile Homer2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Homer2.set_use_count(5);
  Homer2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));

  // Create a Homer home profile with a lower frecency score than other Homer
  // profiles.
  AutofillProfile Homer3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  Homer3.set_use_count(3);
  Homer3.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(5));

  // Create a Homer work profile (different address).
  AutofillProfile Homer4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer4, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "12 Nuclear Plant.", "",
                       "Springfield", "IL", "91601", "US", "9876543");
  Homer4.set_use_count(3);
  Homer4.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(5));

  // Create a Marge profile with a lower frecency score that other Marge
  // profiles.
  AutofillProfile Marge1(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&Marge1, "Marjorie", "J", "Simpson",
                       "marge.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Marge1.set_use_count(4);
  Marge1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));

  // Create a verified Marge home profile with a lower frecency score that the
  // other Marge profile.
  AutofillProfile Marge2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Marge2, "Marjorie", "Jacqueline", "Simpson",
                       "marge.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Marge2.set_use_count(2);
  Marge2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));

  // Create a Barney profile (guest user).
  AutofillProfile Barney(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Barney, "Barney", "", "Gumble", "barney.gumble@abc.com",
                       "ABC", "123 Other Street", "", "Springfield", "IL",
                       "91601", "", "");
  Barney.set_use_count(1);
  Barney.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(180));
  Barney.FinalizeAfterImport();

  AddProfileToPersonalDataManager(Homer1);
  AddProfileToPersonalDataManager(Homer2);
  AddProfileToPersonalDataManager(Homer3);
  AddProfileToPersonalDataManager(Homer4);
  AddProfileToPersonalDataManager(Marge1);
  AddProfileToPersonalDataManager(Marge2);
  AddProfileToPersonalDataManager(Barney);

  // Make sure the 7 profiles were saved;
  EXPECT_EQ(7U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  base::HistogramTester histogram_tester;

  // |Homer1| should get merged into |Homer2| which should then be merged into
  // |Homer3|. |Marge2| should be discarded in favor of |Marge1| which is
  // verified. |Homer4| and |Barney| should not be deduped at all.
  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  // Get the profiles, sorted by frecency to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // The 2 duplicates Homer home profiles with the higher frecency and the
  // unverified Marge profile should have been deduped.
  ASSERT_EQ(4U, profiles.size());
  // 7 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 7, 1);
  // 3 profile were removed (|Homer1|, |Homer2| and |Marge2|).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 3, 1);

  // The remaining profiles should be |Homer3|, |Marge1|, |Homer4| and |Barney|
  // in this order of frecency.
  EXPECT_EQ(Homer3.guid(), profiles[0]->guid());
  EXPECT_EQ(Marge1.guid(), profiles[1]->guid());
  EXPECT_EQ(Homer4.guid(), profiles[2]->guid());
  EXPECT_EQ(Barney.guid(), profiles[3]->guid());

  // |Homer3|'s data:
  // The address should be saved with the syntax of |Homer1| since it has the
  // highest frecency score.
  EXPECT_EQ(base::UTF8ToUTF16("742. Evergreen Terrace"),
            profiles[0]->GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be the full version found in |Homer2|,
  EXPECT_EQ(base::UTF8ToUTF16("Jay"), profiles[0]->GetRawInfo(NAME_MIDDLE));
  // The phone number from |Homer2| should be kept (no loss of information).
  EXPECT_EQ(base::UTF8ToUTF16("12345678910"),
            profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The company name from |Homer3| should be kept (no loss of information).
  EXPECT_EQ(base::UTF8ToUTF16("Fox"), profiles[0]->GetRawInfo(COMPANY_NAME));
  // The country from |Homer1| profile should be kept (no loss of information).
  EXPECT_EQ(base::UTF8ToUTF16("US"),
            profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of Homer 1, 2
  // and 3's respective use counts.
  EXPECT_EQ(10U, profiles[0]->use_count());
  // The use date that results from the merge should be the one from the
  // |Homer1| since it was the most recently used profile.
  EXPECT_LT(Homer1.use_date() - base::TimeDelta::FromSeconds(5),
            profiles[0]->use_date());
  EXPECT_GT(Homer1.use_date() + base::TimeDelta::FromSeconds(5),
            profiles[0]->use_date());

  // The other profiles should not have been modified.
  EXPECT_TRUE(Marge1 == *profiles[1]);
  EXPECT_TRUE(Homer4 == *profiles[2]);
  EXPECT_TRUE(Barney == *profiles[3]);
}

// Tests that ApplyDedupingRoutine is not run if the feature is disabled.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_FeatureDisabled) {
  // Create a profile to dedupe.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  // Create a similar profile.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  // Make sure both profiles were saved.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());

  // The deduping routine should not be run.
  EXPECT_FALSE(personal_data_->ApplyDedupingRoutine());

  // Both profiles should still be present.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
}

TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_NopIfZeroProfiles) {
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
  EnableAutofillProfileCleanup();
  EXPECT_FALSE(personal_data_->ApplyDedupingRoutine());
}

TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_NopIfOneProfile) {
  // Create a profile to dedupe.
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  AddProfileToPersonalDataManager(profile);

  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();
  EXPECT_FALSE(personal_data_->ApplyDedupingRoutine());
}

// Tests that ApplyDedupingRoutine is not run a second time on the same major
// version.
TEST_F(PersonalDataManagerTest, ApplyDedupingRoutine_OncePerVersion) {
  // Create a profile to dedupe.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  // Create a similar profile.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_EQ(2U, personal_data_->GetProfiles().size());

  // Enable the profile cleanup now. Otherwise it would be triggered by the
  // calls to AddProfile.
  EnableAutofillProfileCleanup();

  // The deduping routine should be run a first time.
  EXPECT_TRUE(personal_data_->ApplyDedupingRoutine());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // The profiles should have been deduped
  EXPECT_EQ(1U, profiles.size());

  // Add another duplicate profile.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile3);

  // Make sure |profile3| was saved.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());

  // Re-enable the profile cleanup now that the profile was added.
  EnableAutofillProfileCleanup();

  // The deduping routine should not be run.
  EXPECT_FALSE(personal_data_->ApplyDedupingRoutine());

  // The two duplicate profiles should still be present.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
}

// Tests that DeleteDisusedAddresses only deletes the addresses that are
// supposed to be deleted.
TEST_F(PersonalDataManagerTest,
       DeleteDisusedAddresses_DeleteDesiredAddressesOnly) {
  auto now = AutofillClock::Now();

  // Create unverified/disused/not-used-by-valid-credit-card
  // address(deletable).
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Alice", "", "Delete", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 6", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile0.set_use_date(now - base::TimeDelta::FromDays(400));
  AddProfileToPersonalDataManager(profile0);

  // Create unverified/disused/used-by-expired-credit-card address(deletable).
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Bob", "", "Delete", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 7", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile1.set_use_date(now - base::TimeDelta::FromDays(400));
  CreditCard credit_card0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bob",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card0.set_use_date(now - base::TimeDelta::FromDays(400));
  credit_card0.set_billing_address_id(profile1.guid());
  AddProfileToPersonalDataManager(profile1);
  personal_data_->AddCreditCard(credit_card0);
  WaitForOnPersonalDataChanged();
  // Create verified/disused/not-used-by-valid-credit-card address(not
  // deletable).
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Charlie", "", "Keep", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 8", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile2.set_origin(kSettingsOrigin);
  profile2.set_use_date(now - base::TimeDelta::FromDays(400));
  AddProfileToPersonalDataManager(profile2);

  // Create unverified/recently-used/not-used-by-valid-credit-card address(not
  // deletable).
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Dave", "", "Keep", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 9", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile3.set_use_date(now - base::TimeDelta::FromDays(4));
  AddProfileToPersonalDataManager(profile3);

  // Create unverified/disused/used-by-valid-credit-card address(not deletable).
  AutofillProfile profile4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Emma", "", "Keep", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 10", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile4.set_use_date(now - base::TimeDelta::FromDays(400));
  CreditCard credit_card1(CreditCard::MASKED_SERVER_CARD, "c987");
  test::SetCreditCardInfo(&credit_card1, "Emma", "6543", "01", "2999", "1");
  credit_card1.SetNetworkForMaskedCard(kVisaCard);
  credit_card1.set_billing_address_id(profile4.guid());
  credit_card1.set_use_date(now - base::TimeDelta::FromDays(1));
  AddProfileToPersonalDataManager(profile4);
  personal_data_->AddCreditCard(credit_card1);

  WaitForOnPersonalDataChanged();

  EXPECT_EQ(5U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  // DeleteDisusedAddresses should return true.
  EXPECT_TRUE(personal_data_->DeleteDisusedAddresses());
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(3U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(base::UTF8ToUTF16("Keep"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::UTF8ToUTF16("Keep"),
            personal_data_->GetProfiles()[1]->GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::UTF8ToUTF16("Keep"),
            personal_data_->GetProfiles()[2]->GetRawInfo(NAME_LAST));
}

// Tests that DeleteDisusedCreditCards deletes desired credit cards only.
TEST_F(PersonalDataManagerTest,
       DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards) {
  const char kHistogramName[] = "Autofill.CreditCardsDeletedForDisuse";
  auto now = AutofillClock::Now();

  // Create a recently used local card, it is expected to remain.
  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_date(now - base::TimeDelta::FromDays(4));

  // Create a local card that was expired 400 days ago, but recently used.
  // It is expected to remain.
  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Bob",
                          "378282246310006" /* American Express */, "04",
                          "1999", "1");
  credit_card2.set_use_date(now - base::TimeDelta::FromDays(4));

  // Create a local card expired recently, and last used 400 days ago.
  // It is expected to remain.
  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  base::Time expiry_date = now - base::TimeDelta::FromDays(32);
  base::Time::Exploded exploded;
  expiry_date.UTCExplode(&exploded);
  test::SetCreditCardInfo(&credit_card3, "Clyde", "4111111111111111" /* Visa */,
                          base::StringPrintf("%02d", exploded.month).c_str(),
                          base::StringPrintf("%04d", exploded.year).c_str(),
                          "1");
  credit_card3.set_use_date(now - base::TimeDelta::FromDays(400));

  // Create a local card expired 400 days ago, and last used 400 days ago.
  // It is expected to be deleted.
  CreditCard credit_card4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card4, "David",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card4.set_use_date(now - base::TimeDelta::FromDays(400));
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);

  // Create a unmasked server card expired 400 days ago, and last used 400
  // days ago.
  // It is expected to remain because we do not delete server cards.
  CreditCard credit_card5(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card5, "Emma", "4234567890123456" /* Visa */,
                          "04", "1999", "1");
  credit_card5.set_use_date(now - base::TimeDelta::FromDays(400));

  // Create masked server card expired 400 days ago, and last used 400 days ago.
  // It is expected to remain because we do not delete server cards.
  CreditCard credit_card6(CreditCard::MASKED_SERVER_CARD, "c987");
  test::SetCreditCardInfo(&credit_card6, "Frank", "6543", "01", "1998", "1");
  credit_card6.set_use_date(now - base::TimeDelta::FromDays(400));
  credit_card6.SetNetworkForMaskedCard(kVisaCard);

  // Save the server cards and set used_date to desired dates.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(credit_card5);
  server_cards.push_back(credit_card6);
  SetServerCards(server_cards);
  personal_data_->UpdateServerCardMetadata(credit_card5);
  personal_data_->UpdateServerCardMetadata(credit_card6);

  WaitForOnPersonalDataChanged();
  EXPECT_EQ(6U, personal_data_->GetCreditCards().size());

  // Setup histograms capturing.
  base::HistogramTester histogram_tester;

  // DeleteDisusedCreditCards should return true to indicate it was run.
  EXPECT_TRUE(personal_data_->DeleteDisusedCreditCards());

  // Wait for the data to be refreshed.
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());
  std::unordered_set<base::string16> expectedToRemain = {
      base::UTF8ToUTF16("Alice"), base::UTF8ToUTF16("Bob"),
      base::UTF8ToUTF16("Clyde"), base::UTF8ToUTF16("Emma"),
      base::UTF8ToUTF16("Frank")};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }

  // Verify histograms are logged.
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

TEST_F(PersonalDataManagerTest, DeleteLocalCreditCards) {
  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2020", "1");
  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Ben",
                          "378282246310006" /* American Express */, "04",
                          "2021", "1");
  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde",
                          "5105105105105100" /* Mastercard */, "04", "2022",
                          "1");
  std::vector<CreditCard> cards;
  cards.push_back(credit_card1);
  cards.push_back(credit_card2);

  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);

  personal_data_->DeleteLocalCreditCards(cards);

  // Wait for the data to be refreshed.
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  std::unordered_set<base::string16> expectedToRemain = {
      base::UTF8ToUTF16("Clyde")};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }
}

// Tests that a new local profile is created if no existing one is a duplicate
// of the server address. Also tests that the billing address relationship was
// transferred to the converted address.
TEST_F(PersonalDataManagerTest,
       ConvertWalletAddressesAndUpdateWalletCards_NewProfile) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add two different profiles, a local and a server one. Set the use stats so
  // the server profile has a higher frecency, to have a predictable ordering to
  // validate results.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  local_profile.set_use_count(1);
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.

  if (!StructuredNames()) {
    server_profiles.back().SetRawInfo(NAME_FULL,
                                      base::ASCIIToUTF16("John Doe"));
  }
  EXPECT_EQ(server_profiles.back().GetRawInfo(NAME_FULL),
            base::ASCIIToUTF16("John Doe"));
  server_profiles.back().set_use_count(100);
  SetServerProfiles(server_profiles);

  // Add a server and a local card that have the server address as billing
  // address.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  local_card.set_billing_address_id(kServerAddressId);
  personal_data_->AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  server_cards.push_back(
      CreditCard(CreditCard::MASKED_SERVER_CARD, "server_card1"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, personal_data_->GetServerProfiles().size());
  ASSERT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The Wallet address should have been added as a new local profile.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  histogram_tester.ExpectUniqueSample("Autofill.WalletAddressConversionType",
                                      AutofillMetrics::CONVERTED_ADDRESS_ADDED,
                                      1);

  // The conversion should be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Get the profiles, sorted by frecency to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Make sure that the two profiles have not merged.
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(base::UTF8ToUTF16("John"), profiles[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(local_profile, *profiles[1]);

  // Make sure that the billing address id of the two cards now point to the
  // converted profile.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());

  // Make sure that the added address has the email address of the currently
  // signed-in user.
  EXPECT_EQ(base::UTF8ToUTF16(kPrimaryAccountEmail),
            profiles[0]->GetRawInfo(EMAIL_ADDRESS));
}

// Tests that the converted wallet address is merged into an existing local
// profile if they are considered equivalent. Also tests that the billing
// address relationship was transferred to the converted address.
TEST_F(PersonalDataManagerTest,
       ConvertWalletAddressesAndUpdateWalletCards_MergedProfile) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add two similar profile, a local and a server one. Set the use stats so
  // the server card has a higher frecency, to have a predicatble ordering to
  // validate results.
  // Add a local profile.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "John", "", "Doe", "john@doe.com", "",
                       "1212 Center.", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "19482937549");
  local_profile.set_use_count(1);
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "Fox",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, base::ASCIIToUTF16("John Doe"));
  server_profiles.back().set_use_count(100);
  SetServerProfiles(server_profiles);

  // Add a server and a local card that have the server address as billing
  // address.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  local_card.set_billing_address_id(kServerAddressId);
  personal_data_->AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  server_cards.push_back(
      CreditCard(CreditCard::MASKED_SERVER_CARD, "server_card1"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The Wallet address should have been merged with the existing local profile.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  histogram_tester.ExpectUniqueSample("Autofill.WalletAddressConversionType",
                                      AutofillMetrics::CONVERTED_ADDRESS_MERGED,
                                      1);

  // The conversion should be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Get the profiles, sorted by frequency to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Make sure that the two profiles have merged.
  ASSERT_EQ(1U, profiles.size());

  // Check that the values were merged.
  EXPECT_EQ(base::UTF8ToUTF16("john@doe.com"),
            profiles[0]->GetRawInfo(EMAIL_ADDRESS));
  EXPECT_EQ(base::UTF8ToUTF16("Fox"), profiles[0]->GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(base::UTF8ToUTF16("32801"),
            profiles[0]->GetRawInfo(ADDRESS_HOME_ZIP));

  // Make sure that the billing address id of the two cards now point to the
  // converted profile.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());
}

// Tests that a Wallet address that has already converted does not get converted
// a second time.
TEST_F(PersonalDataManagerTest,
       ConvertWalletAddressesAndUpdateWalletCards_AlreadyConverted) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add a server profile that has already been converted.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "Ray", "Doe",
                       "john@doe.com", "Fox", "1212 Center", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "");
  server_profiles.back().set_has_converted(true);
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  SetServerProfiles(server_profiles);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());

  ///////////////////////////////////////////////////////////////////////
  // Tested method.
  ///////////////////////////////////////////////////////////////////////
  ConvertWalletAddressesAndUpdateWalletCards();

  // There should be no local profiles added.
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
}

// Tests that when the user has multiple similar Wallet addresses, they get
// merged into a single local profile, and that the billing address relationship
// is merged too.
TEST_F(
    PersonalDataManagerTest,
    ConvertWalletAddressesAndUpdateWalletCards_MultipleSimilarWalletAddresses) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");
  const std::string kServerAddressId2("server_address2");

  // Add a unique local profile and two similar server profiles. Set the use
  // stats to have a predicatble ordering to validate results.
  // Add a local profile.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Bob", "", "Doe", "", "Fox",
                       "1212 Center.", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "19482937549");
  local_profile.set_use_count(1);
  AddProfileToPersonalDataManager(local_profile);

  // Add a server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  // This step happens automatically for structured names.
  if (!StructuredNames()) {
    server_profiles.back().SetRawInfo(NAME_FULL,
                                      base::ASCIIToUTF16("John Doe"));
  }
  EXPECT_EQ(server_profiles.back().GetRawInfo(NAME_FULL),
            base::ASCIIToUTF16("John Doe"));
  server_profiles.back().set_use_count(100);

  // Add a similar server profile.
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId2));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe",
                       "john@doe.com", "Fox", "1212 Center", "Bld. 5",
                       "Orlando", "FL", "", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, base::ASCIIToUTF16("John Doe"));
  server_profiles.back().set_use_count(200);
  SetServerProfiles(server_profiles);

  // Add a server and a local card that have the first and second Wallet address
  // as a billing address.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  local_card.set_billing_address_id(kServerAddressId);
  personal_data_->AddCreditCard(local_card);
  WaitForOnPersonalDataChanged();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(
      CreditCard(CreditCard::MASKED_SERVER_CARD, "server_card1"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId2);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetServerProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The first Wallet address should have been added as a new local profile and
  // the second one should have merged with the first.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetServerProfiles().size());
  histogram_tester.ExpectBucketCount("Autofill.WalletAddressConversionType",
                                     AutofillMetrics::CONVERTED_ADDRESS_ADDED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.WalletAddressConversionType",
                                     AutofillMetrics::CONVERTED_ADDRESS_MERGED,
                                     1);

  // The conversion should be recorded in the Wallet addresses.
  EXPECT_TRUE(personal_data_->GetServerProfiles()[0]->has_converted());
  EXPECT_TRUE(personal_data_->GetServerProfiles()[1]->has_converted());

  // Get the profiles, sorted by frecency to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Make sure that the two Wallet addresses merged together and were added as
  // a new local profile.
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(base::UTF8ToUTF16("John"), profiles[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(local_profile, *profiles[1]);

  // Check that the values were merged.
  EXPECT_EQ(base::UTF8ToUTF16("Fox"), profiles[0]->GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(base::UTF8ToUTF16("32801"),
            profiles[0]->GetRawInfo(ADDRESS_HOME_ZIP));

  // Make sure that the billing address id of the two cards now point to the
  // converted profile.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());
}

// Tests a new server card's billing address is updated propely even if the
// address was already converted in the past.
TEST_F(
    PersonalDataManagerTest,
    ConvertWalletAddressesAndUpdateWalletCards_NewCrd_AddressAlreadyConverted) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  // Go through the conversion process for a server address and card. Then add
  // a new server card that refers to the already converted server address as
  // its billing address.
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add a server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "Fox",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, base::ASCIIToUTF16("John Doe"));
  server_profiles.back().set_use_count(100);
  SetServerProfiles(server_profiles);

  // Add a server card that have the server address as billing address.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(
      CreditCard(CreditCard::MASKED_SERVER_CARD, "server_card1"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The Wallet address should have been converted to a new local profile.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // The conversion should be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Make sure that the billing address id of the card now point to the
  // converted profile.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());

  // Add a new server card that has the same billing address as the old one.
  server_cards.push_back(
      CreditCard(CreditCard::MASKED_SERVER_CARD, "server_card2"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1112" /* Visa */, "01", "2888", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The conversion should still be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Get the profiles, sorted by frecency to have a deterministic order.
  profiles = personal_data_->GetProfilesToSuggest();

  // Make sure that there is still only one profile.
  ASSERT_EQ(1U, profiles.size());

  // Make sure that the billing address id of the first server card still refers
  // to the converted address.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  // Make sure that the billing address id of the new server card still refers
  // to the converted address.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());
}

// Tests that Wallet addresses do NOT get converted if they're stored in
// ephemeral storage.
TEST_F(PersonalDataManagerTest, DoNotConvertWalletAddressesInEphemeralStorage) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ResetPersonalDataManager(USER_MODE_NORMAL,
                           /*use_sync_transport_mode=*/true);
  ASSERT_FALSE(personal_data_->IsSyncFeatureEnabled());

  // Add a local profile.
  AutofillProfile local_profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz", "",
                       "Fox", "1212 Center.", "Bld. 5", "", "", "", "", "");
  AddProfileToPersonalDataManager(local_profile);

  // Add two server profiles: The first is unique, the second is similar to the
  // local one but has some additional info.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, "server_address1"));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "");
  server_profiles.back().SetRawInfo(NAME_FULL, base::ASCIIToUTF16("John Doe"));

  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, "server_address2"));
  test::SetProfileInfo(&server_profiles.back(), "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  server_profiles.back().SetRawInfo(
      NAME_FULL, base::ASCIIToUTF16("Josephine Alicia Saenz"));
  SetServerProfiles(server_profiles);

  ASSERT_TRUE(AutofillProfileComparator(personal_data_->app_locale())
                  .AreMergeable(local_profile, server_profiles.back()));

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(2U, personal_data_->GetServerProfiles().size());

  ///////////////////////////////////////////////////////////////////////
  // Tested method.
  ///////////////////////////////////////////////////////////////////////
  // Since the wallet addresses are in ephemeral storage, they should *not* get
  // converted to local addresses.
  ConvertWalletAddressesAndUpdateWalletCards();

  ///////////////////////////////////////////////////////////////////////
  // Validation.
  ///////////////////////////////////////////////////////////////////////
  // There should be no changes to the local profiles: No new one added, and no
  // changes to the existing one (even though the second server profile contains
  // additional information and is mergeable in principle).
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(local_profile, *personal_data_->GetProfiles()[0]);
}

TEST_F(PersonalDataManagerTest, RemoveByGUID_ResetsBillingAddress) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  std::vector<CreditCard> server_cards;

  // Add two different profiles
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Bob", "", "Doe", "", "Fox", "1212 Center.",
                       "Bld. 5", "Orlando", "FL", "32801", "US", "19482937549");
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Seb", "", "Doe", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 5", "Springfield", "IL",
                       "32801", "US", "15151231234");

  // Add a local and a server card that have profile0 as their billing address.
  CreditCard local_card0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card0, "John Dillinger",
                          "4111111111111111" /* Visa */, "01", "2999",
                          profile0.guid());
  CreditCard server_card0(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_card0, "John Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", profile0.guid());
  server_cards.push_back(server_card0);

  // Do the same but for profile1.
  CreditCard local_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card1, "Seb Dillinger",
                          "4111111111111111" /* Visa */, "01", "2999",
                          profile1.guid());
  CreditCard server_card1(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_card1, "John Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", profile1.guid());
  server_cards.push_back(server_card1);

  // Add the data to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);
  personal_data_->AddCreditCard(local_card0);
  personal_data_->AddCreditCard(local_card1);
  SetServerCards(server_cards);

  // Verify that the web database has been updated and the notification sent.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Make sure everything was saved properly.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  ///////////////////////////////////////////////////////////////////////
  // Tested method.
  ///////////////////////////////////////////////////////////////////////
  RemoveByGUIDFromPersonalDataManager(profile0.guid());

  ///////////////////////////////////////////////////////////////////////
  // Validation.
  ///////////////////////////////////////////////////////////////////////

  // Wait for the data to be refreshed.
  // WaitForOnPersonalDataChanged();

  // Make sure only profile0 was deleted.
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(profile1.guid(), personal_data_->GetProfiles()[0]->guid());
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  for (CreditCard* card : personal_data_->GetCreditCards()) {
    if (card->guid() == local_card0.guid() ||
        card->guid() == server_card0.guid()) {
      // The billing address id of local_card0 and server_card0 should have been
      // reset.
      EXPECT_EQ("", card->billing_address_id());
    } else {
      // The billing address of local_card1 and server_card1 should still refer
      // to profile1.
      EXPECT_EQ(profile1.guid(), card->billing_address_id());
    }
  }
}

TEST_F(PersonalDataManagerTest, LogStoredProfileMetrics_NoStoredProfiles) {
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
  histogram_tester.ExpectTotalCount("Autofill.StoredProfileCount", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredProfileCount", 0, 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredProfileDisusedCount", 0);
  histogram_tester.ExpectTotalCount("Autofill.DaysSinceLastUse.StoredProfile",
                                    0);
}

TEST_F(PersonalDataManagerTest, LogStoredProfileMetrics) {
  // Add a recently used (3 days ago) profile.
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Bob", "", "Doe", "", "Fox", "1212 Center.",
                       "Bld. 5", "Orlando", "FL", "32801", "US", "19482937549");
  profile0.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(3));
  AddProfileToPersonalDataManager(profile0);

  // Add a profile used a long time (200 days) ago.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Seb", "", "Doe", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 5", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile1.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(200));
  AddProfileToPersonalDataManager(profile1);

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager(USER_MODE_NORMAL);

  EXPECT_EQ(2u, personal_data_->GetProfiles().size());
  histogram_tester.ExpectTotalCount("Autofill.StoredProfileCount", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredProfileCount", 2, 1);

  histogram_tester.ExpectTotalCount("Autofill.StoredProfileDisusedCount", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredProfileDisusedCount", 1,
                                     1);

  histogram_tester.ExpectTotalCount("Autofill.DaysSinceLastUse.StoredProfile",
                                    2);
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.StoredProfile",
                                     3, 1);
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.StoredProfile",
                                     200, 1);
}

TEST_F(PersonalDataManagerTest, LogStoredCreditCardMetrics) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::TimeDelta::FromDays(30);
  base::Time::Exploded now_exploded;
  base::Time::Exploded one_month_ago_exploded;
  now.LocalExplode(&now_exploded);
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<CreditCard> server_cards;
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::LOCAL_CARD, CreditCard::MASKED_SERVER_CARD,
      CreditCard::FULL_SERVER_CARD};
  for (auto record_type : record_types) {
    // Create a card that's still in active use.
    CreditCard card_in_use = test::GetRandomCreditCard(record_type);
    card_in_use.set_use_date(now - base::TimeDelta::FromDays(30));
    card_in_use.set_use_count(10);

    // Create a card that's not in active use.
    CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
    card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
    card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
    card_in_disuse.set_use_date(now - base::TimeDelta::FromDays(200));
    card_in_disuse.set_use_count(10);

    // Add the cards to the personal data manager in the appropriate way.
    if (record_type == CreditCard::LOCAL_CARD) {
      personal_data_->AddCreditCard(card_in_use);
      personal_data_->AddCreditCard(card_in_disuse);
    } else {
      server_cards.push_back(std::move(card_in_use));
      server_cards.push_back(std::move(card_in_disuse));
    }
  }

  SetServerCards(server_cards);

  // SetServerCards modifies the metadata (use_count and use_date)
  // of unmasked cards. Reset the server card metadata to match the data set
  // up above.
  for (const auto& card : server_cards)
    account_autofill_table_->UpdateServerCardMetadata(card);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  ASSERT_EQ(6U, personal_data_->GetCreditCards().size());

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(6U, personal_data_->GetCreditCards().size());

  // Validate the basic count metrics for both local and server cards. Deep
  // validation of the metrics is done in:
  //    AutofillMetricsTest::LogStoredCreditCardMetrics
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 2, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 2, 1);
}

TEST_F(PersonalDataManagerTest, RemoveExpiredCreditCardsNotUsedSinceTimestamp) {
  const char kHistogramName[] = "Autofill.CreditCardsSuppressedForDisuse";
  const base::Time kNow = AutofillClock::Now();
  constexpr size_t kNumCards = 10;

  // We construct a card vector as below, number indicate days of last used
  // from |kNow|:
  // [30, 90, 150, 210, 270, 0, 60, 120, 180, 240]
  // |expires at 2999     |, |expired at 2001   |
  std::vector<CreditCard> all_card_data;
  std::vector<CreditCard*> all_card_ptrs;
  all_card_data.reserve(kNumCards);
  all_card_ptrs.reserve(kNumCards);
  for (size_t i = 0; i < kNumCards; ++i) {
    constexpr base::TimeDelta k30Days = base::TimeDelta::FromDays(30);
    all_card_data.emplace_back(base::GenerateGUID(), "https://example.com");
    if (i < 5) {
      all_card_data.back().set_use_date(kNow - (i + i + 1) * k30Days);
      test::SetCreditCardInfo(&all_card_data.back(), "Clyde Barrow",
                              "378282246310005" /* American Express */, "04",
                              "2999", "1");
    } else {
      all_card_data.back().set_use_date(kNow - (i + i - 10) * k30Days);
      test::SetCreditCardInfo(&all_card_data.back(), "John Dillinger",
                              "4234567890123456" /* Visa */, "04", "2001", "1");
    }
    all_card_ptrs.push_back(&all_card_data.back());
  }

  // Verify that only expired disused card are removed. Note that only the last
  // two cards have use dates more than 175 days ago and are expired.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);

    // The first 8 are either not expired or having use dates more recent
    // than 175 days ago.
    std::vector<CreditCard*> expected_cards(cards.begin(), cards.begin() + 8);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    PersonalDataManager::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow - base::TimeDelta::FromDays(175), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(expected_cards, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  }

  // Reverse the card order and verify that only expired and disused cards
  // are removed. Note that the first three cards, post reversal,
  // have use dates more then 115 days ago.
  {
    // Create a reversed working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs.rbegin(),
                                   all_card_ptrs.rend());

    // The last 7 cards have use dates more recent than 115 days ago.
    std::vector<CreditCard*> expected_cards(cards.begin() + 3, cards.end());

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    PersonalDataManager::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow - base::TimeDelta::FromDays(115), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(expected_cards, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  }
  // Randomize the card order and validate that the filtered list retains
  // that order. Note that the three cards have use dates more then 115
  // days ago and are expired.
  {
    // A handy constant.
    const base::Time k115DaysAgo = kNow - base::TimeDelta::FromDays(115);

    // Created a shuffled master copy of the card pointers.
    std::vector<CreditCard*> shuffled_cards(all_card_ptrs);
    base::RandomShuffle(shuffled_cards.begin(), shuffled_cards.end());

    // Copy the shuffled card pointer collections to use as the working
    // set.
    std::vector<CreditCard*> cards(shuffled_cards);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    PersonalDataManager::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, k115DaysAgo, &cards);

    // Validate that we have the right cards. Iterate of the the shuffled
    // master copy and the filtered copy at the same time. making sure that
    // the elements in the filtered copy occur in the same order as the shuffled
    // master. Along the way, validate that the elements in and out of the
    // filtered copy have appropriate use dates and expiration states.
    EXPECT_EQ(7u, cards.size());
    auto it = shuffled_cards.begin();
    for (const CreditCard* card : cards) {
      for (; it != shuffled_cards.end() && (*it) != card; ++it) {
        EXPECT_LT((*it)->use_date(), k115DaysAgo);
        ASSERT_TRUE((*it)->IsExpired(kNow));
      }
      ASSERT_TRUE(it != shuffled_cards.end());
      ASSERT_TRUE(card->use_date() > k115DaysAgo || !card->IsExpired(kNow));
      ++it;
    }
    for (; it != shuffled_cards.end(); ++it) {
      EXPECT_LT((*it)->use_date(), k115DaysAgo);
      ASSERT_TRUE((*it)->IsExpired(kNow));
    }

    // Validate the histograms.
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  }

  // Verify all cards are retained if they're sufficiently recently
  // used.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    PersonalDataManager::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow - base::TimeDelta::FromDays(720), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(all_card_ptrs, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 0, 1);
  }

  // Verify all cards are removed if they're all disused and expired.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);
    for (auto it = all_card_ptrs.begin(); it < all_card_ptrs.end(); it++) {
      (*it)->SetExpirationYear(2001);
    }

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    PersonalDataManager::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow + base::TimeDelta::FromDays(1), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_TRUE(cards.empty());
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, kNumCards, 1);
  }
}

TEST_F(PersonalDataManagerTest, CreateDataForTest) {
  // Disable sync so the data gets created.
  sync_service_.SetPreferredDataTypes(syncer::ModelTypeSet());
  sync_service_.SetActiveDataTypes(syncer::ModelTypeSet());

  // By default, the creation of test data is disabled.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  ASSERT_EQ(0U, personal_data_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Turn on test data creation for the rest of this scope.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillCreateDataForTest);

  // Reloading the test profile should result in test data being created.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  const std::vector<AutofillProfile*> addresses = personal_data_->GetProfiles();
  const std::vector<CreditCard*> credit_cards =
      personal_data_->GetCreditCards();
  ASSERT_EQ(3U, addresses.size());
  ASSERT_EQ(3U, credit_cards.size());

  const base::Time disused_threshold =
      AutofillClock::Now() - base::TimeDelta::FromDays(180);
  const base::Time deletion_threshold =
      AutofillClock::Now() - base::TimeDelta::FromDays(395);

  // Verify that there was a valid address created.
  {
    auto it = std::find_if(
        addresses.begin(), addresses.end(), [this](const AutofillProfile* p) {
          return p->GetInfo(NAME_FULL, this->personal_data_->app_locale()) ==
                 base::UTF8ToUTF16("John McTester");
        });
    ASSERT_TRUE(it != addresses.end());
    EXPECT_GT((*it)->use_date(), disused_threshold);
  }

  // Verify that there was a disused address created.
  {
    auto it = std::find_if(
        addresses.begin(), addresses.end(), [this](const AutofillProfile* p) {
          return p->GetInfo(NAME_FULL, this->personal_data_->app_locale()) ==
                 base::UTF8ToUTF16("Polly Disused");
        });
    ASSERT_TRUE(it != addresses.end());
    EXPECT_LT((*it)->use_date(), disused_threshold);
  }

  // Verify that there was a disused deletable address created.
  {
    auto it = std::find_if(
        addresses.begin(), addresses.end(), [this](const AutofillProfile* p) {
          return p->GetInfo(NAME_FULL, this->personal_data_->app_locale()) ==
                 base::UTF8ToUTF16("Polly Deletable");
        });
    ASSERT_TRUE(it != addresses.end());
    EXPECT_LT((*it)->use_date(), deletion_threshold);
    EXPECT_FALSE((*it)->IsVerified());
  }

  // Verify that there was a valid credit card created.
  {
    auto it = std::find_if(
        credit_cards.begin(), credit_cards.end(), [this](const CreditCard* cc) {
          return cc->GetInfo(CREDIT_CARD_NAME_FULL,
                             this->personal_data_->app_locale()) ==
                 base::UTF8ToUTF16("Alice Testerson");
        });
    ASSERT_TRUE(it != credit_cards.end());
    EXPECT_GT((*it)->use_date(), disused_threshold);
  }

  // Verify that there was a disused credit card created.
  {
    auto it = std::find_if(
        credit_cards.begin(), credit_cards.end(), [this](const CreditCard* cc) {
          return cc->GetInfo(CREDIT_CARD_NAME_FULL,
                             this->personal_data_->app_locale()) ==
                 base::UTF8ToUTF16("Bob Disused");
        });
    ASSERT_TRUE(it != credit_cards.end());
    EXPECT_LT((*it)->use_date(), disused_threshold);
  }

  // Verify that there was a disused deletable credit card created.
  {
    auto it = std::find_if(
        credit_cards.begin(), credit_cards.end(), [this](const CreditCard* cc) {
          return cc->GetInfo(CREDIT_CARD_NAME_FULL,
                             this->personal_data_->app_locale()) ==
                 base::UTF8ToUTF16("Charlie Deletable");
        });
    ASSERT_TRUE(it != credit_cards.end());
    EXPECT_LT((*it)->use_date(), deletion_threshold);
    EXPECT_TRUE((*it)->IsExpired(deletion_threshold));
  }
}

// These tests are not applicable on Linux since it does not support full server
// cards.
#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
// Test that calling OnSyncServiceInitialized with a null sync service remasks
// full server cards.
TEST_F(PersonalDataManagerTest, OnSyncServiceInitialized_NoSyncService) {
  base::HistogramTester histogram_tester;
  SetUpThreeCardTypes();

  // Call OnSyncServiceInitialized with no sync service.
  personal_data_->OnSyncServiceInitialized(nullptr);
  WaitForOnPersonalDataChanged();

  // Check that cards were masked and other were untouched.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  std::vector<CreditCard*> server_cards =
      personal_data_->GetServerCreditCards();
  EXPECT_EQ(2U, server_cards.size());
  for (CreditCard* card : server_cards)
    EXPECT_TRUE(card->record_type() == CreditCard::MASKED_SERVER_CARD);
}

// Test that calling OnSyncServiceInitialized with a sync service in auth error
// remasks full server cards.
TEST_F(PersonalDataManagerTest, OnSyncServiceInitialized_NotActiveSyncService) {
  base::HistogramTester histogram_tester;
  SetUpThreeCardTypes();

  // Call OnSyncServiceInitialized with a sync service in auth error.
  syncer::TestSyncService sync_service;
  sync_service.SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  personal_data_->OnSyncServiceInitialized(&sync_service);
  WaitForOnPersonalDataChanged();

  // Remove the auth error to be able to get the server cards.
  sync_service.SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));

  // Check that cards were masked and other were untouched.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  std::vector<CreditCard*> server_cards =
      personal_data_->GetServerCreditCards();
  EXPECT_EQ(2U, server_cards.size());
  for (CreditCard* card : server_cards)
    EXPECT_TRUE(card->record_type() == CreditCard::MASKED_SERVER_CARD);

  // Call OnSyncShutdown to ensure removing observer added by
  // OnSyncServiceInitialized.
  personal_data_->OnSyncShutdown(&sync_service);
}
#endif  // !defined(OS_LINUX) || defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
TEST_F(PersonalDataManagerTest, ExcludeServerSideCards) {
  SetUpThreeCardTypes();

  // include_server_cards is set to false, therefore no server cards should be
  // available for suggestion, but that the other calls to get the credit cards
  // are unaffected.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_
                    ->GetCreditCardsToSuggest(/*include_server_cards=*/false)
                    .size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}
#endif  // !defined(OS_ANDROID)

// Sync Transport mode is only for Win, Mac, and Linux.
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
TEST_F(PersonalDataManagerTest, ServerCardsShowInTransportMode) {
  // Set up PersonalDataManager in transport mode.
  ResetPersonalDataManager(USER_MODE_NORMAL,
                           /*use_sync_transport_mode=*/true);
  SetUpThreeCardTypes();
  AccountInfo active_info = SetActiveSecondaryAccount();

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server cards are available for suggestion.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(
      3U, personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true)
              .size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());

  // Stop Wallet sync.
  sync_service_.SetActiveDataTypes(syncer::ModelTypeSet());

  // Check that server cards are unavailable.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(
      1U, personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true)
              .size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}

// Make sure that the opt in is necessary to show server cards if the
// appropriate feature is disabled.
TEST_F(PersonalDataManagerTest, ServerCardsShowInTransportMode_NeedOptIn) {
  // Set up PersonalDataManager in transport mode.
  ResetPersonalDataManager(USER_MODE_NORMAL,
                           /*use_sync_transport_mode=*/true);
  SetUpThreeCardTypes();
  AccountInfo active_info = SetActiveSecondaryAccount();

  // The server cards should not be available at first. The user needs to
  // accept the opt-in offer.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(
      1U, personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true)
              .size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server cards are available for suggestion.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(
      3U, personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true)
              .size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}
#endif  // defined(OS_WIN) || defined(OS_MAC) ||
        // defined(OS_LINUX) || defined(OS_CHROMEOS)

// Tests that all the non settings origins of autofill profiles are cleared but
// that the settings origins are untouched.
TEST_F(PersonalDataManagerTest, ClearProfileNonSettingsOrigins) {
  // Create three profile with a nonsettings, non-empty origin.
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0, "Marion0", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile0.set_use_count(10000);
  AddProfileToPersonalDataManager(profile0);

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_count(1000);
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::GenerateGUID(), "1234");
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_count(100);
  AddProfileToPersonalDataManager(profile2);

  // Create a profile with a settings origin.
  AutofillProfile profile3(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_count(10);
  AddProfileToPersonalDataManager(profile3);

  ASSERT_EQ(4U, personal_data_->GetProfiles().size());

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillRepeatedly(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .Times(2);  // The setting of profiles 0 and 2 will be cleared.

  personal_data_->ClearProfileNonSettingsOrigins();
  run_loop.Run();

  ASSERT_EQ(4U, personal_data_->GetProfiles().size());

  // The first three profiles' origin should be cleared and the fourth one still
  // be the settings origin.
  EXPECT_TRUE(personal_data_->GetProfilesToSuggest()[0]->origin().empty());
  EXPECT_TRUE(personal_data_->GetProfilesToSuggest()[1]->origin().empty());
  EXPECT_TRUE(personal_data_->GetProfilesToSuggest()[2]->origin().empty());
  EXPECT_EQ(kSettingsOrigin,
            personal_data_->GetProfilesToSuggest()[3]->origin());
}

// Tests that all the non settings origins of autofill credit cards are cleared
// but that the settings origins are untouched.
TEST_F(PersonalDataManagerTest, ClearCreditCardNonSettingsOrigins) {
  // Create three cards with a non settings origin.
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card0.set_use_count(10000);
  personal_data_->AddCreditCard(credit_card0);

  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bob1",
                          "5105105105105101" /* Mastercard */, "04", "1999",
                          "1");
  credit_card1.set_use_count(1000);
  personal_data_->AddCreditCard(credit_card1);

  CreditCard credit_card2(base::GenerateGUID(), "1234");
  test::SetCreditCardInfo(&credit_card2, "Bob2",
                          "5105105105105102" /* Mastercard */, "04", "1999",
                          "1");
  credit_card2.set_use_count(100);
  personal_data_->AddCreditCard(credit_card2);

  // Create a card with a settings origin.
  CreditCard credit_card3(base::GenerateGUID(), kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card3, "Bob3",
                          "5105105105105103" /* Mastercard */, "04", "1999",
                          "1");
  credit_card3.set_use_count(10);
  personal_data_->AddCreditCard(credit_card3);

  WaitForOnPersonalDataChanged();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  personal_data_->ClearCreditCardNonSettingsOrigins();

  WaitForOnPersonalDataChanged();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // The first three profiles' origin should be cleared and the fourth one still
  // be the settings origin.
  EXPECT_TRUE(
      personal_data_->GetCreditCardsToSuggest(false)[0]->origin().empty());
  EXPECT_TRUE(
      personal_data_->GetCreditCardsToSuggest(false)[1]->origin().empty());
  EXPECT_TRUE(
      personal_data_->GetCreditCardsToSuggest(false)[2]->origin().empty());
  EXPECT_EQ(kSettingsOrigin,
            personal_data_->GetCreditCardsToSuggest(false)[3]->origin());
}

// Tests that all the non settings origins of autofill profiles are cleared even
// if sync is disabled.
TEST_F(
    PersonalDataManagerTest,
    SyncServiceInitializedWithAutofillDisabled_ClearProfileNonSettingsOrigins) {
  // Create a profile with a non-settings, non-empty origin.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile, "Marion0", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  // Turn off autofill profile sync.
  auto model_type_set = sync_service_.GetActiveDataTypes();
  model_type_set.Remove(syncer::AUTOFILL_PROFILE);
  sync_service_.SetPreferredDataTypes(model_type_set);
  sync_service_.SetActiveDataTypes(model_type_set);

  // The data should still exist.
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());

  // Reload the personal data manager.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // The profile should still exist.
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());

  // The profile's origin should be cleared
  EXPECT_TRUE(personal_data_->GetProfiles()[0]->origin().empty());
}

// Tests that all the non settings origins of autofill credit cards are cleared
// even if sync is disabled.
TEST_F(
    PersonalDataManagerTest,
    SyncServiceInitializedWithAutofillDisabled_ClearCreditCardNonSettingsOrigins) {
  // Create a card with a non-settings, non-empty origin.
  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  personal_data_->AddCreditCard(credit_card);
  WaitForOnPersonalDataChanged();

  // Turn off autofill profile sync.
  auto model_type_set = sync_service_.GetActiveDataTypes();
  model_type_set.Remove(syncer::AUTOFILL_WALLET_DATA);
  model_type_set.Remove(syncer::AUTOFILL_WALLET_METADATA);
  sync_service_.SetPreferredDataTypes(model_type_set);
  sync_service_.SetActiveDataTypes(model_type_set);

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // Reload the personal data manager.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // The card's origin should be cleared
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->origin().empty());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PersonalDataManagerTest, UsePersistentServerStorage) {
  ResetPersonalDataManager(USER_MODE_NORMAL,
                           /*use_sync_transport_mode=*/false);
  SetUpThreeCardTypes();

  // include_server_cards is set to false, therefore no server cards should be
  // available for suggestion, but that the other calls to get the credit cards
  // are unaffected.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_
                    ->GetCreditCardsToSuggest(/*include_server_cards=*/false)
                    .size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}

// Verify that PDM can switch at runtime between the different storages.
TEST_F(PersonalDataManagerTest, SwitchServerStorages) {
  // Start with account storage.
  ResetPersonalDataManager(USER_MODE_NORMAL,
                           /*use_sync_transport_mode=*/true);
  SetUpThreeCardTypes();

  // Check that we do have 2 server cards, as expected.
  ASSERT_EQ(2U, personal_data_->GetServerCreditCards().size());

  // Switch to persistent storage.
  sync_service_.SetIsAuthenticatedAccountPrimary(true);
  personal_data_->OnStateChanged(&sync_service_);
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());

  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCard(server_card);
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch back to the account storage.
  sync_service_.SetIsAuthenticatedAccountPrimary(false);
  personal_data_->OnStateChanged(&sync_service_);
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PersonalDataManagerTest, UseCorrectStorageForDifferentCards) {
  ResetPersonalDataManager(USER_MODE_NORMAL,
                           /*use_sync_transport_mode=*/true);

  // Add a server card.
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCard(server_card);

  // Set server card metadata.
  server_card.set_use_count(15);
  personal_data_->UpdateServerCardMetadata(server_card);

  WaitForOnPersonalDataChanged();

  // Expect that the server card is stored in the account autofill table.
  std::vector<std::unique_ptr<CreditCard>> cards;
  account_autofill_table_->GetServerCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(server_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  local_card.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(5));
  personal_data_->AddCreditCard(local_card);

  WaitForOnPersonalDataChanged();

  // Expect that the local card is stored in the profile autofill table.
  profile_autofill_table_->GetCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(local_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local profile
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  // Expect that a profile is stored in the profile autofill table.
  profile_autofill_table_->GetAutofillProfiles(&profiles);
  EXPECT_EQ(1U, profiles.size());
  EXPECT_EQ(profile, *profiles[0]);
}

// Requests profiles fields validities according to the server: empty profiles,
// non-existent profiles, and normal ones.
TEST_F(PersonalDataManagerTest, RequestProfileServerValidity) {
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ProfileValidityMap profile_validity_map;
  UserProfileValidityMap user_profile_validity_map;
  std::string autofill_profile_validity;

  // Empty validity map.
  ASSERT_TRUE(
      user_profile_validity_map.SerializeToString(&autofill_profile_validity));
  base::Base64Encode(autofill_profile_validity, &autofill_profile_validity);
  personal_data_->pref_service_->SetString(prefs::kAutofillProfileValidity,
                                           autofill_profile_validity);

  std::string guid = "00000000-0000-0000-0000-0000000000001";
  EXPECT_TRUE(personal_data_->GetProfileValidityByGUID(guid)
                  .field_validity_states()
                  .empty());

  // Non-empty validity map.
  std::vector<ServerFieldType> types = {
      ADDRESS_HOME_LINE1, ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY,
      EMAIL_ADDRESS,      ADDRESS_HOME_ZIP,   NAME_FULL};
  std::vector<AutofillDataModel::ValidityState> states = {
      AutofillDataModel::UNSUPPORTED, AutofillDataModel::EMPTY,
      AutofillDataModel::INVALID,     AutofillDataModel::VALID,
      AutofillDataModel::UNVALIDATED, AutofillDataModel::INVALID};
  ASSERT_EQ(types.size(), states.size());
  for (uint64_t i = 0; i < types.size(); ++i) {
    (*profile_validity_map
          .mutable_field_validity_states())[static_cast<int>(types[i])] =
        static_cast<int>(states[i]);
  }
  (*user_profile_validity_map.mutable_profile_validity())[guid] =
      profile_validity_map;
  ASSERT_TRUE(
      user_profile_validity_map.SerializeToString(&autofill_profile_validity));
  base::Base64Encode(autofill_profile_validity, &autofill_profile_validity);
  personal_data_->pref_service_->SetString(prefs::kAutofillProfileValidity,
                                           autofill_profile_validity);

  // Add another non-empty valdity profile.
  guid = "00000000-0000-0000-0000-0000000000002";
  profile_validity_map.Clear();
  autofill_profile_validity.clear();
  (*profile_validity_map
        .mutable_field_validity_states())[static_cast<int>(EMAIL_ADDRESS)] =
      static_cast<int>(AutofillDataModel::VALID);
  (*user_profile_validity_map.mutable_profile_validity())[guid] =
      profile_validity_map;
  ASSERT_TRUE(
      user_profile_validity_map.SerializeToString(&autofill_profile_validity));
  base::Base64Encode(autofill_profile_validity, &autofill_profile_validity);
  personal_data_->pref_service_->SetString(prefs::kAutofillProfileValidity,
                                           autofill_profile_validity);

  // Profile not found.
  guid = "00000000-0000-0000-0000-0000000000003";
  EXPECT_TRUE(personal_data_->GetProfileValidityByGUID(guid)
                  .field_validity_states()
                  .empty());

  // Existing Profiles.
  guid = "00000000-0000-0000-0000-0000000000001";
  auto validities =
      personal_data_->GetProfileValidityByGUID(guid).field_validity_states();
  ASSERT_EQ(validities.size(), types.size());
  for (uint64_t i = 0; i < types.size(); ++i)
    EXPECT_EQ(validities.at(types[i]), states[i]);

  guid = "00000000-0000-0000-0000-0000000000002";
  validities =
      personal_data_->GetProfileValidityByGUID(guid).field_validity_states();
  ASSERT_FALSE(validities.empty());
  EXPECT_EQ(validities.at(EMAIL_ADDRESS), AutofillDataModel::VALID);
}

// Use the client side validation API to validate three PDM profiles. This one
// doesn't test the upload process or saving to the database.
TEST_F(PersonalDataManagerMockTest, UpdateClientValidityStates) {
  AutofillProfile profile_invalid_province(base::GenerateGUID(),
                                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile_invalid_province, "Alice", "", "Munro",
                       "alice@munro.ca", "Fox", "123 Zoo St", "unit 5",
                       "Montreal", "CA", "H3C 2A3", "CA", "15142343254");
  AddProfileToPersonalDataManager(profile_invalid_province);

  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  auto profiles = personal_data_->GetProfiles();
  profiles[0]->set_is_client_validity_states_updated(false);

  UpdateClientValidityStatesOnPersonalDataManager(profiles);

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());

  EXPECT_TRUE(profiles[0]->is_client_validity_states_updated());
  EXPECT_EQ(AutofillDataModel::VALID,
            profiles[0]->GetValidityState(ADDRESS_HOME_COUNTRY,
                                          AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillDataModel::INVALID,
            profiles[0]->GetValidityState(ADDRESS_HOME_STATE,
                                          AutofillProfile::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profiles[0]->GetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profiles[0]->GetValidityState(ADDRESS_HOME_CITY,
                                          AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profiles[0]->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                          AutofillProfile::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profiles[0]->GetValidityState(EMAIL_ADDRESS, AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profiles[0]->GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                          AutofillProfile::CLIENT));
}

// Check the validity update status for AutofillProfiles.
TEST_F(PersonalDataManagerMockTest, UpdateClientValidityStates_UpdatedFlag) {
  // Create two profiles and add them to personal_data_.
  AutofillProfile profile1(test::GetFullValidProfileForCanada());
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(test::GetFullValidProfileForChina());
  AddProfileToPersonalDataManager(profile2);

  ASSERT_EQ(2U, personal_data_->GetProfiles().size());

  // The validities were set when the profiles were added.
  auto profiles = personal_data_->GetProfiles();
  ASSERT_TRUE(profiles[0]->is_client_validity_states_updated());
  ASSERT_TRUE(profiles[1]->is_client_validity_states_updated());

  *profiles[1] = *profiles[0];
  ASSERT_TRUE(profiles[0]->is_client_validity_states_updated());
  ASSERT_TRUE(profiles[1]->is_client_validity_states_updated());

  profiles[1]->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::UTF8ToUTF16(""));
  ASSERT_TRUE(profiles[0]->is_client_validity_states_updated());
  ASSERT_FALSE(profiles[1]->is_client_validity_states_updated());

  profiles[0]->SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Goli Boli"));
  ASSERT_TRUE(profiles[0]->is_client_validity_states_updated());
}

// Check the validity update status for AutofillProfiles.
TEST_F(PersonalDataManagerMockTest,
       UpdateClientValidityStates_UpdatedFlag_Merge) {
  // Set the pref to the current major version.
  StopTheDedupeProcess();

  AutofillProfile profile1(test::GetFullValidProfileForCanada());
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(test::GetFullValidProfileForCanada());
  profile2.set_guid("00000000-0000-0000-0000-000000002019");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::UTF8ToUTF16(""));
  profile2.FinalizeAfterImport();
  AddProfileToPersonalDataManager(profile2);

  // The validities were set when the profiles were added.
  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(2U, profiles.size());

  // In the legacy implementation of names, the merge operation populates the
  // full name field. This results in a change of the name although the names
  // are technically the same. For structured names, this is not true anymore,
  // because a name is always in a finalized state. To enforce a non-noop merge
  // for structured names, we lift the verification status of the full name.
  // TODO(crbug.com/1103421): Make the test logic less implicit once structured
  // names are fully launched and remove feature test.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInNames)) {
    profiles[0]->SetRawInfoWithVerificationStatus(
        NAME_FULL, profiles[1]->GetRawInfo(NAME_FULL),
        structured_address::VerificationStatus::kUserVerified);
  }
  profiles[1]->MergeDataFrom(*profiles[0], "en");
  ASSERT_TRUE(profiles[0]->is_client_validity_states_updated());
  ASSERT_FALSE(profiles[1]->is_client_validity_states_updated());
}

// Check that the validity states are not updated when the validity flags are up
// to date.
TEST_F(PersonalDataManagerMockTest, UpdateClientValidityStates_AlreadyUpdated) {
  // Create two profiles and add them to personal_data_.
  AutofillProfile profile1(test::GetFullValidProfileForCanada());
  profile1.SetRawInfo(EMAIL_ADDRESS, base::UTF8ToUTF16("invalid email!"));
  AddProfileToPersonalDataManager(profile1);

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  // The validities were updated when the profile was added.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profiles[0]->GetValidityState(EMAIL_ADDRESS, AutofillProfile::CLIENT));

  // Change the email, the validity update would turn false.
  profiles[0]->SetRawInfo(EMAIL_ADDRESS, base::UTF8ToUTF16("alice@gmail.com"));
  EXPECT_FALSE(profiles[0]->is_client_validity_states_updated());
  // Pretend that the validity states are updated.
  profiles[0]->set_is_client_validity_states_updated(true);

  // Validating the profiles through the client validation API should not change
  // the validity states.
  personal_data_->UpdateClientValidityStates(profiles);

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profiles[0]->GetValidityState(EMAIL_ADDRESS, AutofillProfile::CLIENT));

  // Try with the flag as not updated.
  profiles[0]->set_is_client_validity_states_updated(false);

  UpdateClientValidityStatesOnPersonalDataManager(profiles);

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profiles[0]->GetValidityState(EMAIL_ADDRESS, AutofillProfile::CLIENT));
}

// Verify that the fields are validated according to the version.
TEST_F(PersonalDataManagerMockTest, UpdateClientValidityStates_Version) {
  // Create two profiles and add them to personal_data_. Set the guids
  // explicitly to preserve the order.
  AutofillProfile profile2(test::GetFullValidProfileForChina());
  profile2.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("invalid state!"));
  profile2.set_guid("00000000-0000-0000-0000-000000000002");
  profile2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromDays(200));
  AddProfileToPersonalDataManager(profile2);

  AutofillProfile profile1(test::GetFullValidProfileForCanada());
  profile1.SetRawInfo(EMAIL_ADDRESS, base::UTF8ToUTF16("invalid email!"));
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_guid("00000000-0000-0000-0000-000000000001");
  AddProfileToPersonalDataManager(profile1);

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(2U, profiles.size());

  EXPECT_TRUE(profiles[0]->is_client_validity_states_updated());
  EXPECT_TRUE(profiles[1]->is_client_validity_states_updated());
  EXPECT_EQ(CHROME_VERSION_MAJOR, GetLastVersionValidatedUpdate());

  // No validation as both validity update flags are true, and the validation
  // version is set to this version.
  base::RunLoop run_loop;
  EXPECT_CALL(*personal_data_, OnValidated(testing::_)).Times(0);
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);
  personal_data_->UpdateClientValidityStates(profiles);

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(2U, profiles.size());
  ResetAutofillLastVersionValidated();

  EXPECT_EQ(0, GetLastVersionValidatedUpdate());
  EXPECT_TRUE(profiles[0]->is_client_validity_states_updated());
  EXPECT_TRUE(profiles[1]->is_client_validity_states_updated());

  // Should validate regardless of the validity update flag, because of the
  // major version update.
  EXPECT_CALL(*personal_data_, OnValidated(testing::_)).Times(2);
  ON_CALL(*personal_data_, OnValidated(testing::_))
      .WillByDefault(testing::Invoke(personal_data_.get(),
                                     &PersonalDataManagerMock::OnValidatedPDM));

  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillRepeatedly(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(2);
  // Validate the profiles through the client validation API.
  personal_data_->UpdateClientValidityStates(profiles);
  run_loop.Run();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(2U, profiles.size());
  ASSERT_EQ(profiles[0]->guid(), profile1.guid());

  // Verify that the version of the last update is set to this version.
  EXPECT_EQ(CHROME_VERSION_MAJOR, GetLastVersionValidatedUpdate());

  EXPECT_EQ(AutofillDataModel::VALID,
            profiles[0]->GetValidityState(ADDRESS_HOME_COUNTRY,
                                          AutofillProfile::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profiles[0]->GetValidityState(EMAIL_ADDRESS, AutofillProfile::CLIENT));

  EXPECT_EQ(AutofillDataModel::VALID,
            profiles[1]->GetValidityState(ADDRESS_HOME_COUNTRY,
                                          AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillDataModel::INVALID,
            profiles[1]->GetValidityState(ADDRESS_HOME_STATE,
                                          AutofillProfile::CLIENT));
}

// Verifies that the profiles are validated when added, updated.
TEST_F(PersonalDataManagerMockTest, UpdateProfilesValidityStates_AddUpdate) {
  // Add
  AutofillProfile profile1(test::GetFullValidProfileForCanada());
  AddProfileToPersonalDataManager(profile1);

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(true, profiles[0]->is_client_validity_states_updated());

  // Update
  profile1.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("email!"));
  UpdateProfileOnPersonalDataManager(profile1);

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(true, profiles[0]->is_client_validity_states_updated());
}

// Verify that slow delayed validation will still work.
TEST_F(PersonalDataManagerMockTest, UpdateClientValidityStates_Delayed) {
  personal_data_->set_client_profile_validator_for_test(
      TestAutofillProfileValidator::GetDelayedInstance());

  AutofillProfile profile(test::GetFullProfile());
  AddProfileToPersonalDataManager(profile);

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());

  profiles[0]->set_is_client_validity_states_updated(false);

  UpdateClientValidityStatesOnPersonalDataManager(profiles);
  profiles[0] =
      nullptr;  // make sure the async task doesn't depend on the pointer.

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_TRUE(profiles[0]->is_client_validity_states_updated());
}

// The validation should not happen when the feature is disabled.
TEST_F(PersonalDataManagerTest, UpdateClientValidityStates_Disabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kAutofillProfileClientValidation);

  AutofillProfile profile1(test::GetFullValidProfileForCanada());
  AddProfileToPersonalDataManager(profile1);

  auto profiles = personal_data_->GetProfiles();
  EXPECT_FALSE(profiles[0]->is_client_validity_states_updated());

  personal_data_->UpdateClientValidityStates(profiles);

  EXPECT_FALSE(profiles[0]->is_client_validity_states_updated());
  EXPECT_EQ(AutofillDataModel::UNVALIDATED,
            profiles[0]->GetValidityState(ADDRESS_HOME_COUNTRY,
                                          AutofillProfile::CLIENT));
}

TEST_F(PersonalDataManagerTest, GetAccountInfoForPaymentsServer) {
  // Make the IdentityManager return a non-empty AccountInfo when
  // GetPrimaryAccountInfo() is called.
  identity_test_env_.SetPrimaryAccount(kPrimaryAccountEmail);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Make the sync service return a non-empty AccountInfo when
  // GetAuthenticatedAccountInfo() is called.
  AccountInfo active_info;
  active_info.email = kSyncTransportAccountEmail;
  sync_service_.SetAuthenticatedAccountInfo(active_info);

  // The Active Sync AccountInfo should be returned.
  EXPECT_EQ(kSyncTransportAccountEmail,
            personal_data_->GetAccountInfoForPaymentsServer().email);

  // The Active Sync AccountInfo should still be returned even if
  // kAutofillEnableAccountWalletStorage is disabled.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndDisableFeature(
        features::kAutofillEnableAccountWalletStorage);

    EXPECT_EQ(kSyncTransportAccountEmail,
              personal_data_->GetAccountInfoForPaymentsServer().email);
  }
}

TEST_F(PersonalDataManagerTest, OnAccountsCookieDeletedByUserAction) {
  // Set up some sync transport opt-ins in the prefs.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId("account1"), true);
  EXPECT_FALSE(
      prefs_->GetDictionary(prefs::kAutofillSyncTransportOptIn)->DictEmpty());

  // Simulate that the cookies get cleared by the user.
  personal_data_->OnAccountsCookieDeletedByUserAction();

  // Make sure the pref is now empty.
  EXPECT_TRUE(
      prefs_->GetDictionary(prefs::kAutofillSyncTransportOptIn)->DictEmpty());
}

// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerMigrationTest,
       MigrateUserOptedInWalletSyncTransportIfNeeded) {
  ASSERT_EQ(
      signin::IdentityManager::MIGRATION_DONE,
      identity_test_env_.identity_manager()->GetAccountIdMigrationState());

  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromEmail(kPrimaryAccountEmail), true);
  ASSERT_TRUE(::autofill::prefs::IsUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromEmail(kPrimaryAccountEmail)));

  ResetPersonalDataManager(USER_MODE_NORMAL);

  EXPECT_FALSE(::autofill::prefs::IsUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromEmail(kPrimaryAccountEmail)));
  EXPECT_TRUE(::autofill::prefs::IsUserOptedInWalletSyncTransport(
      prefs_.get(), sync_service_.GetAuthenticatedAccountInfo().account_id));
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
TEST_F(PersonalDataManagerTest, ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown.
  // Set an an active secondary account.
  AccountInfo active_info;
  active_info.email = kPrimaryAccountEmail;
  active_info.account_id = CoreAccountId("account_id");
  sync_service_.SetAuthenticatedAccountInfo(active_info);
  sync_service_.SetIsAuthenticatedAccountPrimary(false);

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Set the feature to enabled.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAccountWalletStorage},
      /*disabled_features=*/{});

  const std::string kHistogramName =
      "Autofill.HadUserOptedIn_To_WalletSyncTransportServerCards";

  // Make sure the function returns true.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());
    histogram_tester.ExpectUniqueSample(kHistogramName, false, 1);
  }

  // Set that the user already opted-in. Check that the function now returns
  // false.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
    histogram_tester.ExpectUniqueSample(kHistogramName, true, 1);
  }

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, false);
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());
    histogram_tester.ExpectUniqueSample(kHistogramName, false, 1);
  }

  // Set that the user has no server cards. Check that the function now returns
  // false.
  SetServerCards({});
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
    // The metric should not be logged if the user had no server cards.
    histogram_tester.ExpectTotalCount(kHistogramName, 0);
  }

  // Re-set some server cards. Check that the function now returns true.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());
    histogram_tester.ExpectUniqueSample(kHistogramName, false, 1);
  }

  // Set that the user enabled the sync feature. Check that the function now
  // returns false.
  sync_service_.SetIsAuthenticatedAccountPrimary(true);
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
    // The metric should not be logged for syncing users.
    histogram_tester.ExpectTotalCount(kHistogramName, 0);
  }

  // Re-disable the sync feature. Check that the function now returns true.
  sync_service_.SetIsAuthenticatedAccountPrimary(false);
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());
    histogram_tester.ExpectUniqueSample(kHistogramName, false, 1);
  }

  // Set a null sync service. Check that the function now returns false.
  personal_data_->SetSyncServiceForTest(nullptr);
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
    // The metric should not be logged if there is no sync service since this
    // means the user has no server cards.
    histogram_tester.ExpectTotalCount(kHistogramName, 0);
  }
}
#else   // !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
TEST_F(PersonalDataManagerTest, ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown on Desktop.
  // Set an an active secondary account.
  AccountInfo active_info;
  active_info.email = kPrimaryAccountEmail;
  active_info.account_id = CoreAccountId("account_id");
  sync_service_.SetAuthenticatedAccountInfo(active_info);
  sync_service_.SetIsAuthenticatedAccountPrimary(false);

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Set the feature to enabled.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAccountWalletStorage},
      /*disabled_features=*/{});

  // Make sure the function returns false.
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function still returns
  // false.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, false);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function still
  // returns false.
  SetServerCards({});
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function still returns false.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function still
  // returns false.
  sync_service_.SetIsAuthenticatedAccountPrimary(true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function still returns false.
  sync_service_.SetIsAuthenticatedAccountPrimary(false);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function still returns false.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)

TEST_F(PersonalDataManagerTest, GetSyncSigninState) {
  // Make a non-primary account available with both a refresh token and cookie
  // for the first few tests.
  identity_test_env_.SetPrimaryAccount("test@gmail.com");
  sync_service_.SetIsAuthenticatedAccountPrimary(false);
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));

  // Check that the sync state is |SignedInAndWalletSyncTransportEnabled| if the
  // account info is not empty, the kAutofillEnableAccountWalletStorage feature
  // is enabled and the Wallet data type is active for the sync service.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableAccountWalletStorage},
        /*disabled_features=*/{});

    EXPECT_EQ(AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled,
              personal_data_->GetSyncSigninState());
  }

  // Check that the sync state is |SignedIn| if the
  // kAutofillEnableAccountWalletStorage feature is disabled.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillEnableAccountWalletStorage});

    EXPECT_EQ(AutofillSyncSigninState::kSignedIn,
              personal_data_->GetSyncSigninState());
  }

  // Check that the sync state is |SignedIn| if the sync service does not have
  // wallet data active.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableAccountWalletStorage},
        /*disabled_features=*/{});
    sync_service_.SetActiveDataTypes(syncer::ModelTypeSet());

    EXPECT_EQ(AutofillSyncSigninState::kSignedIn,
              personal_data_->GetSyncSigninState());
  }

// ClearPrimaryAccount is not supported on CrOS.
#if !defined(OS_CHROMEOS)
  // Check that the sync state is |SignedOut| when the account info is empty.
  {
    identity_test_env_.ClearPrimaryAccount();
    EXPECT_EQ(AutofillSyncSigninState::kSignedOut,
              personal_data_->GetSyncSigninState());
  }
#endif

  // Simulate that the user has enabled the sync feature.
  AccountInfo primary_account_info;
  primary_account_info.email = kPrimaryAccountEmail;
  sync_service_.SetAuthenticatedAccountInfo(primary_account_info);
  sync_service_.SetIsAuthenticatedAccountPrimary(true);
// MakePrimaryAccountAvailable is not supported on CrOS.
#if !defined(OS_CHROMEOS)
  identity_test_env_.MakePrimaryAccountAvailable(primary_account_info.email);
#endif

  // Check that the sync state is |SignedInAndSyncFeature| if the sync feature
  // is enabled.
  EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
            personal_data_->GetSyncSigninState());

  // Check that the sync state is |SignedInAndSyncFeature| if the the sync
  // feature is enabled even if the kAutofillEnableAccountWalletStorage feature
  // is enabled.
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeature(
        features::kAutofillEnableAccountWalletStorage);
    EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
              personal_data_->GetSyncSigninState());
  }
}

// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, OnUserAcceptedUpstreamOffer) {
  ///////////////////////////////////////////////////////////
  // kSignedInAndWalletSyncTransportEnabled
  ///////////////////////////////////////////////////////////
  // Make a primary account with no sync consent available to be in Sync
  // Transport for Wallet mode.
  CoreAccountInfo active_info =
      identity_test_env_.MakeUnconsentedPrimaryAccountAvailable(
          kSyncTransportAccountEmail);
  sync_service_.SetAuthenticatedAccountInfo(active_info);
  sync_service_.SetIsAuthenticatedAccountPrimary(false);
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  // Make sure there are no opt-ins recorded yet.
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Account wallet storage only makes sense together with support for
  // unconsented primary accounts, i.e. on Win/Mac/Linux.
#if !defined(OS_CHROMEOS)
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeature(
        features::kAutofillEnableAccountWalletStorage);

    EXPECT_EQ(AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled,
              personal_data_->GetSyncSigninState());

    // Make sure an opt-in gets recorded if the user accepted an Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_TRUE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedIn
  ///////////////////////////////////////////////////////////
  {
    // Without AccountWalletStorage, kSignedInAndWalletSyncTransportEnabled
    // shouldn't be available.
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndDisableFeature(
        features::kAutofillEnableAccountWalletStorage);

    EXPECT_EQ(AutofillSyncSigninState::kSignedIn,
              personal_data_->GetSyncSigninState());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedOut
  ///////////////////////////////////////////////////////////
  identity_test_env_.ClearPrimaryAccount();
  {
    EXPECT_EQ(AutofillSyncSigninState::kSignedOut,
              personal_data_->GetSyncSigninState());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
#endif  // !defined(OS_CHROMEOS)

  ///////////////////////////////////////////////////////////
  // kSignedInAndSyncFeature
  ///////////////////////////////////////////////////////////
  identity_test_env_.SetPrimaryAccount(active_info.email);
  sync_service_.SetIsAuthenticatedAccountPrimary(true);
  {
    EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
              personal_data_->GetSyncSigninState());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

namespace {

class OneTimeObserver : public PersonalDataManagerObserver {
 public:
  explicit OneTimeObserver(PersonalDataManager* manager) : manager_(manager) {}

  ~OneTimeObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  void OnPersonalDataChanged() override {
    ASSERT_TRUE(manager_) << "Callback called after RemoveObserver()";
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void OnPersonalDataFinishedProfileTasks() override {
    EXPECT_TRUE(manager_) << "Callback called after RemoveObserver()";
  }

  bool IsConnected() { return manager_; }

 private:
  PersonalDataManager* manager_;
};

}  // namespace

TEST_F(PersonalDataManagerTest, RemoveObserverInOnPersonalDataChanged) {
  OneTimeObserver observer(personal_data_.get());

  personal_data_->AddObserver(&observer);

  // Do something to trigger a data change
  personal_data_->AddProfile(test::GetFullProfile());

  WaitForOnPersonalDataChanged();

  EXPECT_FALSE(observer.IsConnected()) << "Observer not called";
}

TEST_F(PersonalDataManagerTest, AddAndGetUpiId) {
  constexpr char upi_id[] = "vpa@indianbank";
  personal_data_->AddUpiId(upi_id);
  WaitOnceForOnPersonalDataChanged();
  std::vector<std::string> all_upi_ids = personal_data_->GetUpiIds();
  EXPECT_THAT(all_upi_ids, testing::ElementsAre(upi_id));
}

struct ShareNicknameTestParam {
  std::string local_nickname;
  std::string server_nickname;
  std::string expected_nickname;
};

const ShareNicknameTestParam kShareNicknameTestParam[] = {
    {"", "", ""},
    {"", "server nickname", "server nickname"},
    {"local nickname", "", "local nickname"},
    {"local nickname", "server nickname", "local nickname"},
};

class PersonalDataManagerTestForSharingNickname
    : public PersonalDataManagerTest,
      public testing::WithParamInterface<ShareNicknameTestParam> {
 public:
  PersonalDataManagerTestForSharingNickname()
      : local_nickname_(base::UTF8ToUTF16(GetParam().local_nickname)),
        server_nickname_(base::UTF8ToUTF16(GetParam().server_nickname)),
        expected_nickname_(base::UTF8ToUTF16(GetParam().expected_nickname)) {}

  CreditCard GetLocalCard() {
    CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    local_card.set_use_count(3);
    local_card.set_use_date(AutofillClock::Now() -
                            base::TimeDelta::FromDays(1));
    local_card.SetNickname(local_nickname_);
    return local_card;
  }

  CreditCard GetServerCard() {
    CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
    test::SetCreditCardInfo(&full_server_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    full_server_card.SetNickname(server_nickname_);
    return full_server_card;
  }

  base::string16 local_nickname_;
  base::string16 server_nickname_;
  base::string16 expected_nickname_;

 protected:
  void SetUp() override {
    PersonalDataManagerTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableCardNicknameManagement);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PersonalDataManagerTestForSharingNickname,
                         testing::ValuesIn(kShareNicknameTestParam));

TEST_P(PersonalDataManagerTestForSharingNickname,
       VerifySuggestion_DuplicateCards) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data_->AddCreditCard(local_card);

  SetServerCards({GetServerCard()});

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(2U, personal_data_->GetCreditCards().size());

  // Verifies the suggestion shows the right text.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NUMBER),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].value,
            (expected_nickname_.empty() ? base::ASCIIToUTF16("Amex")
                                        : expected_nickname_) +
                base::UTF8ToUTF16("  ") +
                local_card.ObfuscatedLastFourDigits());
}

TEST_P(PersonalDataManagerTestForSharingNickname,
       VerifySuggestion_UnrelatedCards) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data_->AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  CreditCard server_card = GetServerCard();
  // Make sure the cards are different by giving a different card number.
  server_card.SetNumber(base::ASCIIToUTF16("371449635398431"));
  server_cards.emplace_back(server_card);
  SetServerCards(server_cards);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(2U, personal_data_->GetCreditCards().size());

  // Verifies the suggestion shows the right text.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NUMBER),
          /*field_contents=*/base::string16(),
          /*include_server_cards=*/true);
  ASSERT_EQ(2U, suggestions.size());
  EXPECT_THAT(
      std::vector<base::string16>({suggestions[0].value, suggestions[1].value}),
      testing::UnorderedElementsAre(
          (server_nickname_.empty() ? base::ASCIIToUTF16("Amex")
                                    : server_nickname_) +
              base::UTF8ToUTF16("  ") + server_card.ObfuscatedLastFourDigits(),
          (local_nickname_.empty() ? base::ASCIIToUTF16("Amex")
                                   : local_nickname_) +
              base::UTF8ToUTF16("  ") + local_card.ObfuscatedLastFourDigits()));
}

}  // namespace autofill
