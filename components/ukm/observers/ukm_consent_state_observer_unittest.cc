// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include "base/observer_list.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

class MockSyncService : public syncer::TestSyncService {
 public:
  MockSyncService() {
    SetTransportState(TransportState::INITIALIZING);
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  }
  ~MockSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool history_enabled, bool active) {
    SetTransportState(active ? TransportState::ACTIVE
                             : TransportState::INITIALIZING);
    SetIsUsingSecondaryPassphrase(has_passphrase);
    SetPreferredDataTypes(
        history_enabled
            ? syncer::ModelTypeSet(syncer::HISTORY_DELETE_DIRECTIVES)
            : syncer::ModelTypeSet());

    // It doesn't matter what exactly we set here, it's only relevant that the
    // SyncCycleSnapshot is initialized at all.
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot(
        /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 0, 0,
        0, true, 0, base::Time::Now(), base::Time::Now(),
        std::vector<int>(syncer::ModelType::NUM_ENTRIES, 0),
        std::vector<int>(syncer::ModelType::NUM_ENTRIES, 0),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN, base::TimeDelta::FromMinutes(1),
        false));

    NotifyObserversOfStateChanged();
  }

  void SetAuthError(GoogleServiceAuthError::State error_state) {
    syncer::TestSyncService::SetAuthError(GoogleServiceAuthError(error_state));
    NotifyObserversOfStateChanged();
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.OnSyncShutdown(this);
    }
  }

 private:
  // syncer::TestSyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

  // The list of observers of the SyncService state.
  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncService);
};

class TestUkmConsentStateObserver : public UkmConsentStateObserver {
 public:
  TestUkmConsentStateObserver() : purged_(false), notified_(false) {}
  ~TestUkmConsentStateObserver() override {}

  bool ResetPurged() {
    bool was_purged = purged_;
    purged_ = false;
    return was_purged;
  }

  bool ResetNotified() {
    bool notified = notified_;
    notified_ = false;
    return notified;
  }

 private:
  // UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(bool must_purge) override {
    notified_ = true;
    purged_ = purged_ || must_purge;
  }
  bool purged_;
  bool notified_;
  DISALLOW_COPY_AND_ASSIGN(TestUkmConsentStateObserver);
};

class UkmConsentStateObserverTest : public testing::Test {
 public:
  UkmConsentStateObserverTest() {}
  void RegisterUrlKeyedAnonymizedDataCollectionPref(
      sync_preferences::TestingPrefServiceSyncable& prefs) {
    unified_consent::UnifiedConsentService::RegisterPrefs(prefs.registry());
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(PrefService* prefs,
                                                  bool enabled) {
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmConsentStateObserverTest);
};

}  // namespace

TEST_F(UkmConsentStateObserverTest, NoProfiles) {
  TestUkmConsentStateObserver observer;
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, NotActive) {
  MockSyncService sync;
  sync.SetStatus(false, true, false);
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, OneEnabled) {
  MockSyncService sync;
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, MixedProfiles) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, false);
  observer.StartObserving(&sync1, &prefs1);
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, TwoEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, true);
  observer.StartObserving(&sync1, &prefs1);
  EXPECT_TRUE(observer.ResetNotified());
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, OneAddRemove) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;
  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, PurgeOnDisable) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  observer.StartObserving(&sync, &prefs);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, false);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_TRUE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

}  // namespace ukm
