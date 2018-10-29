// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/abstract_profile_sync_service_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/test_profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/engine/data_type_debug_info_listener.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata_services/web_data_service_test_util.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillChange;
using autofill::AutofillChangeList;
using autofill::AutofillKey;
using autofill::AutofillProfile;
using autofill::AutofillProfileChange;
using autofill::AutofillProfileSyncableService;
using autofill::AutofillTable;
using autofill::AutofillWebDataService;
using autofill::NAME_FULL;
using autofill::PersonalDataManager;
using autofill::ServerFieldType;
using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using syncer::AUTOFILL_PROFILE;
using syncer::BaseNode;
using syncer::syncable::CREATE;
using syncer::syncable::GET_TYPE_ROOT;
using syncer::syncable::MutableEntry;
using syncer::syncable::UNITTEST;
using syncer::syncable::WriterTag;
using syncer::syncable::WriteTransaction;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Not;
using testing::SetArgPointee;
using testing::Return;

namespace browser_sync {

namespace {

void RegisterAutofillPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(autofill::prefs::kAutofillCreditCardEnabled,
                                true);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileEnabled, true);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillWalletImportEnabled,
                                true);
  registry->RegisterIntegerPref(autofill::prefs::kAutofillLastVersionDeduped,
                                atoi(version_info::GetVersionNumber().c_str()));
  registry->RegisterDoublePref(autofill::prefs::kAutofillBillingCustomerNumber,
                               0.0);
  registry->RegisterBooleanPref(
      autofill::prefs::kAutofillJapanCityFieldMigrated, true);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillOrphanRowsRemoved,
                                true);
  registry->RegisterIntegerPref(
      autofill::prefs::kAutofillLastVersionDisusedAddressesDeleted, 0);
  registry->RegisterIntegerPref(
      autofill::prefs::kAutofillLastVersionDisusedCreditCardsDeleted, 0);
  registry->RegisterStringPref(
      autofill::prefs::kAutofillProfileValidity, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

void RunAndSignal(base::OnceClosure cb, WaitableEvent* event) {
  std::move(cb).Run();
  event->Signal();
}

}  // namespace

class AutofillTableMock : public AutofillTable {
 public:
  AutofillTableMock() {}
  MOCK_METHOD2(RemoveFormElement,
               bool(const base::string16& name,
                    const base::string16& value));  // NOLINT
  MOCK_METHOD4(GetAutofillTimestamps,
               bool(const base::string16& name,  // NOLINT
                    const base::string16& value,
                    Time* date_created,
                    Time* date_last_used));
  MOCK_METHOD1(GetAutofillProfiles,
               bool(std::vector<std::unique_ptr<AutofillProfile>>*));  // NOLINT
  MOCK_METHOD1(UpdateAutofillProfile, bool(const AutofillProfile&));   // NOLINT
  MOCK_METHOD1(AddAutofillProfile, bool(const AutofillProfile&));      // NOLINT
  MOCK_METHOD1(RemoveAutofillProfile, bool(const std::string&));       // NOLINT
};

MATCHER_P(MatchProfiles, profile, "") {
  return (profile.Compare(arg) == 0);
}

ACTION_P(LoadAutofillProfiles, datafunc) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles =
      std::move(datafunc());
  arg0->swap(profiles);
}

class WebDatabaseFake : public WebDatabase {
 public:
  explicit WebDatabaseFake(AutofillTable* autofill_table) {
    AddTable(autofill_table);
  }
};

class FakeAutofillBackend : public autofill::AutofillWebDataBackend {
 public:
  FakeAutofillBackend(
      WebDatabase* web_database,
      const base::RepeatingClosure& on_changed,
      const base::RepeatingCallback<void(syncer::ModelType)>& on_sync_started,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner)
      : web_database_(web_database),
        on_changed_(on_changed),
        on_sync_started_(on_sync_started),
        ui_task_runner_(ui_task_runner) {}

