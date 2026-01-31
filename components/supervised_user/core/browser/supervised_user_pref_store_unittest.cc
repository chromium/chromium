// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_pref_store.h"

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/testing_pref_store.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/device_parental_controls_noop_impl.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using ::testing::Eq;
using ::testing::Optional;

class SupervisedUserPrefStoreFixture : public PrefStore::Observer {
 public:
  SupervisedUserPrefStoreFixture(
      supervised_user::FamilyLinkSettingsService* settings_service,
      supervised_user::DeviceParentalControls& device_parental_controls);
  ~SupervisedUserPrefStoreFixture() override;

  base::DictValue* changed_prefs() { return &changed_prefs_; }

  bool initialization_completed() const { return initialization_completed_; }

  // PrefStore::Observer implementation:
  void OnPrefValueChanged(std::string_view key) override;
  void OnInitializationCompleted(bool succeeded) override;

 private:
  scoped_refptr<SupervisedUserPrefStore> pref_store_;
  base::DictValue changed_prefs_;
  bool initialization_completed_;
};

SupervisedUserPrefStoreFixture::SupervisedUserPrefStoreFixture(
    supervised_user::FamilyLinkSettingsService* settings_service,
    supervised_user::DeviceParentalControls& device_parental_controls)
    : pref_store_(new SupervisedUserPrefStore(settings_service,
                                              device_parental_controls)),
      initialization_completed_(pref_store_->IsInitializationComplete()) {
  pref_store_->AddObserver(this);
}

SupervisedUserPrefStoreFixture::~SupervisedUserPrefStoreFixture() {
  pref_store_->RemoveObserver(this);
}

void SupervisedUserPrefStoreFixture::OnPrefValueChanged(std::string_view key) {
  const base::Value* value = nullptr;
  if (pref_store_->GetValue(key, &value)) {
    ASSERT_TRUE(changed_prefs_.SetByDottedPath(key, value->Clone()) != nullptr);
  } else {
    ASSERT_TRUE(changed_prefs_.RemoveByDottedPath(key));
  }
}

void SupervisedUserPrefStoreFixture::OnInitializationCompleted(bool succeeded) {
  EXPECT_FALSE(initialization_completed_);
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(pref_store_->IsInitializationComplete());
  initialization_completed_ = true;
}

}  // namespace

class SupervisedUserPrefStoreTestBase : public ::testing::Test {
 public:
  SupervisedUserPrefStoreTestBase() = default;
  void SetUp() override;
  void TearDown() override;

 protected:
  supervised_user::FamilyLinkSettingsService service_;
  // Backend of the FamilyLinkSettingsService.
  scoped_refptr<TestingPrefStore> service_backing_pref_store_;

#if BUILDFLAG(IS_ANDROID)
  supervised_user::AndroidParentalControls device_parental_controls_;
#else
  supervised_user::DeviceParentalControlsNoOpImpl device_parental_controls_;
#endif  // BUILDFLAG(IS_ANDROID)
};

void SupervisedUserPrefStoreTestBase::SetUp() {
  service_backing_pref_store_ = new TestingPrefStore();
  service_.Init(service_backing_pref_store_);
}

void SupervisedUserPrefStoreTestBase::TearDown() {
  service_.Shutdown();
}

class SupervisedUserPrefStoreTest : public base::test::WithFeatureOverride,
                                    public SupervisedUserPrefStoreTestBase {
 protected:
  SupervisedUserPrefStoreTest()
      : base::test::WithFeatureOverride(
            supervised_user::
                kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs) {}
};

