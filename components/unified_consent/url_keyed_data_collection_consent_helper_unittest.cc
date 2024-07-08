// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

#include <vector>

#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {
namespace {

class UrlKeyedDataCollectionConsentHelperTest
    : public testing::Test,
      public UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  // testing::Test:
  void SetUp() override {
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/syncer::UserSelectableTypeSet());
  }

  void OnUrlKeyedDataCollectionConsentStateChanged(
      UrlKeyedDataCollectionConsentHelper* consent_helper) override {
    state_changed_notifications_.push_back(consent_helper->IsEnabled());
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::vector<bool> state_changed_notifications_;
  syncer::TestSyncService sync_service_;
};

TEST_F(UrlKeyedDataCollectionConsentHelperTest, AnonymizedDataCollection) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> helper =
      UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(&pref_service_);
  helper->AddObserver(this);
  EXPECT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kDisabled);
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_TRUE(state_changed_notifications_.empty());

  pref_service_.SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                           true);
  EXPECT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kEnabled);
  EXPECT_TRUE(helper->IsEnabled());
  ASSERT_EQ(1U, state_changed_notifications_.size());
  EXPECT_TRUE(state_changed_notifications_[0]);

  state_changed_notifications_.clear();
  pref_service_.SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                           false);
  EXPECT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kDisabled);
  EXPECT_FALSE(helper->IsEnabled());
  ASSERT_EQ(1U, state_changed_notifications_.size());
  EXPECT_FALSE(state_changed_notifications_[0]);
  helper->RemoveObserver(this);
}

TEST_F(UrlKeyedDataCollectionConsentHelperTest, PersonalizedDataCollection) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> helper =
      UrlKeyedDataCollectionConsentHelper::
          NewPersonalizedDataCollectionConsentHelper(&sync_service_);
  helper->AddObserver(this);
  EXPECT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kDisabled);
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_TRUE(state_changed_notifications_.empty());

  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kHistory});

  sync_service_.FireStateChanged();
  EXPECT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kInitializing);
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_EQ(1U, state_changed_notifications_.size())
      << "No state change notifications fired, because it's still not enabled, "
         "it's just initializing.";

  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service_.FireStateChanged();
  EXPECT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kEnabled);
  EXPECT_TRUE(helper->IsEnabled());
  EXPECT_EQ(2U, state_changed_notifications_.size());
  helper->RemoveObserver(this);
}

TEST_F(UrlKeyedDataCollectionConsentHelperTest,
       PersonalizedDataCollection_NullSyncService) {
    std::unique_ptr<UrlKeyedDataCollectionConsentHelper> helper =
        UrlKeyedDataCollectionConsentHelper::
            NewPersonalizedDataCollectionConsentHelper(
                nullptr /* sync_service */);
    EXPECT_FALSE(helper->IsEnabled());
}

TEST_F(UrlKeyedDataCollectionConsentHelperTest,
       PersonalizedBookmarksDataCollection) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> helper =
      UrlKeyedDataCollectionConsentHelper::
          NewPersonalizedBookmarksDataCollectionConsentHelper(
              &sync_service_,
              /*require_sync_feature_enabled=*/true);
  helper->AddObserver(this);
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_TRUE(state_changed_notifications_.empty());

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  sync_service_.FireStateChanged();
  EXPECT_TRUE(helper->IsEnabled());
  EXPECT_EQ(1U, state_changed_notifications_.size());
  helper->RemoveObserver(this);
}

TEST_F(UrlKeyedDataCollectionConsentHelperTest,
       PersonalizedBookmarksDataCollection_IsSyncFeatureEnabled) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> helper =
      UrlKeyedDataCollectionConsentHelper::
          NewPersonalizedBookmarksDataCollectionConsentHelper(
              &sync_service_,
              /*require_sync_feature_enabled=*/true);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});
  sync_service_.FireStateChanged();
  EXPECT_TRUE(sync_service_.IsSyncFeatureEnabled());
  EXPECT_TRUE(helper->IsEnabled());

  helper->AddObserver(this);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  EXPECT_FALSE(sync_service_.IsSyncFeatureEnabled());
  EXPECT_TRUE(helper->IsEnabled());
  EXPECT_EQ(0U, state_changed_notifications_.size());

  sync_service_.FireStateChanged();
  EXPECT_FALSE(helper->IsEnabled());
  EXPECT_EQ(1U, state_changed_notifications_.size());
  helper->RemoveObserver(this);
}

TEST_F(UrlKeyedDataCollectionConsentHelperTest,
       PersonalizedBookmarksDataCollection_NullSyncService) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> helper =
      UrlKeyedDataCollectionConsentHelper::
          NewPersonalizedBookmarksDataCollectionConsentHelper(
              /*sync_service=*/nullptr,
              /*require_sync_feature_enabled=*/true);
  EXPECT_FALSE(helper->IsEnabled());
}

}  // namespace
}  // namespace unified_consent