  ~FakeAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return web_database_; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBSequence* observer) override {
  }
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBSequence* observer) override {
  }
  void RemoveExpiredFormElements() override {}

  void NotifyOfAutofillProfileChanged(
      const autofill::AutofillProfileChange& change) override {}
  void NotifyOfCreditCardChanged(
      const autofill::CreditCardChange& change) override {}
  void NotifyOfMultipleAutofillChanges() override {
    DCHECK(!ui_task_runner_->RunsTasksInCurrentSequence());
    ui_task_runner_->PostTask(FROM_HERE, on_changed_);
  }
  void NotifyThatSyncHasStarted(syncer::ModelType model_type) override {
    DCHECK(!ui_task_runner_->RunsTasksInCurrentSequence());
    ui_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(on_sync_started_, model_type));
  }

 private:
  WebDatabase* web_database_;
  base::RepeatingClosure on_changed_;
  base::RepeatingCallback<void(syncer::ModelType)> on_sync_started_;
  const scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

class ProfileSyncServiceAutofillTest;

class TokenWebDataServiceFake : public TokenWebData {
 public:
  TokenWebDataServiceFake(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner)
      : TokenWebData(ui_task_runner, db_task_runner) {}

  bool IsDatabaseLoaded() override { return true; }

  AutofillWebDataService::Handle GetAllTokens(
      WebDataServiceConsumer* consumer) override {
    // TODO(tim): It would be nice if WebDataService was injected on
    // construction of ProfileOAuth2TokenService rather than fetched by
    // Initialize so that this isn't necessary (we could pass a null service).
    // We currently do return it via EXPECT_CALLs, but without depending on
    // order-of-initialization (which seems way more fragile) we can't tell
    // which component is asking at what time, and some components in these
    // Autofill tests require a WebDataService.
    return 0;
  }

 private:
  ~TokenWebDataServiceFake() override {}

  DISALLOW_COPY_AND_ASSIGN(TokenWebDataServiceFake);
};

class WebDataServiceFake : public AutofillWebDataService {
 public:
  WebDataServiceFake(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner)
      : AutofillWebDataService(ui_task_runner, db_task_runner),
        web_database_(nullptr),
        autofill_profile_syncable_service_(nullptr),
        syncable_service_created_or_destroyed_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        db_task_runner_(db_task_runner),
        ui_task_runner_(ui_task_runner) {}

  void SetDatabase(WebDatabase* web_database) { web_database_ = web_database; }

  void StartSyncableService() {
    // The |autofill_profile_syncable_service_| must be constructed on the DB
    // sequence.
    const base::RepeatingClosure& on_changed_callback = base::BindRepeating(
        &WebDataServiceFake::NotifyAutofillMultipleChangedOnUISequence,
        AsWeakPtr());
    const base::RepeatingCallback<void(syncer::ModelType)>&
        on_sync_started_callback = base::BindRepeating(
            &WebDataServiceFake::NotifySyncStartedOnUISequence, AsWeakPtr());

    db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebDataServiceFake::CreateSyncableService,
                                  base::Unretained(this), on_changed_callback,
                                  std::move(on_sync_started_callback)));
    syncable_service_created_or_destroyed_.Wait();
  }

  void ShutdownSyncableService() {
    // The |autofill_profile_syncable_service_| must be destructed on the DB
    // sequence.
    db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebDataServiceFake::DestroySyncableService,
                                  base::Unretained(this)));
    syncable_service_created_or_destroyed_.Wait();
  }

  bool IsDatabaseLoaded() override { return true; }

  WebDatabase* GetDatabase() override { return web_database_; }

  void OnAutofillProfileChanged(const AutofillProfileChange& changes) {
    WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED);

    base::OnceClosure notify_cb = base::BindOnce(
        &AutofillProfileSyncableService::AutofillProfileChanged,
        base::Unretained(autofill_profile_syncable_service_), changes);
    db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RunAndSignal, std::move(notify_cb), &event));
    event.Wait();
  }

 private:
  ~WebDataServiceFake() override {}

  void CreateSyncableService(
      const base::RepeatingClosure& on_changed_callback,
      const base::RepeatingCallback<void(syncer::ModelType)>& on_sync_started) {
    ASSERT_TRUE(db_task_runner_->RunsTasksInCurrentSequence());
    // These services are deleted in DestroySyncableService().
    backend_ = std::make_unique<FakeAutofillBackend>(
        GetDatabase(), on_changed_callback, on_sync_started,
        ui_task_runner_.get());
    AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
        this, backend_.get(), "en-US");

    autofill_profile_syncable_service_ =
        AutofillProfileSyncableService::FromWebDataService(this);

    syncable_service_created_or_destroyed_.Signal();
  }

  void DestroySyncableService() {
    ASSERT_TRUE(db_task_runner_->RunsTasksInCurrentSequence());
    autofill_profile_syncable_service_ = nullptr;
    backend_.reset();
    syncable_service_created_or_destroyed_.Signal();
  }

  WebDatabase* web_database_;
  AutofillProfileSyncableService* autofill_profile_syncable_service_;
  std::unique_ptr<autofill::AutofillWebDataBackend> backend_;

  WaitableEvent syncable_service_created_or_destroyed_;

  const scoped_refptr<base::SingleThreadTaskRunner> db_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFake);
};