TEST_P(SupervisedUserPrefStoreTest, ConfigureSettings) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  // Prefs should not change yet when the service is ready, but not
  // activated yet.
  service_backing_pref_store_->SetInitializationCompleted();
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

  EXPECT_FALSE(
      fixture.changed_prefs()
          ->FindBoolByDottedPath(policy::policy_prefs::kForceGoogleSafeSearch)
          .has_value());

  // kForceYouTubeRestrict defaults to 'moderate' for supervised users on
  // Android and ChromeOS only.
  EXPECT_FALSE(
      fixture.changed_prefs()
          ->FindIntByDottedPath(policy::policy_prefs::kForceYouTubeRestrict)
          .has_value());

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
  base::DictValue hosts;
  hosts.Set("example.com", true);
  hosts.Set("moose.org", false);
  service_.SetLocalSetting(supervised_user::kContentPackManualBehaviorHosts,
                           hosts.Clone());
  EXPECT_EQ(1u, fixture.changed_prefs()->size());

  base::DictValue* manual_hosts = fixture.changed_prefs()->FindDictByDottedPath(
      prefs::kSupervisedUserManualHosts);
  ASSERT_TRUE(manual_hosts);
  EXPECT_TRUE(*manual_hosts == hosts);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The custodian can allow sites and apps to request permissions.
  // Currently tested indirectly by enabling geolocation requests.
  fixture.changed_prefs()->clear();
  service_.SetLocalSetting(supervised_user::kGeolocationDisabled,
                           base::Value(false));
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  fixture.changed_prefs()->clear();
  service_.SetLocalSetting(supervised_user::kGeolocationDisabled,
                           base::Value(true));
  EXPECT_EQ(1u, fixture.changed_prefs()->size());
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions),
              Optional(false));

  // The custodian allows extension installation without parental approval.
  // TODO(b/321240396): test suitable metrics.
  fixture.changed_prefs()->clear();

  service_.SetLocalSetting(
      supervised_user::kSkipParentApprovalToInstallExtensions,
      base::Value(true));
  EXPECT_EQ(1u, fixture.changed_prefs()->size());
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  prefs::kSkipParentApprovalToInstallExtensions),
              Optional(true));

#endif
}

TEST_P(SupervisedUserPrefStoreTest, IsEmptyAfterDeactivation) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  // Prefs should not change yet when the service is ready, but not
  // activated yet.
  service_backing_pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  service_.SetActive(true);
  EXPECT_NE(0u, fixture.changed_prefs()->size())
      << "Expected default values to be set.";

  service_.SetActive(false);
  EXPECT_EQ(0u, fixture.changed_prefs()->size())
      << "Expected all prefs, including defaults, to be cleared.";
}

TEST_P(SupervisedUserPrefStoreTest, LocalOverridesAreClearedAfterDeactivation) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  // Prefs should not change yet when the service is ready, but not
  // activated yet.
  service_backing_pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  service_.SetActive(true);
  service_.SetLocalSetting(supervised_user::kSafeSitesEnabled,
                           base::Value(true));
  EXPECT_NE(0u, fixture.changed_prefs()->size())
      << "Expected default values to be set.";
  EXPECT_EQ(fixture.changed_prefs()->FindBoolByDottedPath(
                prefs::kSupervisedUserSafeSites),
            base::Value(true));

  service_.SetActive(false);
  EXPECT_EQ(0u, fixture.changed_prefs()->size())
      << "Expected all prefs, including defaults, to be cleared.";
}

TEST_P(SupervisedUserPrefStoreTest, ActivateSettingsBeforeInitialization) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  service_.SetActive(true);
  EXPECT_FALSE(fixture.initialization_completed());
  EXPECT_EQ(0u, fixture.changed_prefs()->size());

  service_backing_pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());

  // This assertion is a bit weak, but here's its sense: the settings service
  // was active before the initialization of pref store was complete. When
  // `SetInitializationCompleted` is called, the settings service first notifies
  // its observers about completed initialization, and then immediately sends
  // notifications about resulting prefs (for an active settings service, that's
  // a non-zero number).
  EXPECT_LT(0u, fixture.changed_prefs()->size());
}

TEST_P(SupervisedUserPrefStoreTest, CreatePrefStoreAfterInitialization) {
  service_backing_pref_store_->SetInitializationCompleted();
  service_.SetActive(true);

  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_TRUE(fixture.initialization_completed());
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(SupervisedUserPrefStoreTest,
       ContentFiltersServiceEnablesBrowserFilters) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  service_backing_pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());

  device_parental_controls_.SetBrowserContentFiltersEnabledForTesting(true);
  EXPECT_THAT(
      fixture.changed_prefs()->FindIntByDottedPath(
          policy::policy_prefs::kIncognitoModeAvailability),
      Optional(static_cast<int>(policy::IncognitoModeAvailability::kDisabled)));
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  prefs::kSupervisedUserSafeSites),
              Optional(true));

  // The other filter is not affecting incognito mode.
  device_parental_controls_.SetSearchContentFiltersEnabledForTesting(false);
  EXPECT_THAT(
      fixture.changed_prefs()->FindIntByDottedPath(
          policy::policy_prefs::kIncognitoModeAvailability),
      Optional(static_cast<int>(policy::IncognitoModeAvailability::kDisabled)));
}

