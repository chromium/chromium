// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/testing_pref_store.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/supervised_user/core/browser/supervised_user_pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Optional;

class SupervisedUserPrefStoreFixture : public PrefStore::Observer {
 public:
  explicit SupervisedUserPrefStoreFixture(
      supervised_user::SupervisedUserSettingsService* settings_service);
  ~SupervisedUserPrefStoreFixture() override;

  base::Value::Dict* changed_prefs() { return &changed_prefs_; }

  bool initialization_completed() const { return initialization_completed_; }

  // PrefStore::Observer implementation:
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

 private:
  scoped_refptr<SupervisedUserPrefStore> pref_store_;
  base::Value::Dict changed_prefs_;
  bool initialization_completed_;
};

SupervisedUserPrefStoreFixture::SupervisedUserPrefStoreFixture(
    supervised_user::SupervisedUserSettingsService* settings_service)
    : pref_store_(new SupervisedUserPrefStore(settings_service)),
      initialization_completed_(pref_store_->IsInitializationComplete()) {
  pref_store_->AddObserver(this);
}

SupervisedUserPrefStoreFixture::~SupervisedUserPrefStoreFixture() {
  pref_store_->RemoveObserver(this);
}

void SupervisedUserPrefStoreFixture::OnPrefValueChanged(
    const std::string& key) {
  const base::Value* value = nullptr;
  ASSERT_TRUE(pref_store_->GetValue(key, &value));
  changed_prefs_.SetByDottedPath(key, value->Clone());
}

void SupervisedUserPrefStoreFixture::OnInitializationCompleted(bool succeeded) {
  EXPECT_FALSE(initialization_completed_);
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(pref_store_->IsInitializationComplete());
  initialization_completed_ = true;
}

}  // namespace

class SupervisedUserPrefStoreTest : public ::testing::Test {
 public:
  SupervisedUserPrefStoreTest() {}
  void SetUp() override;
  void TearDown() override;

 protected:
  supervised_user::SupervisedUserSettingsService service_;
  scoped_refptr<TestingPrefStore> pref_store_;
};

void SupervisedUserPrefStoreTest::SetUp() {
  pref_store_ = new TestingPrefStore();
  service_.Init(pref_store_);
}

void SupervisedUserPrefStoreTest::TearDown() {
  service_.Shutdown();
}

TEST_F(SupervisedUserPrefStoreTest, ConfigureSettings) {
  SupervisedUserPrefStoreFixture fixture(&service_);
  EXPECT_FALSE(fixture.initialization_completed());

  // Prefs should not change yet when the service is ready, but not
  // activated yet.
  pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  service_.SetActive(true);

  // kIncognitoModeAvailability must be disabled for all supervised users.
  EXPECT_THAT(
      fixture.changed_prefs()->FindIntByDottedPath(
          policy::policy_prefs::kIncognitoModeAvailability),
      Optional(static_cast<int>(policy::IncognitoModeAvailability::kDisabled)));

  // kSupervisedModeManualHosts does not have a hardcoded value.
  EXPECT_FALSE(fixture.changed_prefs()->FindDictByDottedPath(
      prefs::kSupervisedUserManualHosts));

  // kForceGoogleSafeSearch defaults to true if the relevant feature flag is
  // enabled.
  if (base::FeatureList::IsEnabled(
          supervised_user::kForceGoogleSafeSearchForSupervisedUsers)) {
    EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                    policy::policy_prefs::kForceGoogleSafeSearch),
                Optional(true));
  } else {
    EXPECT_FALSE(
        fixture.changed_prefs()
            ->FindBoolByDottedPath(policy::policy_prefs::kForceGoogleSafeSearch)
            .has_value());
  }

  // kForceYouTubeRestrict defaults to 'moderate' for supervised users on
  // Android and ChromeOS only.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  int force_youtube_restrict =
      fixture.changed_prefs()
          ->FindIntByDottedPath(policy::policy_prefs::kForceYouTubeRestrict)
          .value_or(safe_search_api::YOUTUBE_RESTRICT_OFF);
  EXPECT_EQ(force_youtube_restrict, safe_search_api::YOUTUBE_RESTRICT_MODERATE);