ACTION_P2(ReturnNewDataTypeManagerWithDebugListener,
          sync_client,
          debug_listener) {
  return std::make_unique<syncer::DataTypeManagerImpl>(
      sync_client, arg0, debug_listener, arg2, arg3, arg4, arg5);
}

class MockPersonalDataManager : public PersonalDataManager {
 public:
  MockPersonalDataManager() : PersonalDataManager("en-US") {}
  MOCK_CONST_METHOD0(IsDataLoaded, bool());
  MOCK_METHOD0(LoadProfiles, void());
  MOCK_METHOD0(LoadCreditCards, void());
  MOCK_METHOD0(LoadPaymentsCustomerData, void());
  MOCK_METHOD0(Refresh, void());
};

class AddAutofillProfileHelper;

class ProfileSyncServiceAutofillTest
    : public AbstractProfileSyncServiceTest,
      public syncer::DataTypeDebugInfoListener {
 public:
  // DataTypeDebugInfoListener implementation.
  void OnDataTypeConfigureComplete(
      const std::vector<syncer::DataTypeConfigurationStats>&
          configuration_stats) override {
    ASSERT_EQ(1u, configuration_stats.size());
    association_stats_ = configuration_stats[0].association_stats;
  }

 protected:
  ProfileSyncServiceAutofillTest() : debug_ptr_factory_(this) {
    autofill::CountryNames::SetLocaleString("en-US");
    RegisterAutofillPrefs(
        profile_sync_service_bundle()->pref_service()->registry());

    data_type_thread()->Start();
    profile_sync_service_bundle()->set_db_thread(
        data_type_thread()->task_runner());

    web_database_ = std::make_unique<WebDatabaseFake>(&autofill_table_);
    web_data_wrapper_ = std::make_unique<MockWebDataServiceWrapper>(
        new WebDataServiceFake(base::ThreadTaskRunnerHandle::Get(),
                               data_type_thread()->task_runner()),
        new TokenWebDataServiceFake(base::ThreadTaskRunnerHandle::Get(),
                                    data_type_thread()->task_runner()));
    web_data_service_ = static_cast<WebDataServiceFake*>(
        web_data_wrapper_->GetProfileAutofillWebData().get());
    web_data_service_->SetDatabase(web_database_.get());

    personal_data_manager_ = std::make_unique<MockPersonalDataManager>();

    EXPECT_CALL(personal_data_manager(), LoadProfiles());
    EXPECT_CALL(personal_data_manager(), LoadCreditCards());
    EXPECT_CALL(personal_data_manager(), LoadPaymentsCustomerData());

    personal_data_manager_->Init(web_data_service_,
                                 /*account_database=*/nullptr,
                                 profile_sync_service_bundle()->pref_service(),
                                 /*identity_manager=*/nullptr,
                                 /*client_profile_validator=*/nullptr,
                                 /*history_service=*/nullptr,
                                 /*is_off_the_record=*/false);

    web_data_service_->StartSyncableService();

    ProfileSyncServiceBundle::SyncClientBuilder builder(
        profile_sync_service_bundle());
    builder.SetPersonalDataManager(personal_data_manager_.get());
    builder.SetSyncServiceCallback(GetSyncServiceCallback());
    builder.SetSyncableServiceCallback(base::BindRepeating(
        &ProfileSyncServiceAutofillTest::GetSyncableServiceForType,
        base::Unretained(this)));
    builder.set_activate_model_creation();
    sync_client_owned_ = builder.Build();
    sync_client_ = sync_client_owned_.get();
  }

  ~ProfileSyncServiceAutofillTest() override {
    web_data_service_->ShutdownOnUISequence();
    web_data_service_->ShutdownSyncableService();
    web_data_wrapper_->Shutdown();
    web_data_service_ = nullptr;
    web_data_wrapper_.reset();
    web_database_.reset();
    // Shut down the service explicitly before some data members from this
    // test it needs will be deleted.
    sync_service()->Shutdown();
  }

  int GetSyncCount() {
    syncer::ReadTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::ReadNode node(&trans);
    if (node.InitTypeRoot(AUTOFILL_PROFILE) != BaseNode::INIT_OK)
      return 0;
    return node.GetTotalNodeCount() - 1;
  }

  void StartAutofillProfileSyncService(base::OnceClosure callback) {
    profile_sync_service_bundle()
        ->identity_test_env()
        ->MakePrimaryAccountAvailable("test_user@gmail.com");
    CreateSyncService(std::move(sync_client_owned_), std::move(callback));

    EXPECT_CALL(*profile_sync_service_bundle()->component_factory(),
                CreateCommonDataTypeControllers(_, _))
        .WillOnce(testing::InvokeWithoutArgs([=]() {
          syncer::DataTypeController::TypeVector controllers;
          controllers.push_back(
              std::make_unique<AutofillProfileDataTypeController>(
                  data_type_thread()->task_runner(), base::DoNothing(),
                  sync_client_, web_data_service_));
          return controllers;
        }));
    EXPECT_CALL(*profile_sync_service_bundle()->component_factory(),
                CreateDataTypeManager(_, _, _, _, _, _))
        .WillOnce(ReturnNewDataTypeManagerWithDebugListener(
            sync_client_,
            syncer::MakeWeakHandle(debug_ptr_factory_.GetWeakPtr())));

    EXPECT_CALL(personal_data_manager(), IsDataLoaded())
        .WillRepeatedly(Return(true));

    sync_service()->Initialize();
    base::RunLoop().Run();

    // It's possible this test triggered an unrecoverable error, in which case
    // we can't get the sync count.
    if (sync_service()->IsSyncFeatureActive()) {
      EXPECT_EQ(GetSyncCount(),
                association_stats_.num_sync_items_after_association);
    }
    EXPECT_EQ(association_stats_.num_sync_items_after_association,
              association_stats_.num_sync_items_before_association +
                  association_stats_.num_sync_items_added -
                  association_stats_.num_sync_items_deleted);
  }

  bool AddAutofillSyncNode(const AutofillProfile& profile) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::WriteNode node(&trans);
    std::string tag = profile.guid();
    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(AUTOFILL_PROFILE, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS)
      return false;

    sync_pb::EntitySpecifics specifics;
    AutofillProfileSyncableService::WriteAutofillProfile(profile, &specifics);
    node.SetEntitySpecifics(specifics);
    return true;
  }

  bool GetAutofillProfilesFromSyncDBUnderProfileNode(
      std::vector<AutofillProfile>* profiles) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service()->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(AUTOFILL_PROFILE) != BaseNode::INIT_OK) {
      return false;
    }

    int64_t child_id = autofill_root.GetFirstChildId();
    while (child_id != syncer::kInvalidId) {
      syncer::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != BaseNode::INIT_OK)
        return false;

      const sync_pb::AutofillProfileSpecifics& autofill(
          child_node.GetEntitySpecifics().autofill_profile());
      AutofillProfile p;
      p.set_guid(autofill.guid());
      AutofillProfileSyncableService::OverwriteProfileWithServerData(autofill,
                                                                     &p);
      profiles->push_back(p);
      child_id = child_node.GetSuccessorId();
    }
    return true;
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL(autofill_table_, RemoveFormElement(_, _)).Times(0);
    EXPECT_CALL(autofill_table_, GetAutofillTimestamps(_, _, _, _)).Times(0);
  }

  AutofillTableMock& autofill_table() { return autofill_table_; }

  MockPersonalDataManager& personal_data_manager() {
    return *personal_data_manager_;
  }

  WebDataServiceFake* web_data_service() { return web_data_service_.get(); }

 private:
  friend class AddAutofillProfileHelper;

  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) {
    DCHECK(type == AUTOFILL_PROFILE);
    return AutofillProfileSyncableService::FromWebDataService(
               web_data_service_.get())
        ->AsWeakPtr();
  }

  AutofillTableMock autofill_table_;
  std::unique_ptr<WebDatabaseFake> web_database_;
  std::unique_ptr<MockWebDataServiceWrapper> web_data_wrapper_;
  scoped_refptr<WebDataServiceFake> web_data_service_;
  std::unique_ptr<MockPersonalDataManager> personal_data_manager_;
  syncer::DataTypeAssociationStats association_stats_;
  base::WeakPtrFactory<DataTypeDebugInfoListener> debug_ptr_factory_;
  // |sync_client_owned_| keeps the created client until it is passed to the
  // created ProfileSyncService. |sync_client_| just keeps a weak reference to
  // the client the whole time.
  std::unique_ptr<syncer::FakeSyncClient> sync_client_owned_;
  syncer::FakeSyncClient* sync_client_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceAutofillTest);
};