TEST_P(SupervisedUserPrefStoreTest, ContentFiltersServiceEnablesSearchFilters) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  service_backing_pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());

  device_parental_controls_.SetSearchContentFiltersEnabledForTesting(true);

  EXPECT_THAT(
      fixture.changed_prefs()->FindIntByDottedPath(
          policy::policy_prefs::kIncognitoModeAvailability),
      Optional(static_cast<int>(policy::IncognitoModeAvailability::kDisabled)));
  EXPECT_THAT(fixture.changed_prefs()->FindBoolByDottedPath(
                  policy::policy_prefs::kForceGoogleSafeSearch),
              Optional(true));

  // The other filter is not affecting incognito mode.
  device_parental_controls_.SetBrowserContentFiltersEnabledForTesting(false);
  EXPECT_THAT(
      fixture.changed_prefs()->FindIntByDottedPath(
          policy::policy_prefs::kIncognitoModeAvailability),
      Optional(static_cast<int>(policy::IncognitoModeAvailability::kDisabled)));
}

TEST_P(SupervisedUserPrefStoreTest, InactiveSettingsServiceDoesNotAffectPrefs) {
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);
  EXPECT_FALSE(fixture.initialization_completed());

  service_backing_pref_store_->SetInitializationCompleted();
  EXPECT_TRUE(fixture.initialization_completed());

  // After set search is set, one pref is expected to change.
  device_parental_controls_.SetSearchContentFiltersEnabledForTesting(true);
  base::DictValue prefs;
  EXPECT_EQ(2u, fixture.changed_prefs()->size());
  EXPECT_TRUE(fixture.changed_prefs()->FindBoolByDottedPath(
      policy::policy_prefs::kForceGoogleSafeSearch));
  EXPECT_TRUE(fixture.changed_prefs()->FindIntByDottedPath(
      policy::policy_prefs::kIncognitoModeAvailability));

  // service_ is still inactive. On each SetLocalSetting, service_ wants to emit
  // empty dict (empty dict because it's inactive) that would normally clear the
  // prefs, but this happens only once (subsequent attempts to emit empty
  // settings are vetoed). This means that prefs coming from
  // SetSearchFiltersEnabled are maintained.
  service_.SetLocalSetting(supervised_user::kGeolocationDisabled,
                           base::Value(true));
  EXPECT_EQ(2u, fixture.changed_prefs()->size());
  EXPECT_TRUE(fixture.changed_prefs()->FindBoolByDottedPath(
      policy::policy_prefs::kForceGoogleSafeSearch));
  EXPECT_TRUE(fixture.changed_prefs()->FindIntByDottedPath(
      policy::policy_prefs::kIncognitoModeAvailability));
}

// Family Link and Device Parental Controls cooperate to block incognito mode
// and force safe search.
TEST_F(SupervisedUserPrefStoreTestBase, SearchAndIncognitoPrefsAreMerged) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      supervised_user::
          kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs);
  SupervisedUserPrefStoreFixture fixture(&service_, device_parental_controls_);

  service_backing_pref_store_->SetInitializationCompleted();
  service_.SetActive(true);

  ASSERT_TRUE(fixture.initialization_completed());

  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled,
            static_cast<policy::IncognitoModeAvailability>(
                *fixture.changed_prefs()->FindIntByDottedPath(
                    policy::policy_prefs::kIncognitoModeAvailability)));
  EXPECT_FALSE(
      fixture.changed_prefs()
          ->FindIntByDottedPath(policy::policy_prefs::kForceGoogleSafeSearch)
          .has_value());

  // After enabling device parental controls, incognito mode is still disabled
  // but now also safe search is forced (merged with family link settings).
  device_parental_controls_.SetSearchContentFiltersEnabledForTesting(true);
  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled,
            static_cast<policy::IncognitoModeAvailability>(
                *fixture.changed_prefs()->FindIntByDottedPath(
                    policy::policy_prefs::kIncognitoModeAvailability)));
  EXPECT_TRUE(*fixture.changed_prefs()->FindBoolByDottedPath(
      policy::policy_prefs::kForceGoogleSafeSearch));
}
#endif  // BUILDFLAG(IS_ANDROID)

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SupervisedUserPrefStoreTest);