#else
  EXPECT_FALSE(
      fixture.changed_prefs()
          ->FindIntByDottedPath(policy::policy_prefs::kForceYouTubeRestrict)
          .has_value());
#endif

#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  syncer::prefs::internal::kSyncPayments),
              false);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Permissions requests default to allowed, to match server-side behavior.
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions),
              Optional(true));
#endif

  // Activating the service again should not change anything.
  fixture.changed_prefs()->clear();
  service_.SetActive(true);
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  // kSupervisedModeManualHosts can be configured by the custodian.
  base::Value::Dict hosts;
  hosts.Set("example.com", true);
  hosts.Set("moose.org", false);
  service_.SetLocalSetting(supervised_user::kContentPackManualBehaviorHosts,
                           hosts.Clone());
  EXPECT_EQ(1u, fixture.changed_prefs()->size());

  base::Value::Dict* manual_hosts =
      fixture.changed_prefs()->FindDictByDottedPath(
          prefs::kSupervisedUserManualHosts);
  ASSERT_TRUE(manual_hosts);
  EXPECT_TRUE(*manual_hosts == hosts);

  // kForceGoogleSafeSearch can be configured by the custodian, overriding the
  // hardcoded default.
  fixture.changed_prefs()->clear();
  service_.SetLocalSetting(supervised_user::kForceSafeSearch,
                           base::Value(false));
  EXPECT_EQ(1u, fixture.changed_prefs()->size());
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  policy::policy_prefs::kForceGoogleSafeSearch),
              Optional(false));

  // kForceYouTubeRestrict can be configured by the custodian on Android and
  // ChromeOS only.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  force_youtube_restrict =
      fixture.changed_prefs()
          ->FindIntByDottedPath(policy::policy_prefs::kForceYouTubeRestrict)
          .value_or(safe_search_api::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_EQ(force_youtube_restrict, safe_search_api::YOUTUBE_RESTRICT_OFF);
#else
  EXPECT_FALSE(
      fixture.changed_prefs()
          ->FindIntByDottedPath(policy::policy_prefs::kForceYouTubeRestrict)
          .has_value());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The custodian can allow sites and apps to request permissions.
  // Currently tested indirectly by enabling geolocation requests.
  // TODO(crbug/1024646): Update Kids Management server to set a new bit for
  // extension permissions and update this test.

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", 0);

  fixture.changed_prefs()->clear();
  service_.SetLocalSetting(supervised_user::kGeolocationDisabled,
                           base::Value(false));
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  histogram_tester.ExpectUniqueSample(
      "SupervisedUsers.ExtensionsMayRequestPermissions", /*enabled=*/true, 1);
  histogram_tester.ExpectTotalCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", 1);

  fixture.changed_prefs()->clear();
  service_.SetLocalSetting(supervised_user::kGeolocationDisabled,
                           base::Value(true));
  EXPECT_EQ(1u, fixture.changed_prefs()->size());
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions),
              Optional(false));

  histogram_tester.ExpectBucketCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", /*enabled=*/false, 1);
  histogram_tester.ExpectTotalCount(
      "SupervisedUsers.ExtensionsMayRequestPermissions", 2);

#endif
}

TEST_F(SupervisedUserPrefStoreTest, ActivateSettingsBeforeInitialization) {
  SupervisedUserPrefStoreFixture fixture(&service_);
  EXPECT_FALSE(fixture.initialization_completed());

  service_.SetActive(true);
  EXPECT_FALSE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());
}

TEST_F(SupervisedUserPrefStoreTest, CreatePrefStoreAfterInitialization) {
  pref_store_->SetInitializationCompleted();
  service_.SetActive(true);

  SupervisedUserPrefStoreFixture fixture(&service_);
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());
}