class AddAutofillProfileHelper {
 public:
  AddAutofillProfileHelper(ProfileSyncServiceAutofillTest* test,
                           const std::vector<AutofillProfile>& entries)
      : callback_(base::BindOnce(&AddAutofillProfileHelper::AddAutofillCallback,
                                 base::Unretained(this),
                                 test,
                                 entries)),
        success_(false) {}

  base::OnceClosure callback() { return std::move(callback_); }
  bool success() { return success_; }

 private:
  void AddAutofillCallback(ProfileSyncServiceAutofillTest* test,
                           const std::vector<AutofillProfile>& entries) {
    if (!test->CreateRoot(AUTOFILL_PROFILE))
      return;

    for (size_t i = 0; i < entries.size(); ++i) {
      if (!test->AddAutofillSyncNode(entries[i]))
        return;
    }
    success_ = true;
  }

  base::OnceClosure callback_;
  bool success_;
};

TEST_F(ProfileSyncServiceAutofillTest, HasProfileEmptySync) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<AutofillProfile> expected_profiles;
  std::unique_ptr<AutofillProfile> profile0 =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      profile0.get(), "54B3F9AA-335E-4F71-A27D-719C41564230", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  expected_profiles.push_back(*profile0);
  profiles.push_back(std::move(profile0));
  auto profile_returner = [&profiles]() { return std::move(profiles); };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
  EXPECT_CALL(personal_data_manager(), Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL_PROFILE);
  StartAutofillProfileSyncService(create_root.callback());
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(&sync_profiles));
  EXPECT_EQ(1U, sync_profiles.size());
  EXPECT_EQ(0, expected_profiles[0].Compare(sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeProfile) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");

  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Alicia", "Saenz", "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
      "Orlando", "FL", "32801", "US", "19482937549");

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);

  EXPECT_CALL(autofill_table(),
              UpdateAutofillProfile(MatchProfiles(sync_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
}

// Tests that a sync with a new native profile that matches a more recent new
// sync profile but with less information results in the native profile being
// deleted and replaced by the sync profile with merged usage stats.
TEST_F(
    ProfileSyncServiceAutofillTest,
    HasNativeHasSyncMergeSimilarProfileCombine_SyncHasMoreInfoAndMoreRecent) {
  // Create two almost identical profiles. The GUIDs are different and the
  // native profile has no value for company name.
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(4321));

  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(1234));

  AutofillProfile expected_profile(sync_profile);
  expected_profile.SetRawInfo(NAME_FULL,
                              ASCIIToUTF16("Billing Mitchell Morrison"));
  expected_profile.set_use_count(1);

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
  EXPECT_CALL(autofill_table(),
              AddAutofillProfile(MatchProfiles(expected_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_table(),
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF"))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(sync_profile, "en-US"));
  // Make sure the additional information from the sync profile was kept.
  EXPECT_EQ(ASCIIToUTF16("Fox"),
            new_sync_profiles[0].GetRawInfo(ServerFieldType::COMPANY_NAME));
  // Check that the latest use date is saved.
  EXPECT_EQ(base::Time::FromTimeT(4321), new_sync_profiles[0].use_date());
  // Check that the use counts were added (default value is 1).
  EXPECT_EQ(1U, new_sync_profiles[0].use_count());
}

// Tests that a sync with a new native profile that matches an older new sync
// profile but with less information results in the native profile being deleted
// and replaced by the sync profile with merged usage stats.
TEST_F(ProfileSyncServiceAutofillTest,
       HasNativeHasSyncMergeSimilarProfileCombine_SyncHasMoreInfoAndOlder) {
  // Create two almost identical profiles. The GUIDs are different and the
  // native profile has no value for company name.
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(1234));

  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(4321));

  AutofillProfile expected_profile(sync_profile);
  expected_profile.SetRawInfo(NAME_FULL,
                              ASCIIToUTF16("Billing Mitchell Morrison"));
  expected_profile.set_use_count(1);
  expected_profile.set_use_date(native_profile->use_date());

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
  EXPECT_CALL(autofill_table(),
              AddAutofillProfile(MatchProfiles(expected_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_table(),
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF"))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(sync_profile, "en-US"));
  // Make sure the additional information from the sync profile was kept.
  EXPECT_EQ(ASCIIToUTF16("Fox"),
            new_sync_profiles[0].GetRawInfo(ServerFieldType::COMPANY_NAME));
  // Check that the latest use date is saved.
  EXPECT_EQ(base::Time::FromTimeT(4321), new_sync_profiles[0].use_date());
  // Check that the use counts were added (default value is 1).
  EXPECT_EQ(1U, new_sync_profiles[0].use_count());
}

// Tests that a sync with a new native profile that matches an a new sync
// profile but with more information results in the native profile being deleted
// and replaced by the sync profile with the native profiles additional
// information merged in. The merge should happen even if the sync profile is
// more recent.
TEST_F(ProfileSyncServiceAutofillTest,
       HasNativeHasSyncMergeSimilarProfileCombine_NativeHasMoreInfo) {
  // Create two almost identical profiles. The GUIDs are different and the
  // sync profile has no value for company name.
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(4321));

  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(1234));

  AutofillProfile expected_profile(*native_profile);
  expected_profile.SetRawInfo(NAME_FULL,
                              ASCIIToUTF16("Billing Mitchell Morrison"));
  expected_profile.set_use_date(sync_profile.use_date());
  expected_profile.set_use_count(1);

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
  EXPECT_CALL(autofill_table(),
              AddAutofillProfile(MatchProfiles(expected_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_table(),
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF"))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(expected_profile, "en-US"));
  // Make sure the additional information of the native profile was saved into
  // the sync profile.
  EXPECT_EQ(ASCIIToUTF16("Fox"),
            new_sync_profiles[0].GetRawInfo(ServerFieldType::COMPANY_NAME));
  // Check that the latest use date is saved.
  EXPECT_EQ(base::Time::FromTimeT(4321), new_sync_profiles[0].use_date());
  // Check that the use counts were added (default value is 1).
  EXPECT_EQ(1U, new_sync_profiles[0].use_count());
}

// Tests that a sync with a new native profile that differ only by name a new
// sync profile results in keeping both profiles.
TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSync_DifferentPrimaryInfo) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_date(base::Time::FromTimeT(4321));

  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing",
      "John", "Smith", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_date(base::Time::FromTimeT(1234));

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
  EXPECT_CALL(autofill_table(), AddAutofillProfile(MatchProfiles(sync_profile)))
      .WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);

  EXPECT_CALL(personal_data_manager(), Refresh());
  // Adds all entries in |sync_profiles| to sync.
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  // The two profiles should be kept.
  ASSERT_EQ(2U, new_sync_profiles.size());
}

// Tests that a new native profile that is the same as a new sync profile except
// with different GUIDs results in the native profile being deleted and replaced
// by the sync profile.
TEST_F(ProfileSyncServiceAutofillTest, MergeProfileWithDifferentGuid) {
  AutofillProfile sync_profile;

  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
      "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  sync_profile.set_use_count(20);
  sync_profile.set_use_date(base::Time::FromTimeT(1234));

  std::string native_guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), native_guid.c_str(), "Billing", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910");
  native_profile->set_use_count(5);
  native_profile->set_use_date(base::Time::FromTimeT(4321));

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);

  EXPECT_CALL(autofill_table(), AddAutofillProfile(_)).WillOnce(Return(true));
  EXPECT_CALL(autofill_table(), RemoveAutofillProfile(native_guid))
      .WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  // Check that the profiles were merged.
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
  // Check that the sync guid was kept.
  EXPECT_EQ(sync_profile.guid(), new_sync_profiles[0].guid());
  // Check that the sync profile use count was kept.
  EXPECT_EQ(20U, new_sync_profiles[0].use_count());
  // Check that the sync profile use date was kept.
  EXPECT_EQ(base::Time::FromTimeT(1234), new_sync_profiles[0].use_date());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddProfile) {
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager(), Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, AUTOFILL_PROFILE);
  StartAutofillProfileSyncService(create_root.callback());
  ASSERT_TRUE(create_root.success());

  AutofillProfile added_profile;
  autofill::test::SetProfileInfoWithGuid(
      &added_profile, "D6ADA912-D374-4C0A-917D-F5C8EBE43011", "Josephine",
      "Alicia", "Saenz", "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
      "Orlando", "FL", "32801", "US", "19482937549");

  AutofillProfileChange change(AutofillProfileChange::ADD, added_profile.guid(),
                               &added_profile);
  web_data_service()->OnAutofillProfileChanged(change);

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, added_profile.Compare(new_sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveProfile) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(
      &sync_profile, "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine",
      "Alicia", "Saenz", "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
      "Orlando", "FL", "32801", "US", "19482937549");
  std::unique_ptr<AutofillProfile> native_profile =
      std::make_unique<AutofillProfile>();
  autofill::test::SetProfileInfoWithGuid(
      native_profile.get(), "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine",
      "Alicia", "Saenz", "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
      "Orlando", "FL", "32801", "US", "19482937549");

  std::vector<std::unique_ptr<AutofillProfile>> native_profiles;
  native_profiles.push_back(std::move(native_profile));
  auto profile_returner = [&native_profiles]() {
    return std::move(native_profiles);
  };
  EXPECT_CALL(autofill_table(), GetAutofillProfiles(_))
      .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillProfileHelper add_autofill(this, sync_profiles);
  EXPECT_CALL(personal_data_manager(), Refresh());
  StartAutofillProfileSyncService(add_autofill.callback());
  ASSERT_TRUE(add_autofill.success());

  AutofillProfileChange change(AutofillProfileChange::REMOVE,
                               sync_profile.guid(), nullptr);
  web_data_service()->OnAutofillProfileChanged(change);

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(
      GetAutofillProfilesFromSyncDBUnderProfileNode(&new_sync_profiles));
  ASSERT_EQ(0U, new_sync_profiles.size());
}

}  // namespace browser_sync
